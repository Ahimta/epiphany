/*
 *  Copyright (C) 2000, 2001, 2002 Marco Pesenti Gritti
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "ephy-shell.h"
#include "ephy-embed-shell.h"
#include "eel-gconf-extensions.h"
#include "ephy-prefs.h"
#include "ephy-favicon-cache.h"
#include "ephy-stock-icons.h"
#include "ephy-filesystem-autocompletion.h"
#include "ephy-window.h"
#include "ephy-file-helpers.h"
#include "ephy-thread-helpers.h"
#include "ephy-bookmarks-import.h"
#include "ephy-debug.h"

#include <string.h>
#include <libgnomeui/gnome-client.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-i18n.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>

#ifdef ENABLE_NAUTILUS_VIEW

#include <bonobo/bonobo-generic-factory.h>
#include "ephy-nautilus-view.h"

#define EPHY_NAUTILUS_VIEW_OAFIID "OAFIID:GNOME_Ephy_NautilusViewFactory"

#endif

struct EphyShellPrivate
{
	Session *session;
	EphyAutocompletion *autocompletion;
	EphyBookmarks *bookmarks;
};

enum
{
        STARTPAGE_HOME,
        STARTPAGE_LAST,
        STARTPAGE_BLANK,
};

static void
ephy_shell_class_init (EphyShellClass *klass);
static void
ephy_shell_init (EphyShell *gs);
static void
ephy_shell_finalize (GObject *object);
static void
ephy_init_services (EphyShell *gs);

#ifdef ENABLE_NAUTILUS_VIEW

static void
ephy_nautilus_view_init_factory (EphyShell *gs);
static BonoboObject *
ephy_nautilus_view_new (BonoboGenericFactory *factory,
			  const char *id,
			  EphyShell *gs);

#endif

static GObjectClass *parent_class = NULL;

EphyShell *ephy_shell;

GType
ephy_shell_get_type (void)
{
        static GType ephy_shell_type = 0;

        if (ephy_shell_type == 0)
        {
                static const GTypeInfo our_info =
                {
                        sizeof (EphyShellClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) ephy_shell_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (EphyShell),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) ephy_shell_init
                };

                ephy_shell_type = g_type_register_static (EPHY_EMBED_SHELL_IMPL,
							  "EphyShell",
							  &our_info, 0);
        }

        return ephy_shell_type;

}

static void
ephy_shell_class_init (EphyShellClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = ephy_shell_finalize;
}

static void
ephy_shell_command_cb (EphyEmbedShell *shell,
		       char *command,
		       char *param,
		       gpointer data)
{
	EphyBookmarks *bookmarks;
	GtkWidget *dialog;

	bookmarks = ephy_shell_get_bookmarks (EPHY_SHELL (shell));

	if (strcmp (command, "import-bookmarks") == 0)
	{
		ephy_bookmarks_import_mozilla (bookmarks, param);

		dialog = gtk_message_dialog_new
			(NULL,
                         GTK_DIALOG_MODAL,
                         GTK_MESSAGE_ERROR,
                         GTK_BUTTONS_OK,
                         _("Bookmarks imported successfully."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
	else if (strcmp (command, "configure-network") == 0)
	{
		ephy_file_launch_application ("gnome-network-preferences",
					      NULL,
					      FALSE);
	}
}

static void
ephy_shell_new_window_cb (EphyEmbedShell *shell,
			  EphyEmbed **new_embed,
                          EmbedChromeMask chromemask,
			  gpointer data)
{
	EphyTab *new_tab;
        EphyWindow *window;
	gboolean open_in_tab;

	g_assert (new_embed != NULL);

	open_in_tab = (chromemask & EMBED_CHROME_OPENASCHROME) ?
		      FALSE :
		      eel_gconf_get_boolean (CONF_TABS_TABBED_POPUPS);

	if (open_in_tab)
	{
		window = ephy_shell_get_active_window (ephy_shell);
	}
	else
	{
		window = ephy_window_new ();
		ephy_window_set_chrome (window, chromemask);
	}

	new_tab = ephy_tab_new ();
	ephy_window_add_tab (window, new_tab, FALSE);
	*new_embed = ephy_tab_get_embed (new_tab);
}

static void
ephy_shell_init (EphyShell *gs)
{
	ephy_shell = gs;
	g_object_add_weak_pointer (G_OBJECT(ephy_shell),
				   (gpointer *)&ephy_shell);

	ephy_debug_init ();
	ephy_thread_helpers_init ();
	ephy_node_system_init ();
	ephy_file_helpers_init ();
	ephy_stock_icons_init ();
	ephy_ensure_dir_exists (ephy_dot_dir ());

        gs->priv = g_new0 (EphyShellPrivate, 1);
	gs->priv->session = NULL;
	gs->priv->bookmarks = NULL;

	g_signal_connect (G_OBJECT (gs),
			  "new_window_orphan",
			  G_CALLBACK(ephy_shell_new_window_cb),
			  NULL);

	g_signal_connect (G_OBJECT (gs),
			  "command",
			  G_CALLBACK(ephy_shell_command_cb),
			  NULL);

	ephy_init_services (gs);
}

static void
ephy_shell_finalize (GObject *object)
{
        EphyShell *gs;

        g_return_if_fail (object != NULL);
        g_return_if_fail (IS_EPHY_SHELL (object));

	gs = EPHY_SHELL (object);

        g_return_if_fail (gs->priv != NULL);

	g_assert (ephy_shell == NULL);

	if (gs->priv->session)
	{
		g_return_if_fail (IS_SESSION(gs->priv->session));
		g_object_remove_weak_pointer
			(G_OBJECT(gs->priv->session),
                         (gpointer *)&gs->priv->session);
		g_object_unref (G_OBJECT (gs->priv->session));
	}

	if (gs->priv->autocompletion)
	{
		g_object_unref (gs->priv->autocompletion);
	}

	if (gs->priv->bookmarks)
	{
		g_object_unref (gs->priv->bookmarks);
	}

        G_OBJECT_CLASS (parent_class)->finalize (object);

        g_free (gs->priv);

	ephy_file_helpers_shutdown ();
	ephy_node_system_shutdown ();

	LOG ("Ephy shell finalized")

	bonobo_main_quit ();

	LOG ("Bonobo quit done")
}

EphyShell *
ephy_shell_new (void)
{
	return EPHY_SHELL (g_object_new (EPHY_SHELL_TYPE, NULL));
}

static void
ephy_init_services (EphyShell *gs)
{
	/* preload the prefs */
	/* it also enables notifiers support */
	eel_gconf_monitor_add ("/apps/epiphany");
	eel_gconf_monitor_add ("/apps/nautilus/preferences");

#ifdef ENABLE_NAUTILUS_VIEW

	ephy_nautilus_view_init_factory (gs);

#endif

}

static char *
build_homepage_url (EphyShell *gs,
		    EphyEmbed *previous_embed)
{
        const gchar *last_page_url;
        gchar *home_page_url;
        gint page_type;
        EphyHistory *gh;
	char *result = NULL;

        if (previous_embed == NULL)
        {
                page_type = STARTPAGE_HOME;
        }
        else
        {
                page_type = eel_gconf_get_integer (CONF_GENERAL_NEWPAGE_TYPE);
        }

        /* return the appropriate page */
        if (page_type == STARTPAGE_HOME)
        {
                /* get location of home page */
                home_page_url = eel_gconf_get_string(CONF_GENERAL_HOMEPAGE);
		result = home_page_url;
        }
	else if (page_type == STARTPAGE_LAST)
	{
		if (previous_embed != NULL)
		{
			ephy_embed_get_location (previous_embed,
						 TRUE,
						 &result);
		}

		if (result == NULL)
		{
			/* get location of last page */
			gh = ephy_embed_shell_get_global_history
				(EPHY_EMBED_SHELL (gs));
			last_page_url = ephy_history_get_last_page (gh);
			result = g_strdup (last_page_url);
		}
	}

	if (result == NULL)
	{
		/* even in case of error, it's a good default */
		result = g_strdup ("about:blank");
	}

	return result;
}

/**
 * ephy_shell_get_active_window:
 * @gs: a #EphyShell
 *
 * Get the current active window. Use it when you
 * need to take an action (like opening an url) on
 * a window but you dont have a target window.
 * Ex. open a new tab from command line.
 *
 * Return value: the current active window
 **/
EphyWindow *
ephy_shell_get_active_window (EphyShell *gs)
{
	Session *session;
	GList *windows;

	session = ephy_shell_get_session (gs);
	windows = session_get_windows (session);

	if (windows == NULL) return NULL;

	return EPHY_WINDOW(windows->data);
}

/**
 * ephy_shell_new_tab:
 * @shell: a #EphyShell
 * @parent_window: the target #EphyWindow or %NULL
 * @previous_tab: the referrer tab or %NULL
 * @url: an url to load or %NULL
 *
 * Create a new tab and the parent window when necessary.
 * Ever use this function to open urls in new window/tabs.
 *
 * ReturnValue: the created #EphyTab
 **/
EphyTab *
ephy_shell_new_tab (EphyShell *shell,
		      EphyWindow *parent_window,
		      EphyTab *previous_tab,
		      const char *url,
		      EphyNewTabFlags flags)
{
	EphyWindow *window;
	EphyTab *tab;
	EphyEmbed *embed;
	gboolean in_new_window;
	gboolean jump_to;
	EphyEmbed *previous_embed = NULL;

	in_new_window = !eel_gconf_get_boolean (CONF_TABS_TABBED);

	if (flags & EPHY_NEW_TAB_IN_NEW_WINDOW) in_new_window = TRUE;
	if (flags & EPHY_NEW_TAB_IN_EXISTING_WINDOW) in_new_window = FALSE;

	jump_to = eel_gconf_get_boolean (CONF_TABS_TABBED_AUTOJUMP);

	if (flags & EPHY_NEW_TAB_JUMP) jump_to = TRUE;
	if (flags & EPHY_NEW_TAB_DONT_JUMP_TO) jump_to = FALSE;

	if (!in_new_window && parent_window != NULL)
	{
		window = parent_window;
	}
	else
	{
		window = ephy_window_new ();
	}

	if (previous_tab)
	{
		previous_embed = ephy_tab_get_embed (previous_tab);
	}

	tab = ephy_tab_new ();
	embed = ephy_tab_get_embed (tab);
	gtk_widget_show (GTK_WIDGET(embed));
	ephy_window_add_tab (window, tab,
			     jump_to);
	gtk_widget_show (GTK_WIDGET(window));

	if (flags & EPHY_NEW_TAB_HOMEPAGE)
	{
		char *homepage;

		homepage = build_homepage_url (shell, previous_embed);
		g_assert (homepage != NULL);

		ephy_embed_load_url (embed, homepage);

		g_free (homepage);
	}
	else if ((flags & EPHY_NEW_TAB_IS_A_COPY) ||
		 (flags & EPHY_NEW_TAB_VIEW_SOURCE))
	{
		EmbedDisplayType display_type =
			(flags & EPHY_NEW_TAB_VIEW_SOURCE) ?
			 DISPLAY_AS_SOURCE : DISPLAY_NORMAL;
		EphyEmbed *source = ephy_tab_get_embed(previous_tab);
		ephy_embed_load_url (embed, "about:blank");
		ephy_embed_copy_page (embed, source, display_type);
	}
	else if (url)
	{
		ephy_embed_load_url (embed, url);
	}

        return tab;
}

#ifdef ENABLE_NAUTILUS_VIEW

static void
ephy_nautilus_view_init_factory (EphyShell *gs)
{
	BonoboGenericFactory *ephy_nautilusview_factory;
	ephy_nautilusview_factory = bonobo_generic_factory_new
		(EPHY_NAUTILUS_VIEW_OAFIID,
		 (BonoboFactoryCallback) ephy_nautilus_view_new, gs);
	if (!BONOBO_IS_GENERIC_FACTORY (ephy_nautilusview_factory))
		g_warning ("Couldn't create the factory!");

}

static BonoboObject *
ephy_nautilus_view_new (BonoboGenericFactory *factory, const char *id,
			  EphyShell *gs)
{
	return ephy_nautilus_view_new_component (gs);
}

#endif

/**
 * ephy_shell_get_session:
 * @gs: a #EphyShell
 *
 * Returns current session.
 *
 * Return value: the current session.
 **/
Session *
ephy_shell_get_session (EphyShell *gs)
{
	if (!gs->priv->session)
	{
		gs->priv->session = session_new ();
		g_object_add_weak_pointer
			(G_OBJECT(gs->priv->session),
                         (gpointer *)&gs->priv->session);
	}

	return gs->priv->session;
}

EphyAutocompletion *
ephy_shell_get_autocompletion (EphyShell *gs)
{
	EphyShellPrivate *p = gs->priv;

	if (!p->autocompletion)
	{
		static const gchar *prefixes[] = {
			EPHY_AUTOCOMPLETION_USUAL_WEB_PREFIXES,
			NULL
		};

		EphyHistory *gh = ephy_embed_shell_get_global_history (EPHY_EMBED_SHELL (gs));
		EphyBookmarks *bmk = ephy_shell_get_bookmarks (gs);
		EphyFilesystemAutocompletion *fa = ephy_filesystem_autocompletion_new ();
		p->autocompletion = ephy_autocompletion_new ();
		ephy_autocompletion_set_prefixes (p->autocompletion, prefixes);

		ephy_autocompletion_add_source (p->autocompletion,
						EPHY_AUTOCOMPLETION_SOURCE (gh));
		ephy_autocompletion_add_source (p->autocompletion,
						EPHY_AUTOCOMPLETION_SOURCE (fa));
		ephy_autocompletion_add_source (p->autocompletion,
						EPHY_AUTOCOMPLETION_SOURCE (bmk));

		g_object_unref (gh);
		g_object_unref (fa);
		g_object_unref (gs->priv->bookmarks);
	}
	return p->autocompletion;
}

EphyBookmarks *
ephy_shell_get_bookmarks (EphyShell *gs)
{
	if (gs->priv->bookmarks == NULL)
	{
		gs->priv->bookmarks = ephy_bookmarks_new ();
	}

	return gs->priv->bookmarks;
}
