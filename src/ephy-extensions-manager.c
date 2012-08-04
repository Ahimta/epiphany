/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *  Copyright © 2004 Adam Hooper
 *  Copyright © 2005 Crispin Flowerday
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ephy-extensions-manager.h"

#include "ephy-debug.h"
#include "ephy-embed-container.h"
#include "ephy-file-helpers.h"
#include "ephy-loader.h"
#include "ephy-object-helpers.h"
#include "ephy-prefs.h"
#include "ephy-settings.h"
#include "ephy-shell.h"
#include "ephy-shlib-loader.h"

#include <gio/gio.h>
#include <gmodule.h>
#include <string.h>

#define EE_GROUP		"Epiphany Extension"
#define DOT_INI			".ephy-extension"
#define RELOAD_DELAY		333 /* ms */
#define RELOAD_SYNC_DELAY	1 /* seconds */

#define EPHY_EXTENSIONS_MANAGER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_EXTENSIONS_MANAGER, EphyExtensionsManagerPrivate))

struct _EphyExtensionsManagerPrivate
{
	gboolean initialised;

	GList *data;
	GList *factories;
	GList *extensions;
	GList *dir_monitors;
	GList *windows;
	guint sync_timeout_id;
	GHashTable *reload_hash;
};

typedef struct
{
	EphyExtensionInfo info;
	gboolean load_failed;

	char *loader_type;

	EphyLoader *loader; /* NULL if never loaded */
	GObject *extension; /* NULL if unloaded */
} ExtensionInfo;

typedef struct
{
	char *type;
	EphyLoader *loader;
} LoaderInfo;

typedef enum
{
	FORMAT_UNKNOWN,
	FORMAT_INI
} ExtensionFormat;

enum
{
	CHANGED,
	ADDED,
	REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void ephy_extensions_manager_class_init	(EphyExtensionsManagerClass *klass);
static void ephy_extensions_manager_iface_init	(EphyExtensionIface *iface);
static void ephy_extensions_manager_init	(EphyExtensionsManager *manager);

G_DEFINE_TYPE_WITH_CODE (EphyExtensionsManager, ephy_extensions_manager, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (EPHY_TYPE_EXTENSION,
						ephy_extensions_manager_iface_init))

static void
ephy_extensions_manager_toggle_load (EphyExtensionsManager *manager,
				     const char *identifier,
				     gboolean status)
{
	char **exts;
	GVariantBuilder builder;
	int i;
	gboolean found = FALSE;

	g_return_if_fail (EPHY_IS_EXTENSIONS_MANAGER (manager));
	g_return_if_fail (identifier != NULL);

	if (status)
		LOG ("Adding '%s' to extensions", identifier);
	else
		LOG ("Removing '%s' from extensions", identifier);

	exts = g_settings_get_strv (EPHY_SETTINGS_MAIN,
				    EPHY_PREFS_ENABLED_EXTENSIONS);

	g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);
	for (i = 0; exts[i]; i++)
	{
		/* Ignore the extension if we are removing it. */
		if (g_strcmp0 (exts[i], identifier) == 0)
		{
			found = TRUE;
			if (!status)
				continue;
		}

		g_variant_builder_add (&builder, "s", exts[i]);
	}

	if (status && !found)
		g_variant_builder_add (&builder, "s", identifier);

	g_settings_set (EPHY_SETTINGS_MAIN,
			EPHY_PREFS_ENABLED_EXTENSIONS,
			"as", &builder);
}

/**
 * ephy_extensions_manager_load:
 * @manager: an #EphyExtensionsManager
 * @identifier: identifier of the extension to load
 *
 * Loads the extension corresponding to @identifier.
 **/
void
ephy_extensions_manager_load (EphyExtensionsManager *manager,
			      const char *identifier)
{
	ephy_extensions_manager_toggle_load (manager, identifier, TRUE);
}

/**
 * ephy_extensions_manager_unload:
 * @manager: an #EphyExtensionsManager
 * @identifier: filename of extension to unload, minus "lib" and "extension.so"
 *
 * Unloads the extension specified by @identifier.
 *
 * The extension with the same filename can afterwards be reloaded. However,
 * if any GTypes within the extension have changed parent types, Epiphany must
 * be restarted.
 **/
void
ephy_extensions_manager_unload (EphyExtensionsManager *manager,
				const char *identifier)
{
	ephy_extensions_manager_toggle_load (manager, identifier, FALSE);
}

/**
 * ephy_extensions_manager_register:
 * @manager: an #EphyExtensionsManager
 * @object: an Extension
 *
 * Registers @object with the extensions manager. @object must implement the
 * #EphyExtension interface.
 **/
void
ephy_extensions_manager_register (EphyExtensionsManager *manager,
				  GObject *object)
{
	g_return_if_fail (EPHY_IS_EXTENSIONS_MANAGER (manager));
	g_return_if_fail (EPHY_IS_EXTENSION (object));

	manager->priv->extensions = g_list_prepend (manager->priv->extensions,
						    g_object_ref (object));
}


/**
 * ephy_extensions_manager_get_extensions:
 * @manager: an #EphyExtensionsManager
 *
 * Returns the list of known extensions.
 *
 * Returns: (element-type EphyEmbed) (transfer container): a list of
 *          #EphyExtensionInfo
 **/
GList *
ephy_extensions_manager_get_extensions (EphyExtensionsManager *manager)
{
	return g_list_copy (manager->priv->data);
}

static void
free_extension_info (ExtensionInfo *info)
{
	EphyExtensionInfo *einfo = (EphyExtensionInfo *) info;

	g_free (einfo->identifier);
	g_key_file_free (einfo->keyfile);
	g_free (info->loader_type);

	if (info->extension != NULL)
	{
		g_return_if_fail (info->loader != NULL);

		ephy_loader_release_object (info->loader, info->extension);
	}
	if (info->loader != NULL)
	{
		g_object_unref (info->loader);
	}

	g_free (info);
}

static void
free_loader_info (LoaderInfo *info)
{
	g_free (info->type);
	g_object_unref (info->loader);
	g_free (info);
}

static int
find_extension_info (const ExtensionInfo *info,
		     const char *identifier)
{
	return strcmp (info->info.identifier, identifier);
}

static ExtensionInfo *
ephy_extensions_manager_parse_keyfile (EphyExtensionsManager *manager,
				       GKeyFile *key_file,
				       const char *identifier)
{
	ExtensionInfo *info;
	EphyExtensionInfo *einfo;
	char *start_group;

	LOG ("Parsing INI description file for '%s'", identifier);

	start_group = g_key_file_get_start_group (key_file);
	if (start_group == NULL ||
	    strcmp (start_group, EE_GROUP) != 0 ||
	    !g_key_file_has_group (key_file, "Loader"))
	{
		g_warning ("Invalid extension description file for '%s'; "
			   "missing 'Epiphany Extension' or 'Loader' group",
			   identifier);
		
		g_key_file_free (key_file);
		g_free (start_group);
		return NULL;
	}
	g_free (start_group);

	if (!g_key_file_has_key (key_file, EE_GROUP, "Name", NULL) ||
	    !g_key_file_has_key (key_file, EE_GROUP, "Description", NULL))
	{
		g_warning ("Invalid extension description file for '%s'; "
			   "missing 'Name' or 'Description' keys.",
			   identifier);
		
		g_key_file_free (key_file);
		return NULL;
	}

	info = g_new0 (ExtensionInfo, 1);
	einfo = (EphyExtensionInfo *) info;
	einfo->identifier = g_strdup (identifier);
	einfo->keyfile = key_file;

	info->loader_type = g_key_file_get_string (key_file, "Loader", "Type", NULL);

	/* sanity check */
	if (info->loader_type == NULL || info->loader_type[0] == '\0')
	{
		free_extension_info (info);
		return NULL;
	}

	manager->priv->data = g_list_prepend (manager->priv->data, info);

	g_signal_emit (manager, signals[ADDED], 0, info);

	return info;
}

static void
ephy_extensions_manager_load_ini_file (EphyExtensionsManager *manager,
				       const char *identifier,
				       const char *path)
{
	GKeyFile *keyfile;
	GError *err = NULL;

	keyfile = g_key_file_new ();
	if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &err))
	{
		g_warning ("Could load key file for '%s': '%s'",
			   identifier, err->message);
		g_error_free (err);
		g_key_file_free (keyfile);
		return;
	}

	ephy_extensions_manager_parse_keyfile (manager, keyfile, identifier);
}

static char *
path_to_identifier (const char *path)
{
	char *identifier, *dot;

	identifier = g_path_get_basename (path);
	dot = strstr (identifier, DOT_INI);

	g_return_val_if_fail (dot != NULL, NULL);

	*dot = '\0';

	return identifier;
}

static ExtensionFormat
format_from_path (const char *path)
{
	ExtensionFormat format = FORMAT_UNKNOWN;

	if (g_str_has_suffix (path, DOT_INI))
	{
		format = FORMAT_INI;
	}

	return format;
}

static void
ephy_extensions_manager_load_file (EphyExtensionsManager *manager,
				   const char *path)
{
	GList *element;
	char *identifier;
	ExtensionFormat format;

	identifier = path_to_identifier (path);
	g_return_if_fail (identifier != NULL);
	if (identifier == NULL) return;

	format = format_from_path (path);
	g_return_if_fail (format != FORMAT_UNKNOWN);

	element = g_list_find_custom (manager->priv->data, identifier,
				      (GCompareFunc) find_extension_info);
	if (element != NULL)
	{
		g_warning ("Extension description for '%s' already read!",
			   identifier);

		g_free (identifier);
		return;
	}

	if (format == FORMAT_INI)
	{
		ephy_extensions_manager_load_ini_file (manager, identifier,
						       path);
	}

	g_free (identifier);
}


static int
find_loader (const LoaderInfo *info,
	      const char *type)
{
	return strcmp (info->type, type);
}

static char *
sanitise_type (const char *string)
{
	char *str, *p;

	str = g_strdup (string);
	for (p = str; *p != '\0'; p++)
	{
		if (!g_ascii_isalpha (*p)) *p = '-';
	}

	return str;
}

static EphyLoader *
get_loader_for_type (EphyExtensionsManager *manager,
		     const char *type)
{
	LoaderInfo *info;
	GList *l;
	char *path, *name, *stype, *data;
	GKeyFile *keyfile;
	EphyLoader *shlib_loader;
	GObject *loader;

	LOG ("Looking for loader for type '%s'", type);

	l = g_list_find_custom (manager->priv->factories, type,
				(GCompareFunc) find_loader);
	if (l != NULL)
	{
		info = (LoaderInfo *) l->data;
		return g_object_ref (info->loader);
	}

	if (strcmp (type, "shlib") == 0)
	{
		info = g_new (LoaderInfo, 1);
		info->type = g_strdup (type);
		info->loader = g_object_new (EPHY_TYPE_SHLIB_LOADER, NULL);

		manager->priv->factories =
			g_list_append (manager->priv->factories, info);

		return g_object_ref (info->loader);
	}
	if (strcmp (type, "python") == 0 ||
	    strcmp (type, "seed") == 0)
	{
		return NULL;
	}

	shlib_loader = get_loader_for_type (manager, "shlib");
	g_return_val_if_fail (shlib_loader != NULL, NULL);

	stype = sanitise_type (type);
	name = g_strconcat ("lib", stype, "loader.", G_MODULE_SUFFIX, NULL);
	path = g_build_filename (LOADER_DIR, name, NULL);
	data = g_strconcat ("[Loader]\nType=shlib\nLibrary=", path, "\n", NULL);
	g_free (stype);
	g_free (name);
	g_free (path);

	keyfile = g_key_file_new ();
	if (!g_key_file_load_from_data (keyfile, data, strlen (data), 0, NULL))
	{
		g_free (data);
		return NULL;
	}

	loader = ephy_loader_get_object (shlib_loader, keyfile);
	g_key_file_free (keyfile);

	if (EPHY_IS_LOADER (loader))
	{
		info = g_new (LoaderInfo, 1);
		info->type = g_strdup (type);
		info->loader = EPHY_LOADER (loader);

		manager->priv->factories =
			g_list_append (manager->priv->factories, info);

		return g_object_ref (info->loader);
	}

	g_return_val_if_reached (NULL);

	return NULL;
}

static void
attach_window (EphyWindow *window,
	       EphyExtension *extension)
{
	GList *tabs, *l;

	ephy_extension_attach_window (extension, window);

	tabs = ephy_embed_container_get_children 
	   (EPHY_EMBED_CONTAINER (ephy_window_get_notebook (window)));
	for (l = tabs; l; l = l->next)
	{
		ephy_extension_attach_tab (extension, window,
					   EPHY_EMBED (l->data));
	}
	g_list_free (tabs);
}

static void
load_extension (EphyExtensionsManager *manager,
		ExtensionInfo *info)
{
	EphyLoader *loader;

	g_return_if_fail (info->extension == NULL);

	LOG ("Loading extension '%s'", info->info.identifier);

	/* don't try again */
	if (info->load_failed) return;

	/* get a loader */
	loader = get_loader_for_type (manager, info->loader_type);
	if (loader == NULL)
	{
		g_message ("No loader found for extension '%s' of type '%s'\n",
			   info->info.identifier, info->loader_type);
		return;
	}

	info->loader = loader;

	info->extension = ephy_loader_get_object (loader, info->info.keyfile);

	/* attach if the extension implements EphyExtensionIface */
	if (EPHY_IS_EXTENSION (info->extension))
	{
		manager->priv->extensions =
			g_list_prepend (manager->priv->extensions,
					g_object_ref (info->extension));

		g_list_foreach (manager->priv->windows, (GFunc) attach_window,
				info->extension);
	}

	if (info->extension != NULL)
	{
		info->info.active = TRUE;
	}
	else
	{
		info->info.active = FALSE;
		info->load_failed = TRUE;
	}
}

static void
detach_window (EphyWindow *window,
	       EphyExtension *extension)
{
	GList *tabs, *l;

	tabs = ephy_embed_container_get_children
		(EPHY_EMBED_CONTAINER (ephy_window_get_notebook (window)));
	for (l = tabs; l; l = l->next)
	{
		ephy_extension_detach_tab (extension, window,
					   EPHY_EMBED (l->data));
	}
	g_list_free (tabs);

	ephy_extension_detach_window (extension, window);
}

static void
unload_extension (EphyExtensionsManager *manager,
		  ExtensionInfo *info)
{
	g_return_if_fail (info->loader != NULL);
	g_return_if_fail (info->extension != NULL || info->load_failed);

	LOG ("Unloading extension '%s'", info->info.identifier);

	if (info->load_failed) return;

	/* detach if the extension implements EphyExtensionIface */
	if (EPHY_IS_EXTENSION (info->extension))
	{
		g_list_foreach (manager->priv->windows, (GFunc) detach_window,
				info->extension);

		manager->priv->extensions =
			g_list_remove (manager->priv->extensions, info->extension);

		/* we own two refs to the extension, the one we added when
		 * we added it to the priv->extensions list, and the one returned
		 * from get_object. Release object, and queue a unref, since if the
		 * extension has its own functions queued in the idle loop, the
		 * functions must exist in memory before being called.
		 */
		ephy_object_idle_unref (info->extension);
	}

	ephy_loader_release_object (info->loader, G_OBJECT (info->extension));

	info->info.active = FALSE;
	info->extension = NULL;
}

static void
sync_loaded_extensions (EphyExtensionsManager *manager)
{
	char **extensions;
	GVariantBuilder builder;
	int i;
	gboolean has_ui = FALSE;
	GList *l;
	ExtensionInfo *info;

	LOG ("Synching changed list of active extensions");

	extensions = g_settings_get_strv (EPHY_SETTINGS_MAIN,
					  EPHY_PREFS_ENABLED_EXTENSIONS);

	g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);

	/* Make sure the extensions-manager-ui is always loaded. */
	for (i = 0; extensions[i]; i++)
	{
		if (g_strcmp0 (extensions[i], "extensions-manager-ui") == 0)
			has_ui = TRUE;

		g_variant_builder_add (&builder, "s", extensions[i]);
	}

	if (!has_ui)
	{
		g_variant_builder_add (&builder, "s", "extensions-manager-ui");
		g_settings_set (EPHY_SETTINGS_MAIN,
				EPHY_PREFS_ENABLED_EXTENSIONS,
				"as", &builder);

		g_strfreev (extensions);
		extensions = g_settings_get_strv
					(EPHY_SETTINGS_MAIN,
					 EPHY_PREFS_ENABLED_EXTENSIONS);
	}
	else
	{
		g_variant_builder_clear (&builder);
	}

	for (l = manager->priv->data; l != NULL; l = l->next)
	{
		gboolean changed;
		gboolean active = FALSE;
		int j;
		
		info = (ExtensionInfo *) l->data;

		for (j = 0; extensions[j]; j++)
		{
			if (!active && g_strcmp0 (extensions[j],
						  info->info.identifier) == 0)
				active = TRUE;
		}

		LOG ("Extension '%s' is %sactive and %sloaded",
		     info->info.identifier,
		     active ? "" : "not ",
		     info->info.active ? "" : "not ");

		changed = (info->info.enabled != active);

		info->info.enabled = active;

		if (active != info->info.active)
		{
			if (active)
			{
				load_extension (manager, info);
			}
			else
			{
				unload_extension (manager, info);
			}

			if (active == info->info.active)
			{
				changed = TRUE;
			}
		}
		
		if (changed)
		{
			g_signal_emit (manager, signals[CHANGED], 0, info);
		}
	}

	g_strfreev (extensions);
}

static void
ephy_extensions_manager_unload_file (EphyExtensionsManager *manager,
				     const char *path)
{
	GList *l;
	ExtensionInfo *info;
	char *identifier;

	identifier = path_to_identifier (path);

	l = g_list_find_custom (manager->priv->data, identifier,
				(GCompareFunc) find_extension_info);

	if (l != NULL)
	{
		info = (ExtensionInfo *) l->data;

		manager->priv->data = g_list_remove (manager->priv->data, info);

		if (info->info.active == TRUE)
		{
			unload_extension (manager, info);
		}

		g_signal_emit (manager, signals[REMOVED], 0, info);

		free_extension_info (info);
	}

	g_free (identifier);
}

static gboolean
reload_sync_cb (EphyExtensionsManager *manager)
{
	EphyExtensionsManagerPrivate *priv = manager->priv;

	if (priv->sync_timeout_id != 0)
	{
		g_source_remove (priv->sync_timeout_id);
		priv->sync_timeout_id = 0;
	}

	sync_loaded_extensions (manager);

	return FALSE;
}

static gboolean
reload_cb (gpointer *data)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (data[0]);
	EphyExtensionsManagerPrivate *priv = manager->priv;
	char *path = data[1];

	LOG ("Reloading %s", path);

	/* We still need path and don't want to remove the timeout
	 * which will be removed automatically when we return, so 
	 * just use _steal instead of _remove.
	 */
	g_hash_table_steal (priv->reload_hash, path);

	ephy_extensions_manager_load_file (manager, path);
	g_free (path);

	/* Schedule a sync of active extensions */
	/* FIXME: just look if we need to activate *this* extension? */

	if (priv->sync_timeout_id != 0)
	{
		g_source_remove (priv->sync_timeout_id);
	}

	priv->sync_timeout_id = g_timeout_add_seconds (RELOAD_SYNC_DELAY,
					       (GSourceFunc) reload_sync_cb,
					       manager);
	return FALSE;
}

static void
schedule_load_from_monitor (EphyExtensionsManager *manager,
			    const char *path)
{
	EphyExtensionsManagerPrivate *priv = manager->priv;
	char *identifier, *copy;
	gpointer *data;
	guint timeout_id;

	/* When a file is installed, it sometimes gets CREATED empty and then
	 * gets its contents filled later (for a CHANGED signal). Theoretically
	 * I suppose we could get a CHANGED signal when the file is half-full,
	 * but I doubt that'll happen much (the files are <1000 bytes). We
	 * don't want warnings all over the place, so we just wait a bit before
	 * actually reloading the file. (We're assuming that if a file is
	 * empty it'll be filled soon and this function will be called again.)
	 *
	 * Oh, and we return if the extension is already loaded, too.
	 */

	identifier = path_to_identifier (path);
	g_return_if_fail (identifier != NULL);
	if (identifier == NULL) return;

	if (g_list_find_custom (manager->priv->data, identifier,
				(GCompareFunc) find_extension_info) != NULL)
	{
		g_free (identifier);
		return;
	}
	g_free (identifier);

	g_return_if_fail (priv->reload_hash != NULL);

	data = g_new (gpointer, 2);
	data[0] = (gpointer) manager;
	data[1] = copy = g_strdup (path);
	timeout_id = g_timeout_add_full (G_PRIORITY_LOW, RELOAD_DELAY,
					 (GSourceFunc) reload_cb,
					 data, (GDestroyNotify) g_free);
	g_hash_table_replace (priv->reload_hash, copy /* owns it */,
			      GUINT_TO_POINTER (timeout_id));
}

static void
dir_changed_cb (GFileMonitor *monitor,
		GFile *child,
		GFile *other_child,
		GFileMonitorEvent event_type,
		EphyExtensionsManager *manager)
{
	char *path;
	
	path = g_file_get_path (child);

	/*
	 * We only deal with XML and INI files:
	 * Add them to the manager when created, remove them when deleted.
	 */
	if (format_from_path (path) == FORMAT_UNKNOWN) return;

	switch (event_type)
	{
		case G_FILE_MONITOR_EVENT_CREATED:
		case G_FILE_MONITOR_EVENT_CHANGED:
			schedule_load_from_monitor (manager, path);
			break;
		case G_FILE_MONITOR_EVENT_DELETED:
			ephy_extensions_manager_unload_file (manager, path);
			break;
		default:
			break;
	}
	
	g_free (path);
}

static void
ephy_extensions_manager_load_dir (EphyExtensionsManager *manager,
				  const char *path)
{
	char *file_path;
	GError *error = NULL;
	GDir *dir;
	const char *dir_elem;
	GFile *directory;
	GFileMonitor *monitor;

	LOG ("Scanning directory '%s'", path);

	START_PROFILER ("Scanning directory")

	dir = g_dir_open (path, 0, &error);
	if (error)
	{
		LOG ("Failed to open extension directory %s: %s",
		     path, error->message);
		g_error_free (error);
		return;
	}

	dir_elem = g_dir_read_name (dir);
	while (dir_elem)
	{
		if (format_from_path (dir_elem) != FORMAT_UNKNOWN)
		{
			file_path = g_build_filename (path, dir_elem, NULL);
			ephy_extensions_manager_load_file (manager, file_path);
			g_free (file_path);
		}

		dir_elem = g_dir_read_name (dir);
	}
	g_dir_close (dir);

	directory = g_file_new_for_path (path);
	monitor = g_file_monitor_directory (directory, 0, NULL, NULL);
	g_object_unref (directory);

	if (monitor != NULL)
	{
		g_signal_connect (monitor, "changed",
				  G_CALLBACK (dir_changed_cb),
				  manager);
		manager->priv->dir_monitors = g_list_prepend
			(manager->priv->dir_monitors, monitor);
	}

	STOP_PROFILER ("Scanning directory")
}

static void
active_extensions_cb (GSettings *settings,
		      char *key,
		      EphyExtensionsManager *manager)
{
	sync_loaded_extensions (manager);
}

static void
cancel_timeout (gpointer data)
{
	guint id = GPOINTER_TO_UINT (data);

	g_source_remove (id);
}

static void
ephy_extensions_manager_init (EphyExtensionsManager *manager)
{
	EphyExtensionsManagerPrivate *priv;

	priv = manager->priv = EPHY_EXTENSIONS_MANAGER_GET_PRIVATE (manager);

	priv->reload_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
						   (GDestroyNotify) g_free,
						   (GDestroyNotify) cancel_timeout);
}

void
ephy_extensions_manager_startup (EphyExtensionsManager *manager)
{
	char *path;

	g_return_if_fail (EPHY_IS_EXTENSIONS_MANAGER (manager));

	LOG ("EphyExtensionsManager startup");

	/* load the extensions descriptions */
	path = g_build_filename (ephy_dot_dir (), "extensions", NULL);
	ephy_extensions_manager_load_dir (manager, path);
	g_free (path);

	ephy_extensions_manager_load_dir (manager, EXTENSIONS_DIR);

	sync_loaded_extensions (manager);

	g_signal_connect (EPHY_SETTINGS_MAIN,
			  "changed::" EPHY_PREFS_ENABLED_EXTENSIONS,
			  G_CALLBACK (active_extensions_cb),
			  manager);
}

static void
ephy_extensions_manager_dispose (GObject *object)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (object);
	EphyExtensionsManagerPrivate *priv = manager->priv;

	if (priv->reload_hash != NULL)
	{
		g_hash_table_destroy (priv->reload_hash);
		priv->reload_hash = NULL;
	}

	if (priv->sync_timeout_id != 0)
	{
		g_source_remove (priv->sync_timeout_id);
		priv->sync_timeout_id = 0;
	}

	if (priv->dir_monitors != NULL)
	{
		g_list_foreach (priv->dir_monitors, (GFunc) g_file_monitor_cancel, NULL);
		g_list_free (priv->dir_monitors);
		priv->dir_monitors = NULL;
	}

	if (priv->extensions != NULL)
	{
		g_list_foreach (priv->extensions, (GFunc) g_object_unref, NULL);
		g_list_free (priv->extensions);
		priv->extensions = NULL;
	}

	if (priv->factories != NULL)
	{
		/* FIXME release loaded loaders */
		g_list_foreach (priv->factories, (GFunc) free_loader_info, NULL);
		g_list_free (priv->factories);
		priv->factories = NULL;
	}

	if (priv->data != NULL)
	{
		g_list_foreach (priv->data, (GFunc) free_extension_info, NULL);
		g_list_free (priv->data);
		priv->data = NULL;
	}

	if (priv->windows != NULL)
	{
		g_list_free (priv->windows);
		priv->windows = NULL;
	}

	G_OBJECT_CLASS (ephy_extensions_manager_parent_class)->dispose (object);
}

static void
attach_extension_to_window (EphyExtension *extension,
			    EphyWindow *window)
{
	attach_window (window, extension);
}

static void
impl_attach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);

	LOG ("Attach window %p", window);

	g_list_foreach (manager->priv->extensions,
			(GFunc) attach_extension_to_window, window);

	manager->priv->windows = g_list_prepend (manager->priv->windows, window);
}

static void
impl_detach_window (EphyExtension *extension,
		    EphyWindow *window)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);
	GList *tabs, *l;

	LOG ("Detach window %p", window);

	manager->priv->windows = g_list_remove (manager->priv->windows, window);

	g_object_ref (window);

	/* Detach tabs (uses impl_detach_tab) */
	tabs = ephy_embed_container_get_children
		(EPHY_EMBED_CONTAINER (ephy_window_get_notebook (window)));
	for (l = tabs; l; l = l->next)
	{
		ephy_extension_detach_tab (extension, window,
					   EPHY_EMBED (l->data));
	}
	g_list_free (tabs);

	/* Then detach the window */
	g_list_foreach (manager->priv->extensions,
			(GFunc) ephy_extension_detach_window, window);

	g_object_unref (window);
}

static void
impl_attach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyEmbed *embed)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);
	GList *l;

	LOG ("Attach window %p embed %p", window, embed);

	for (l = manager->priv->extensions; l; l = l->next)
	{
		ephy_extension_attach_tab (EPHY_EXTENSION (l->data),
					   window, embed);
	}
}

static void
impl_detach_tab (EphyExtension *extension,
		 EphyWindow *window,
		 EphyEmbed *embed)
{
	EphyExtensionsManager *manager = EPHY_EXTENSIONS_MANAGER (extension);
	GList *l;

	LOG ("Detach window %p embed %p", window, embed);

	g_object_ref (window);
	g_object_ref (embed);

	for (l = manager->priv->extensions; l; l = l->next)
	{
		ephy_extension_detach_tab (EPHY_EXTENSION (l->data),
					   window, embed);
	}

	g_object_unref (embed);
	g_object_unref (window);
}

static void
ephy_extensions_manager_iface_init (EphyExtensionIface *iface)
{
	iface->attach_window = impl_attach_window;
	iface->detach_window = impl_detach_window;
	iface->attach_tab    = impl_attach_tab;
	iface->detach_tab    = impl_detach_tab;
}

static void
ephy_extensions_manager_class_init (EphyExtensionsManagerClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->dispose = ephy_extensions_manager_dispose;

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyExtensionsManagerClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);
	signals[ADDED] =
		g_signal_new ("added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyExtensionsManagerClass, added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);
	signals[REMOVED] =
		g_signal_new ("removed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyExtensionsManagerClass, removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);
	
	g_type_class_add_private (object_class, sizeof (EphyExtensionsManagerPrivate));
}
