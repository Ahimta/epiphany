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

#include "popup-commands.h"
#include "ephy-shell.h"
#include "ephy-new-bookmark.h"
#include "ephy-embed-persist.h"
#include "ephy-prefs.h"
#include "ephy-embed-utils.h"
#include "eel-gconf-extensions.h"
#include "ephy-file-helpers.h"

#include <string.h>

static EphyEmbedEvent *
get_event_info (EphyWindow *window)
{
	EphyEmbedEvent *info;
	EphyTab *tab;

	tab = ephy_window_get_active_tab (window);
	g_return_val_if_fail (tab != NULL, NULL);

	info = ephy_tab_get_event (tab);
	g_return_val_if_fail (info != NULL, NULL);

	return info;
}

void
popup_cmd_link_in_new_window (EggAction *action,
		              EphyWindow *window)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	GValue *value;

	tab = ephy_window_get_active_tab (window);

	info = get_event_info (window);

	ephy_embed_event_get_property (info, "link", &value);

	ephy_shell_new_tab (ephy_shell, NULL, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_IN_NEW_WINDOW);
}

void
popup_cmd_link_in_new_tab (EggAction *action,
		           EphyWindow *window)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	GValue *value;

	tab = ephy_window_get_active_tab (window);

	info = get_event_info (window);

	ephy_embed_event_get_property (info, "link", &value);

	ephy_shell_new_tab (ephy_shell, window, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
}

void
popup_cmd_image_in_new_tab (EggAction *action,
			    EphyWindow *window)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	GValue *value;

	tab = ephy_window_get_active_tab (window);

	info = get_event_info (window);

	ephy_embed_event_get_property (info, "image", &value);

	ephy_shell_new_tab (ephy_shell, window, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);
}

void
popup_cmd_image_in_new_window (EggAction *action,
			       EphyWindow *window)
{
	EphyEmbedEvent *info;
	EphyTab *tab;
	GValue *value;

	tab = ephy_window_get_active_tab (window);

	info = get_event_info (window);

	ephy_embed_event_get_property (info, "image", &value);

	ephy_shell_new_tab (ephy_shell, NULL, tab,
			    g_value_get_string (value),
			    EPHY_NEW_TAB_IN_NEW_WINDOW);
}

void
popup_cmd_add_link_bookmark (EggAction *action,
			     EphyWindow *window)
{
	GtkWidget *new_bookmark;
	EphyBookmarks *bookmarks;
	EphyEmbedEvent *info;
	EphyEmbed *embed;
	GValue *link_title;
	GValue *link_rel;
	GValue *link;
	GValue *link_is_smart;
	const char *title;
	const char *location;
	const char *rel;
	gboolean is_smart;

	info = get_event_info (window);
	embed = ephy_window_get_active_embed (window);

	ephy_embed_event_get_property (info, "link_is_smart", &link_is_smart);
	ephy_embed_event_get_property (info, "link", &link);
	ephy_embed_event_get_property (info, "link_title", &link_title);
	ephy_embed_event_get_property (info, "link_rel", &link_rel);

	title = g_value_get_string (link_title);
	location = g_value_get_string (link);
	rel = g_value_get_string (link_rel);
	is_smart = g_value_get_int (link_is_smart);

	g_return_if_fail (location);

	if (!title || !title[0])
	{
		title = location;
	}

	bookmarks = ephy_shell_get_bookmarks (ephy_shell);
	new_bookmark = ephy_new_bookmark_new
		(bookmarks, GTK_WINDOW (window), location);
	ephy_new_bookmark_set_title
		(EPHY_NEW_BOOKMARK (new_bookmark), title);
	ephy_new_bookmark_set_smarturl
		(EPHY_NEW_BOOKMARK (new_bookmark), rel);
	gtk_widget_show (new_bookmark);
}

void
popup_cmd_frame_in_new_tab (EggAction *action,
			    EphyWindow *window)
{
	EphyTab *tab;
	EphyEmbed *embed;
	char *location;

	tab = ephy_window_get_active_tab (window);

	embed = ephy_window_get_active_embed (window);

	ephy_embed_get_location (embed, FALSE, &location);

	ephy_shell_new_tab (ephy_shell, window, tab,
			    location,
			    EPHY_NEW_TAB_IN_EXISTING_WINDOW);

	g_free (location);
}

void
popup_cmd_frame_in_new_window (EggAction *action,
			       EphyWindow *window)
{
	EphyTab *tab;
	EphyEmbed *embed;
	char *location;

	tab = ephy_window_get_active_tab (window);

	embed = ephy_window_get_active_embed (window);

	ephy_embed_get_location (embed, FALSE, &location);

	ephy_shell_new_tab (ephy_shell, NULL, tab,
			    location,
			    EPHY_NEW_TAB_IN_NEW_WINDOW);

	g_free (location);
}

static void
popup_cmd_copy_to_clipboard (EphyWindow *window, const char *text)
{
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_NONE),
				text, -1);
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY),
				text, -1);
}

void
popup_cmd_copy_page_location (EggAction *action,
                              EphyWindow *window)
{
	char *location;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_get_location (embed, FALSE, &location);
	popup_cmd_copy_to_clipboard (window, location);
	g_free (location);
}

void
popup_cmd_copy_email (EggAction *action,
                      EphyWindow *window)
{
	EphyEmbedEvent *info;
	const char *location;
	GValue *value;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, "email", &value);
	location = g_value_get_string (value);
	popup_cmd_copy_to_clipboard (window, location);
}

void
popup_cmd_copy_link_location (EggAction *action,
			      EphyWindow *window)
{
	EphyEmbedEvent *info;
	const char *location;
	GValue *value;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, "link", &value);
	location = g_value_get_string (value);
	popup_cmd_copy_to_clipboard (window, location);
}

static void
save_property_url (EggAction *action,
		   EphyWindow *window,
		   gboolean ask_dest,
		   gboolean show_progress,
		   const char *property)
{
	EphyEmbedEvent *info;
	const char *location;
	GValue *value;
	GtkWidget *widget;
	EphyEmbedPersist *persist;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, property, &value);
	location = g_value_get_string (value);

	widget = GTK_WIDGET (embed);

	persist = ephy_embed_persist_new (embed);

	ephy_embed_persist_set_source (persist, location);

	if (show_progress)
	{
		ephy_embed_persist_set_flags (persist,
					      EMBED_PERSIST_SHOW_PROGRESS);
	}

	ephy_embed_utils_save (GTK_WIDGET (window),
			       CONF_STATE_DOWNLOADING_DIR,
			       ask_dest,
                               FALSE,
                               persist);
}

void
popup_cmd_open_link (EggAction *action,
		     EphyWindow *window)
{
	EphyEmbedEvent *info;
	const char *location;
	GValue *value;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, "link", &value);
	location = g_value_get_string (value);

	ephy_embed_load_url (embed, location);
}

void
popup_cmd_download_link (EggAction *action,
			 EphyWindow *window)
{
	save_property_url (action, window,
		           eel_gconf_get_boolean
		           (CONF_STATE_DOWNLOADING_DIR),
		           TRUE, "link");
}

void
popup_cmd_save_image_as (EggAction *action,
			 EphyWindow *window)
{
	save_property_url (action, window, TRUE, FALSE, "image");
}

#define CONF_DESKTOP_BG_PICTURE "/desktop/gnome/background/picture_filename"
#define CONF_DESKTOP_BG_TYPE "/desktop/gnome/background/picture_options"

static void
background_download_completed (EphyEmbedPersist *persist,
			       gpointer data)
{
	const char *bg;
	char *type;

	ephy_embed_persist_get_dest (persist, &bg);
	eel_gconf_set_string (CONF_DESKTOP_BG_PICTURE, bg);

	type = eel_gconf_get_string (CONF_DESKTOP_BG_TYPE);
	if (type || strcmp (type, "none") == 0)
	{
		eel_gconf_set_string (CONF_DESKTOP_BG_TYPE,
				      "wallpaper");
	}

	g_free (type);

	g_object_unref (persist);
}

void
popup_cmd_set_image_as_background (EggAction *action,
				   EphyWindow *window)
{
	EphyEmbedEvent *info;
	const char *location;
	char *dest, *base;
	GValue *value;
	EphyEmbedPersist *persist;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, "image", &value);
	location = g_value_get_string (value);

	persist = ephy_embed_persist_new (embed);

	base = g_path_get_basename (location);
	dest = g_build_filename (ephy_dot_dir (),
				 base, NULL);

	ephy_embed_persist_set_source (persist, location);
	ephy_embed_persist_set_dest (persist, dest);

	ephy_embed_persist_save (persist);

	g_signal_connect (persist, "completed",
			  G_CALLBACK (background_download_completed),
			  NULL);

	g_free (dest);
	g_free (base);
}

void
popup_cmd_copy_image_location (EggAction *action,
			       EphyWindow *window)
{
	EphyEmbedEvent *info;
	const char *location;
	GValue *value;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, "image", &value);
	location = g_value_get_string (value);
	popup_cmd_copy_to_clipboard (window, location);
}

void
popup_cmd_save_background_as (EggAction *action,
			      EphyWindow *window)
{
	save_property_url (action, window, TRUE, FALSE, "background_image");
}

void
popup_cmd_open_frame (EggAction *action,
		      EphyWindow *window)
{
	char *location;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	ephy_embed_get_location (embed, FALSE, &location);

	ephy_embed_load_url (embed, location);
}

void
popup_cmd_open_image (EggAction *action,
		      EphyWindow *window)
{
	EphyEmbedEvent *info;
	const char *location;
	GValue *value;
	EphyEmbed *embed;

	embed = ephy_window_get_active_embed (window);
	g_return_if_fail (embed != NULL);

	info = get_event_info (window);
	ephy_embed_event_get_property (info, "image", &value);
	location = g_value_get_string (value);

	ephy_embed_load_url (embed, location);
}

