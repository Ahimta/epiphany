/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002 Jorn Baayen <jorn@nl.linux.org>
 *  Copyright © 2003-2004 Marco Pesenti Gritti
 *  Copyright © 2004, 2005 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
 *  $Id$
 */

#include "config.h"

#include "ephy-favicon-cache.h"

#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "ephy-embed-shell.h"
#include "ephy-embed-persist.h"
#include "ephy-embed-factory.h"
#include "ephy-file-helpers.h"
#include "ephy-node-common.h"
#include "ephy-node.h"
#include "ephy-debug.h"
#include "ephy-glib-compat.h"

#include <glib/gstdio.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-directory.h>

#define EPHY_FAVICON_CACHE_XML_ROOT    (const xmlChar *)"ephy_favicons_cache"
#define EPHY_FAVICON_CACHE_XML_VERSION (const xmlChar *)"1.1"

#define EPHY_FAVICON_CACHE_OBSOLETE_DAYS 30
#define SECS_PER_DAY (60*60*24)

/* how often to save the cache, in seconds */
#define CACHE_SAVE_INTERVAL	(10 * 60) /* seconds */

/* how often to delete old pixbufs from the cache, in seconds */
#define CACHE_CLEANUP_INTERVAL	(5 * 60) /* seconds */

/* how long to keep pixbufs in cache, in seconds. This should be longer than CACHE_CLEANUP_INTERVAL */
#define PIXBUF_CACHE_KEEP_TIME	10 * 60 /* s */

/* this is very generous, most files are 4k */
#define EPHY_FAVICON_MAX_SIZE	64 * 1024 /* byte */

static void ephy_favicon_cache_class_init (EphyFaviconCacheClass *klass);
static void ephy_favicon_cache_init	  (EphyFaviconCache *cache);
static void ephy_favicon_cache_finalize	  (GObject *object);
static gboolean kill_download		  (const char*, EphyEmbedPersist*, EphyFaviconCache*);

#define EPHY_FAVICON_CACHE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), EPHY_TYPE_FAVICON_CACHE, EphyFaviconCachePrivate))

struct _EphyFaviconCachePrivate
{
	char *directory;
	char *xml_file;
	EphyNodeDb *db;
	EphyNode *icons;
	GHashTable *icons_hash;
	GHashTable *downloads_hash;
	guint autosave_timeout;
	guint cleanup_timeout;
	guint dirty : 1;
	guint64 requests;
	guint64 cached;
};

typedef struct
{
	EphyNode *node;
	time_t timestamp;
	GdkPixbuf *pixbuf;
	guint load_failed : 1;
} PixbufCacheEntry;

typedef gboolean (* FilterFunc) (EphyNode*, time_t);

enum
{
	CHANGED,
	LAST_SIGNAL
};

enum
{
	EPHY_NODE_FAVICON_PROP_URL	 = 2,
	EPHY_NODE_FAVICON_PROP_FILENAME	 = 3,
	EPHY_NODE_FAVICON_PROP_LAST_USED = 4,
	EPHY_NODE_FAVICON_PROP_STATE	 = 5,
	EPHY_NODE_FAVICON_PROP_CHECKOLD  = 6,
	EPHY_NODE_FAVICON_PROP_CHECKED	 = 7,
};

enum
{
	NEEDS_TYPE_CHECK = 1 << 0,
	NEEDS_RENAME	 = 1 << 1,
	NEEDS_ICO_CHECK	 = 1 << 2,
	NEEDS_MASK	 = 0x3f
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

GType
ephy_favicon_cache_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0))
	{
		const GTypeInfo our_info =
		{
			sizeof (EphyFaviconCacheClass),
			NULL,
			NULL,
			(GClassInitFunc) ephy_favicon_cache_class_init,
			NULL,
			NULL,
			sizeof (EphyFaviconCache),
			0,
			(GInstanceInitFunc) ephy_favicon_cache_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "EphyFaviconCache",
					       &our_info, 0);
	}

	return type;
}

static void
ephy_favicon_cache_class_init (EphyFaviconCacheClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = ephy_favicon_cache_finalize;

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyFaviconCacheClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (EphyFaviconCachePrivate));
}

EphyFaviconCache *
ephy_favicon_cache_new (void)
{
	return EPHY_FAVICON_CACHE (g_object_new (EPHY_TYPE_FAVICON_CACHE, NULL));
}

static void
pixbuf_cache_entry_free (PixbufCacheEntry *entry)
{
	if (entry->pixbuf != NULL)
	{
		g_object_unref (entry->pixbuf);
	}

	g_free (entry);
}

static gboolean
icon_is_obsolete (EphyNode *node, time_t now)
{
	int last_visit;

	last_visit = ephy_node_get_property_int
		(node, EPHY_NODE_FAVICON_PROP_LAST_USED);
	return now - last_visit >= EPHY_FAVICON_CACHE_OBSOLETE_DAYS*SECS_PER_DAY;
}

static void
icons_added_cb (EphyNode *node,
		EphyNode *child,
		EphyFaviconCache *eb)
{
	PixbufCacheEntry *entry = g_new0 (PixbufCacheEntry, 1);

	entry->node = child;

	g_hash_table_insert (eb->priv->icons_hash,
			     (char *) ephy_node_get_property_string (child, EPHY_NODE_FAVICON_PROP_URL),
			     entry);
}

static void
icons_removed_cb (EphyNode *node,
		  EphyNode *child,
		  guint old_index,
		  EphyFaviconCache *eb)
{
	g_hash_table_remove (eb->priv->icons_hash,
			     ephy_node_get_property_string (child, EPHY_NODE_FAVICON_PROP_URL));
}

static void
remove_obsolete_icons (EphyFaviconCache *cache,
		       FilterFunc filter)
{
	EphyFaviconCachePrivate *priv = cache->priv;
	GPtrArray *children;
	int i;
	time_t now;

	now = time (NULL);

	children = ephy_node_get_children (priv->icons);
	for (i = (int) children->len - 1; i >= 0; i--)
	{
		EphyNode *kid;

		kid = g_ptr_array_index (children, i);

		if (!filter || filter (kid, now))
		{
			const char *filename;
			char *path;

			filename = ephy_node_get_property_string
				(kid, EPHY_NODE_FAVICON_PROP_FILENAME);
			path = g_build_filename (priv->directory,
						 filename, NULL);
			gnome_vfs_unlink (path);

			g_free (path);
			ephy_node_unref (kid);
		}
	}
}

static void
prepare_close_cb (EphyEmbedShell *shell,
		  EphyFaviconCache *cache)
{
	EphyFaviconCachePrivate *priv = cache->priv;

	g_hash_table_foreach_remove (priv->downloads_hash,
				     (GHRFunc) kill_download, cache);
}

static void
ephy_favicon_cache_save (EphyFaviconCache *cache)
{
	EphyFaviconCachePrivate *priv = cache->priv;
	int ret;

	if (priv->dirty)
	{
		LOG ("Saving favicon cache\n");

		ret = ephy_node_db_write_to_xml_safe
			(cache->priv->db, (const xmlChar *) priv->xml_file,
			 EPHY_FAVICON_CACHE_XML_ROOT,
			 EPHY_FAVICON_CACHE_XML_VERSION,
			 NULL,
			 priv->icons, NULL, NULL,
			 NULL);

		/* still dirty if save failed */
		priv->dirty = ret < 0;
	}
}

static gboolean
periodic_save_cb (EphyFaviconCache *cache)
{
	ephy_favicon_cache_save (cache);
	return TRUE;
}

static void
cleanup_entry (char *key,
	       PixbufCacheEntry *entry,
	       time_t* now)
{
	if (entry->pixbuf != NULL &&
	    *now - entry->timestamp > PIXBUF_CACHE_KEEP_TIME)
	{
		LOG ("Evicting pixbuf for \"%s\" from pixbuf cache", key);

		g_object_unref (entry->pixbuf);
		entry->pixbuf = NULL;
		entry->load_failed = FALSE;
		entry->timestamp = 0;
	}
}

static gboolean
periodic_cleanup_cb (EphyFaviconCache *cache)
{
	EphyFaviconCachePrivate *priv = cache->priv;
	time_t now;

	LOG ("Cleanup");

	now = time (NULL);
	g_hash_table_foreach (priv->icons_hash, (GHFunc) cleanup_entry, &now);

	return TRUE;
}

static void
ephy_favicon_cache_init (EphyFaviconCache *cache)
{
	EphyFaviconCachePrivate *priv;
	EphyNodeDb *db;

	priv = cache->priv = EPHY_FAVICON_CACHE_GET_PRIVATE (cache);

	db = ephy_node_db_new (EPHY_NODE_DB_SITEICONS);
	cache->priv->db = db;

	cache->priv->xml_file = g_build_filename (ephy_dot_dir (),
						  "ephy-favicon-cache.xml",
						  NULL);

	cache->priv->directory = g_build_filename (ephy_dot_dir (),
						   "favicon_cache",
						   NULL);

	if (g_file_test (cache->priv->directory, G_FILE_TEST_IS_DIR) == FALSE)
	{
		if (mkdir (cache->priv->directory, 488) != 0)
		{
			g_error ("Couldn't mkdir %s.", cache->priv->directory);
		}
	}

	/* The key is owned by the node */
	priv->icons_hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
						  (GDestroyNotify) pixbuf_cache_entry_free);
	priv->downloads_hash = g_hash_table_new_full (g_str_hash,
						      g_str_equal,
						      g_free, NULL);
	
	/* Icons */
	cache->priv->icons = ephy_node_new_with_id (db, ICONS_NODE_ID);
	ephy_node_signal_connect_object (cache->priv->icons,
					 EPHY_NODE_CHILD_ADDED,
					 (EphyNodeCallback) icons_added_cb,
					 G_OBJECT (cache));
	ephy_node_signal_connect_object (cache->priv->icons,
					 EPHY_NODE_CHILD_REMOVED,
					 (EphyNodeCallback) icons_removed_cb,
					 G_OBJECT (cache));

	ephy_node_db_load_from_file (cache->priv->db, cache->priv->xml_file,
				     EPHY_FAVICON_CACHE_XML_ROOT,
				     EPHY_FAVICON_CACHE_XML_VERSION);

	/* listen to prepare-close on the shell */
	g_signal_connect_object (embed_shell, "prepare-close",
				 G_CALLBACK (prepare_close_cb), cache, 0);

	priv->autosave_timeout = g_timeout_add_seconds (CACHE_SAVE_INTERVAL,
						(GSourceFunc) periodic_save_cb,
						cache);
	priv->cleanup_timeout = g_timeout_add_seconds (CACHE_CLEANUP_INTERVAL,
					       (GSourceFunc) periodic_cleanup_cb,
					       cache);
}

static gboolean
kill_download (const char *key,
	       EphyEmbedPersist *persist,
	       EphyFaviconCache *cache)
{
	EphyNode *icon;

	/* disconnect "completed" and "cancelled" callbacks */
	g_signal_handlers_disconnect_matched
		(persist,G_SIGNAL_MATCH_DATA , 0, 0, NULL, NULL, cache);

	/* now cancel the download */
	ephy_embed_persist_cancel (persist);

	icon = g_hash_table_lookup (cache->priv->icons_hash, key);
	g_return_val_if_fail (EPHY_IS_NODE (icon), TRUE);

	ephy_node_unref (icon);

	return TRUE;
}

static gboolean
delete_file (const char *rel_path,
	     GnomeVFSFileInfo *info,
	     gboolean rec_will_loop,
	     EphyFaviconCache *cache,
	     gboolean *recurse)
{
	EphyFaviconCachePrivate *priv = cache->priv;
	char *path;

	*recurse = FALSE;

	g_return_val_if_fail (info != NULL, TRUE);

	if ((info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) == 0 ||
	    info->type != GNOME_VFS_FILE_TYPE_REGULAR) return TRUE;

	path = g_build_filename (priv->directory, rel_path, NULL);
	gnome_vfs_unlink (path);
	g_free (path);

	/* continue with the visit */
	return TRUE;
}

static void
ephy_favicon_cache_finalize (GObject *object)
{
	EphyFaviconCache *cache = EPHY_FAVICON_CACHE (object);
	EphyFaviconCachePrivate *priv = cache->priv;

	LOG ("EphyFaviconCache finalising");

	g_hash_table_foreach_remove (priv->downloads_hash,
				     (GHRFunc) kill_download, cache);

	if (priv->autosave_timeout != 0)
	{
		g_source_remove (priv->autosave_timeout);
	}

	if (priv->cleanup_timeout != 0)
	{
		g_source_remove (priv->cleanup_timeout);
	}

	remove_obsolete_icons (cache, icon_is_obsolete);

	ephy_favicon_cache_save (cache);

	g_free (priv->xml_file);
	g_free (priv->directory);
	g_hash_table_destroy (priv->icons_hash);
	g_hash_table_destroy (priv->downloads_hash);

	g_object_unref (priv->db);

	LOG ("Requests: cached %" G_GUINT64_FORMAT " / total %" G_GUINT64_FORMAT " = %.3f",
	     priv->cached, priv->requests, ((double) priv->cached) / ((double) priv->requests));

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
favicon_download_completed_cb (EphyEmbedPersist *persist,
			       EphyFaviconCache *cache)
{
	const char *url;

	url = ephy_embed_persist_get_source (persist);
	g_return_if_fail (url != NULL);

	LOG ("Favicon cache download completed for %s", url);

	g_hash_table_remove (cache->priv->downloads_hash, url);

	g_signal_emit (G_OBJECT (cache), signals[CHANGED], 0, url);

	g_object_unref (persist);

	cache->priv->dirty = TRUE;
}

static void
favicon_download_cancelled_cb (EphyEmbedPersist *persist,
			       EphyFaviconCache *cache)
{
	const char *url, *dest;

	url = ephy_embed_persist_get_source (persist);
	g_return_if_fail (url != NULL);

	LOG ("Favicon cache download cancelled %s", url);

	g_hash_table_remove (cache->priv->downloads_hash, url);

	/* remove a partially downloaded file */
	dest = ephy_embed_persist_get_dest (persist);
	gnome_vfs_unlink (dest);

	/* FIXME: re-schedule to try again after n days? */

	g_object_unref (persist);

	cache->priv->dirty = TRUE;
}

static void
ephy_favicon_cache_download (EphyFaviconCache *cache,
			     const char *favicon_url,
			     const char *filename)
{
	EphyEmbedPersist *persist;
	char *dest;

	LOG ("Download favicon: %s", favicon_url);

	g_return_if_fail (EPHY_IS_FAVICON_CACHE (cache));
	g_return_if_fail (favicon_url != NULL);
	g_return_if_fail (filename != NULL);

	dest = g_build_filename (cache->priv->directory, filename, NULL);

	persist = EPHY_EMBED_PERSIST
		(ephy_embed_factory_new_object (EPHY_TYPE_EMBED_PERSIST));

	ephy_embed_persist_set_dest (persist, dest);
	ephy_embed_persist_set_flags (persist, EPHY_EMBED_PERSIST_NO_VIEW |
					       EPHY_EMBED_PERSIST_NO_CERTDIALOGS |
					       EPHY_EMBED_PERSIST_DO_CONVERSION |
					       EPHY_EMBED_PERSIST_NO_COOKIES
				               );
	ephy_embed_persist_set_max_size (persist, EPHY_FAVICON_MAX_SIZE);
	ephy_embed_persist_set_source (persist, favicon_url);

	g_free (dest);

	g_signal_connect (G_OBJECT (persist), "completed",
			  G_CALLBACK (favicon_download_completed_cb), cache);
	g_signal_connect (G_OBJECT (persist), "cancelled",
			  G_CALLBACK (favicon_download_cancelled_cb), cache);

	g_hash_table_insert (cache->priv->downloads_hash,
			     g_strdup (favicon_url), persist);

	ephy_embed_persist_save (persist);
}

/**
 * ephy_favicons_cache_get:
 * @cache:
 * @url: the URL of the icon to retrieve
 * 
 * Note: This will always return %NULL for non-http URLs.
 * 
 * Return value: the site icon at @url as a #GdkPixbuf, or %NULL if
 * if could not be retrieved. Unref when you don't need it anymore.
 */
GdkPixbuf *
ephy_favicon_cache_get (EphyFaviconCache *cache,
			const char *url)
{
	EphyFaviconCachePrivate *priv = cache->priv;
	time_t now;
	PixbufCacheEntry *entry;
	EphyNode *icon;
	char *pix_file;
	GdkPixbuf *pixbuf = NULL;
	guint checklevel = NEEDS_MASK;
	int width, height;
	int favicon_checked;

	if (url == NULL) return NULL;

	if (!g_str_has_prefix (url, "http://") &&
	    !g_str_has_prefix (url, "https://")) return NULL;

	priv->requests += 1;

	now = time (NULL);

	entry = g_hash_table_lookup (priv->icons_hash, url);

	if (entry != NULL && entry->pixbuf != NULL)
	{
		LOG ("Pixbuf in cache for \"%s\"", url);

		priv->cached += 1;
		entry->timestamp = now;
		return g_object_ref (entry->pixbuf);
	}
	else if (entry != NULL && entry->load_failed)
	{
		/* No need to try again */
		priv->cached += 1;
		return NULL;
	}

	if (entry == NULL)
	{
		char *filename;

		filename = gnome_thumbnail_md5 (url);

		icon = ephy_node_new (cache->priv->db);
		ephy_node_set_property_string (icon,
					       EPHY_NODE_FAVICON_PROP_URL,
					       url);
		ephy_node_set_property_string (icon,
					       EPHY_NODE_FAVICON_PROP_FILENAME,
					       filename);
		ephy_node_set_property_int (icon,
					    EPHY_NODE_FAVICON_PROP_CHECKED,
					    (int) NEEDS_MASK & ~NEEDS_RENAME);

		/* This will also add an entry to the icons hash */
		ephy_node_add_child (priv->icons, icon);

		ephy_favicon_cache_download (cache, url, filename);

		g_free (filename);

		entry = g_hash_table_lookup (priv->icons_hash, url);
	}

	g_return_val_if_fail (entry != NULL, NULL);
	g_return_val_if_fail (entry->pixbuf == NULL, NULL);

	icon = entry->node;

	/* update timestamp */
	ephy_node_set_property_int (icon, EPHY_NODE_FAVICON_PROP_LAST_USED,
				    now);

	priv->dirty = TRUE;

	if (g_hash_table_lookup (cache->priv->downloads_hash, url) != NULL)
	{
		/* still downloading, return NULL */
		return NULL;
	}

	pix_file = g_build_filename
		(cache->priv->directory,
		 ephy_node_get_property_string (icon, EPHY_NODE_FAVICON_PROP_FILENAME),
		 NULL);

	/* FIXME queue re-download? */
	if (g_file_test (pix_file, G_FILE_TEST_EXISTS) == FALSE)
	{
		g_free (pix_file);
		return NULL;
	}

	/* Check for supported icon types */
	favicon_checked = ephy_node_get_property_int
		(icon, EPHY_NODE_FAVICON_PROP_CHECKED);
	if (favicon_checked != -1)
	{
		checklevel = (guint) favicon_checked;
	}

	/* First, update the filename */
	if (checklevel & NEEDS_RENAME)
	{
		char *new_pix_file, *urlhash;
		GValue value = { 0, };

		urlhash = gnome_thumbnail_md5 (url);
		new_pix_file = g_build_filename (cache->priv->directory, urlhash, NULL);

		g_value_init (&value, G_TYPE_STRING);
		g_value_take_string (&value, urlhash);
		ephy_node_set_property (icon, EPHY_NODE_FAVICON_PROP_FILENAME, &value);
		g_value_unset (&value);

		/* rename the file */
		g_rename (pix_file, new_pix_file);

		g_free (pix_file);
		pix_file = new_pix_file;

		checklevel &= ~NEEDS_RENAME;
		ephy_node_set_property_int (icon,
					    EPHY_NODE_FAVICON_PROP_CHECKED,
					    (int) checklevel);
	}

	/* Now check the type. We renamed the file above, so gnome-vfs does NOT
	 * fall back to extension checking if the slow mime check fails for 
	 * whatever reason
	 */
	if (checklevel & NEEDS_TYPE_CHECK)
	{
		GnomeVFSFileInfo *info;
		gboolean valid = FALSE, is_ao = FALSE;;

		/* Sniff mime type and check if it's safe to open */
		info = gnome_vfs_file_info_new ();
		if (gnome_vfs_get_file_info (pix_file, info,
					     GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
					     GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE) == GNOME_VFS_OK &&
		    (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE) &&
		    info->mime_type != NULL)
		{
			valid = strcmp (info->mime_type, "image/x-ico") == 0 ||
				strcmp (info->mime_type, "image/png") == 0 ||
				strcmp (info->mime_type, "image/gif") == 0;
			is_ao = strcmp (info->mime_type, "application/octet-stream") == 0;
		}
		gnome_vfs_file_info_unref (info);

		/* As a special measure, we try to load an application/octet-stream file
		 * as an ICO file, since we cannot detect a ICO file without .ico extension
		 * (the mime system has no magic for it).
		 */
		if (is_ao)
		{
			GdkPixbufLoader *loader;
			char *buf = NULL;
			gsize count = 0;

			loader = gdk_pixbuf_loader_new_with_type ("ico", NULL);
			if (loader != NULL)
			{
				if (g_file_get_contents (pix_file, &buf, &count, NULL))
				{
					gdk_pixbuf_loader_write (loader, (const guchar *) buf, count, NULL);
					g_free (buf);
				}
				
				gdk_pixbuf_loader_close (loader, NULL);

				pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
				if (pixbuf != NULL)
				{
					g_object_ref (pixbuf);
					valid = TRUE;
				}
	
				g_object_unref (loader);
			}
		}

		/* persist the check value */
		if (valid)
		{
			checklevel &= ~NEEDS_TYPE_CHECK;
		}
		else
		{
			/* remove invalid file from cache */
			/* gnome_vfs_unlink (pix_file); */
		}

		ephy_node_set_property_int (icon,
					    EPHY_NODE_FAVICON_PROP_CHECKED,
					    (int) checklevel);

		/* epiphany 1.6 compat */
		ephy_node_set_property_boolean
			(icon, EPHY_NODE_FAVICON_PROP_CHECKOLD, valid);
	}

	/* if it still needs the check, mime type couldn't be checked. Deny! */
	if (checklevel & NEEDS_TYPE_CHECK)
	{
		g_free (pix_file);
		return NULL;
	}

	/* we could already have a pixbuf from the application/octet-stream check */
	if (pixbuf == NULL)
	{
		pixbuf = gdk_pixbuf_new_from_file (pix_file, NULL);
	}

	g_free (pix_file);

	if (pixbuf == NULL)
	{
		entry->load_failed = TRUE;
		return NULL;
	}

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	/* Reject icons that are too small */
	if (width < 12 || height < 12)
	{
		entry->load_failed = TRUE;
		return NULL;
	}

	/* Scale icons that are too big */
	if (width > 16 || height > 16)
	{
		GdkPixbuf *scaled = gdk_pixbuf_scale_simple (pixbuf, 16, 16,
							     GDK_INTERP_NEAREST);
		g_object_unref (G_OBJECT (pixbuf));
		pixbuf = scaled;
	}

	/* Put the icon in the cache */
	LOG ("Putting pixbuf for \"%s\" in cache", url);

	entry->pixbuf = g_object_ref (pixbuf);
	entry->timestamp = now;
	entry->load_failed = FALSE;

	return pixbuf;
}

/**
 * ephy_favicons_cache_clear:
 * @cache:
 * 
 * Clears the favicon cache and removes any stored icon files from disk.
 */
void
ephy_favicon_cache_clear (EphyFaviconCache *cache)
{
	EphyFaviconCachePrivate *priv = cache->priv;

	g_return_if_fail (EPHY_IS_FAVICON_CACHE (cache));

	remove_obsolete_icons (cache, NULL);
	ephy_favicon_cache_save (cache);

	/* Now remove any remaining files from the cache directory */
	gnome_vfs_directory_visit (priv->directory,
				   GNOME_VFS_FILE_INFO_DEFAULT,
				   GNOME_VFS_DIRECTORY_VISIT_SAMEFS |
				   GNOME_VFS_DIRECTORY_VISIT_LOOPCHECK,
				   (GnomeVFSDirectoryVisitFunc) delete_file,
				   cache);
}
