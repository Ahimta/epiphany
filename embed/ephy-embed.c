/*
 *  Copyright (C) 2000-2003 Marco Pesenti Gritti
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
 *
 *  $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ephy-embed.h"

#include "ephy-marshal.h"
#include "mozilla-embed-single.h"
#include "mozilla-embed.h"

static void ephy_embed_base_init (gpointer g_class);

GType
ephy_embed_chrome_get_type (void)
{
	static GType etype = 0;

	if (etype == 0)
	{
		static const GFlagsValue values[] =
		{
		{ EPHY_EMBED_CHROME_MENUBAR, "EPHY_EMBED_CHROME_MENUBAR", "menubar" },
		{ EPHY_EMBED_CHROME_TOOLBAR, "EPHY_EMBED_CHROME_TOOLBAR", "toolbar" },
		{ EPHY_EMBED_CHROME_STATUSBAR, "EPHY_EMBED_CHROME_STATUSBAR", "statusbar" },
		{ EPHY_EMBED_CHROME_BOOKMARKSBAR, "EPHY_EMBED_CHROME_BOOKMARKSBAR", "bookmarksbar" },
		{ 0, NULL, NULL }
		};

		etype = g_flags_register_static ("EphyEmbedChrome", values);
	}

	return etype;
}

GType
ephy_embed_get_type (void)
{
	static GType ephy_embed_type = 0;

	if (ephy_embed_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (EphyEmbedIface),
			ephy_embed_base_init,
			NULL,
		};

		ephy_embed_type = g_type_register_static (G_TYPE_INTERFACE,
							  "EphyEmbed",
							  &our_info,
							  (GTypeFlags)0);
	}

	return ephy_embed_type;
}

static void
ephy_embed_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized)
	{
/**
 * EphyEmbed::ge-new-window:
 * @embed:
 * @new_embed: a newly-generated child #EphyEmbed
 * @mask: @new_embed's #EphyChromeMask
 *
 * The ::ge_new_window signal is emitted when a new window has been opened by
 * the embed. For example, when a JavaScript popup window is opened.
 **/
		g_signal_new ("ge_new_window",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIface, new_window),
			      NULL, NULL,
			      ephy_marshal_VOID__POINTER_INT,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_POINTER,
			      G_TYPE_INT);
/**
 * EphyEmbed::ge-context-menu:
 * @embed:
 * @event: the #EphyEmbedEvent which triggered this signal
 *
 * The ::ge_context_menu signal is emitted when a context menu is to be
 * displayed. This will usually happen when the user right-clicks on a part of
 * @embed.
 **/
		g_signal_new ("ge_context_menu",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, context_menu),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_OBJECT);
/**
 * EphyEmbed::ge-favicon:
 * @embed:
 * @address: the URL to @embed's web site's favicon
 *
 * The ::ge_favicon signal is emitted when @embed discovers that a favourite
 * icon (favicon) is available for the site it is visiting.
 **/
		g_signal_new ("ge_favicon",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIface, favicon),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);
/**
 * EphyEmbed::ge-location:
 * @embed:
 * @address: the new URL @embed is visiting
 *
 * The ::ge_location signal is emitted when @embed begins to load a new web
 * page. For example, if the user clicks on a link or enters an address of if
 * the previous web page had JavaScript or a META REFRESH tag.
 *
 * The ::ge_location signal will be emitted even when @embed is simply
 * refreshing the same web page.
 **/
		g_signal_new ("ge_location",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIface, location),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);
/**
 * EphyEmbed::ge-net-state:
 * @embed:
 * @uri: the URI @embed is loading
 * @state: the #EmbedState of @embed
 *
 * The ::ge_net_state signal is emitted when @embed's network negotiation state
 * changes. For example, this will indicate when page loading is complete or
 * cancelled.
 **/
		g_signal_new ("ge_net_state",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIface, net_state),
			      NULL, NULL,
			      ephy_marshal_VOID__STRING_INT,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_STRING,
			      G_TYPE_INT);
/**
 * EphyEmbed::ge-dom-mouse-click:
 * @embed:
 * @event: the #EphyEmbedEvent which triggered this signal
 *
 * The ::ge_dom_mouse_click signal is emitted when the user clicks in @embed.
 **/
		g_signal_new ("ge_dom_mouse_click",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, dom_mouse_click),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_OBJECT);
/**
 * EphyEmbed::ge-dom-mouse-down:
 * @embed:
 * @event: the #EphyEmbedEvent which triggered this signal
 *
 * The ::ge_dom_mouse_down signal is emitted when the user depresses a mouse
 * button.
 **/
		g_signal_new ("ge_dom_mouse_down",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, dom_mouse_down),
			      g_signal_accumulator_true_handled, NULL,
			      ephy_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN,
			      1,
			      G_TYPE_OBJECT);
/**
 * EphyEmbed::ge-security-change:
 * @embed:
 * @level: @embed's new #EmbedSecurityLevel
 *
 * The ::ge_security_change signal is emitted when the security level of @embed
 * changes. For example, this will happen when the user browses from an
 * insecure website to an SSL-secured one.
 **/
		g_signal_new ("ge_security_change",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, security_change),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_INT);
/**
 * EphyEmbed::ge-zoom-change:
 * @embed:
 * @zoom: @embed's new zoom level
 *
 * The ::ge_zoom_change signal is emitted when @embed's zoom changes. This can
 * be manual (the user modified the zoom level) or automatic (@embed's zoom is
 * automatically changed when browsing to a new site for which the user
 * previously specified a zoom level).
 *
 * A @zoom value of 1.0 indicates 100% (normal zoom).
 **/
		g_signal_new ("ge_zoom_change",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EphyEmbedIface, zoom_change),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__FLOAT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_FLOAT);

		g_signal_new ("ge_content_change",
			      EPHY_TYPE_EMBED,
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EphyEmbedIface, content_change),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

		initialized = TRUE;
	}
}

/**
 * ephy_embed_load_url:
 * @embed: an #EphyEmbed
 * @url: a URL
 *
 * Loads a new web page in @embed.
 **/
void
ephy_embed_load_url (EphyEmbed *embed,
		     const char *url)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->load_url (embed, url);
}

/**
 * ephy_embed_stop_load:
 * @embed: an #EphyEmbed
 *
 * If @embed is loading, stops it from continuing.
 **/
void
ephy_embed_stop_load (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->stop_load (embed);
}

/**
 * ephy_embed_can_go_back:
 * @embed: an #EphyEmbed
 *
 * Return value: %TRUE if @embed can return to a previously-visited location
 **/
gboolean
ephy_embed_can_go_back (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->can_go_back (embed);
}

/**
 * ephy_embed_can_go_forward:
 * @embed: an #EphyEmbed
 *
 * Return value: %TRUE if @embed has gone back, and can thus go forward again
 **/
gboolean
ephy_embed_can_go_forward (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->can_go_forward (embed);
}

/**
 * ephy_embed_can_go_up:
 * @embed: an #EphyEmbed
 *
 * Returns whether @embed can travel to a higher-level directory on the server.
 * For example, for http://www.example.com/subdir/index.html, returns %TRUE; for
 * http://www.example.com/index.html, returns %FALSE.
 *
 * Return value: %TRUE if @embed can browse to a higher-level directory
 **/
gboolean
ephy_embed_can_go_up (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->can_go_up (embed);
}

/**
 * ephy_embed_get_go_up_list:
 * @embed: an #EphyEmbed
 *
 * Returns a list of (%char *) URLs to higher-level directories on the same
 * server, in order of deepest to shallowest. For example, given
 * "http://www.example.com/dir/subdir/file.html", will return a list containing
 * "http://www.example.com/dir/subdir/", "http://www.example.com/dir/" and
 * "http://www.example.com/".
 *
 * Return value: a list of URLs higher up in @embed's web page's directory
 * hierarchy
 **/
GSList *
ephy_embed_get_go_up_list (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_go_up_list (embed);
}

/**
 * ephy_embed_go_back:
 * @embed: an #EphyEmbed
 *
 * Causes @embed to return to the previously-visited web page.
 **/
void
ephy_embed_go_back (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->go_back (embed);
}

/**
 * ephy_embed_go_forward:
 * @embed: an #EphyEmbed
 *
 * If @embed has returned to a previously-visited web page, proceed forward to
 * the next page.
 **/
void
ephy_embed_go_forward (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->go_forward (embed);
}

/**
 * ephy_embed_go_up:
 * @embed: an #EphyEmbed
 *
 * Moves @embed one level up in its web page's directory hierarchy.
 **/
void
ephy_embed_go_up (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->go_up (embed);
}

/**
 * ephy_embed_get_title:
 * @embed: an #EphyEmbed
 *
 * Return value: the title of the web page displayed in @embed
 **/
char *
ephy_embed_get_title (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_title (embed);
}

/**
 * ephy_embed_get_location:
 * @embed: an #EphyEmbed
 * @toplevel: %FALSE to return the location of the focused frame only
 *
 * Returns the URL of the web page displayed in @embed.
 *
 * If the web page contains frames, @toplevel will determine which location to
 * retrieve. If @toplevel is %TRUE, the return value will be the location of the
 * frameset document. If @toplevel is %FALSE, the return value will be the
 * location of the currently-focused frame.
 *
 * Return value: the URL of the web page displayed in @embed
 **/
char *
ephy_embed_get_location (EphyEmbed *embed,
			 gboolean toplevel)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_location (embed, toplevel);
}

/**
 * ephy_embed_get_link_message:
 * @embed: an #EphyEmbed
 *
 * When the user is hovering the mouse over a hyperlink, returns the URL of the
 * hyperlink.
 *
 * Return value: the URL of the link over which the mouse is hovering
 **/
char *
ephy_embed_get_link_message (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_link_message (embed);
}

/**
 * ephy_embed_get_js_status:
 * @embed: an #EphyEmbed
 *
 * Displays the message JavaScript is attempting to display in the statusbar.
 *
 * Note that Epiphany does not display JavaScript statusbar messages.
 *
 * Return value: a message from JavaScript meant to be displayed in the
 *               statusbar
 **/
char *
ephy_embed_get_js_status (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_js_status (embed);
}

/**
 * ephy_embed_reload:
 * @embed: an #EphyEmbed
 * @flags: %EMBED_RELOAD_FORCE to bypass cache
 *
 * Reloads the web page being displayed in @embed.
 *
 * If @flags is %EMBED_RELOAD_FORCE, cache and proxy will be bypassed when
 * reloading the page. Otherwise, use %EMBED_RELOAD_NORMAL.
 **/
void
ephy_embed_reload (EphyEmbed *embed,
		   EmbedReloadFlags flags)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->reload (embed, flags);
}

/**
 * ephy_embed_set_zoom:
 * @embed: an #EphyEmbed
 * @zoom: the new zoom level
 *
 * Sets the zoom level for a web page.
 *
 * Zoom is normally controlled by the Epiphany itself and remembered in
 * Epiphany's history data. Be very careful not to break this behavior if using
 * this function; better yet, don't use this function at all.
 **/
void
ephy_embed_set_zoom (EphyEmbed *embed,
		     float zoom)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->set_zoom (embed, zoom);
}

/**
 * ephy_embed_get_zoom:
 * @embed: an #EphyEmbed
 *
 * Returns the zoom level of @embed. A zoom of 1.0 corresponds to 100% (normal
 * size).
 *
 * Return value: the zoom level of @embed
 **/
float
ephy_embed_get_zoom (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_zoom (embed);
}

/**
 * ephy_embed_shistory_n_items:
 * @embed: an #EphyEmbed
 *
 * Returns the number of items in @embed's history. In other words, returns the
 * number of pages @embed has visited.
 *
 * The number is upper-bound by Mozilla's browser.sessionhistory.max_entries
 * preference.
 *
 * Return value: the number of items in @embed's history
 **/
int
ephy_embed_shistory_n_items  (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->shistory_n_items (embed);
}

/**
 * ephy_embed_shistory_get_nth:
 * @embed: an #EphyEmbed
 * @nth: index of the desired page in @embed's browser history
 * @is_relative: if %TRUE, add @embed's current history position to @nth
 * @url: returned value of the history entry's URL
 * @title: returned value of the history entry's title
 *
 * Fetches the @url and @title of the @nth item in @embed's session history.
 * If @is_relative is %TRUE, @nth is an offset from the browser's current
 * history position. For example, calling this function with @is_relative %TRUE
 * and @nth %0 will return the URL and title of the current page.
 **/
void
ephy_embed_shistory_get_nth (EphyEmbed *embed,
			     int nth,
			     gboolean is_relative,
			     char **url,
			     char **title)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->shistory_get_nth (embed, nth, is_relative, url, title);
}

/**
 * ephy_embed_shistory_get_pos:
 * @embed: an #EphyEmbed
 *
 * Returns @embed's current position in its history. If the user never uses the
 * "Back" button, this number will be the same as the return value of
 * ephy_embed_shistory_n_items().
 *
 * Return value: @embed's current position in its history
 **/
int
ephy_embed_shistory_get_pos (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->shistory_get_pos (embed);
}

/**
 * ephy_embed_shistory_go_nth:
 * @embed: an #EphyEmbed
 * @nth: desired history index
 *
 * Opens the webpage at location @nth in @embed's history.
 **/
void
ephy_embed_shistory_go_nth (EphyEmbed *embed,
			    int nth)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->shistory_go_nth (embed, nth);
}

/**
 * ephy_embed_get_security_level:
 * @embed: an #EphyEmbed
 * @level: return value of security level
 * @description: return value of the description of the security level
 *
 * Fetches the #EmbedSecurityLevel and a newly-allocated string description
 * of the security state of @embed.
 **/
void
ephy_embed_get_security_level (EphyEmbed *embed,
			       EmbedSecurityLevel *level,
			       char **description)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->get_security_level (embed, level, description);
}

/**
 * ephy_embed_find_set_properties:
 * @embed: an #EphyEmbed
 * @search_string: the desired search string
 * @case_sensitive: %TRUE for "case sensitive" to be set
 * @wrap_around: %TRUE for "wrap around" to be set
 *
 * Sets the properties of @embed's "Find" dialog.
 **/
void
ephy_embed_find_set_properties  (EphyEmbed *embed,
				 const char *search_string,
				 gboolean case_sensitive,
				 gboolean wrap_around)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->find_set_properties (embed, search_string, case_sensitive,
				    wrap_around);
}

/**
 * ephy_embed_find_next:
 * @embed: an #EphyEmbed
 * @backwards: %FALSE to search forwards in the document
 *
 * Equivalent to pressing "Next" in @embed's Find dialog.
 *
 * Return value: %TRUE if a next match was found
 **/
gboolean
ephy_embed_find_next (EphyEmbed *embed,
		      gboolean backwards)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->find_next (embed, backwards);
}

/**
 * ephy_embed_activate:
 * @embed: an #EphyEmbed
 *
 * Gives focus to @embed (i.e., Mozilla).
 **/
void
ephy_embed_activate (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->activate (embed);
}

/**
 * ephy_embed_set_encoding:
 * @embed: an #EphyEmbed
 * @encoding: the desired encoding
 *
 * Sets @embed's character encoding to @encoding. These cryptic encoding
 * strings are listed in <filename>embed/ephy-encodings.c</filename>.
 *
 * Pass an empty string (not NULL) in @encoding to reset @embed to use the
 * document-specified encoding.
 **/
void
ephy_embed_set_encoding (EphyEmbed *embed,
			 const char *encoding)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->set_encoding (embed, encoding);
}

/**
 * ephy_embed_get_encoding:
 * @embed: an #EphyEmbed
 *
 * Returns the @embed's document's encoding
 **/
char *
ephy_embed_get_encoding (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->get_encoding (embed);
}

/**
 * ephy_embed_has_automatic_encoding:
 * @embed: an #EphyEmbed
 *
 * Returns whether the @embed's document's was determined by the document itself
 **/
gboolean
ephy_embed_has_automatic_encoding (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->has_automatic_encoding (embed);
}

/**
 * ephy_embed_print:
 * @embed: an #EphyEmbed
 * @info: an #EmbedPrintInfo with all printing settings
 *
 * Sends a document to the printer.
 *
 * Normally one would use ephy_window_print() to display the print dialog, which
 * will build its own #EmbedPrintInfo and then call this function.
 **/
void
ephy_embed_print (EphyEmbed *embed,
		  EmbedPrintInfo *info)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->print (embed, info);
}

/**
 * ephy_embed_print_preview_close:
 * @embed: an #EphyEmbed
 *
 * Closes @embed's print preview dialog.
 **/
void
ephy_embed_print_preview_close (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	iface->print_preview_close (embed);
}

/**
 * ephy_embed_print_preview_n_pages:
 * @embed: an #EphyEmbed
 *
 * Returns the number of pages which would appear in @embed's loaded document
 * if it were to be printed.
 *
 * Return value: the number of pages in @embed's loaded document
 **/
int
ephy_embed_print_preview_n_pages (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->print_preview_n_pages (embed);
}

/**
 * ephy_embed_print_preview_navigate:
 * @embed: an #EphyEmbed
 * @type: an #EphyPrintPreviewNavType which determines where to navigate
 * @page: if @type is %PRINTPREVIEW_GOTO_PAGENUM, the desired page number
 *
 * Navigates @embed's print preview.
 **/
void
ephy_embed_print_preview_navigate (EphyEmbed *embed,
				   EmbedPrintPreviewNavType type,
				   int page)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->print_preview_navigate (embed, type, page);
}

/**
 * ephy_embed_has_modified_forms:
 * @embed: an #EphyEmbed
 *
 * Returns %TRUE if the user has modified &lt;input&gt; or &lt;textarea&gt;
 * values in @embed's loaded document.
 *
 * Return value: %TRUE if @embed has user-modified forms
 **/
gboolean
ephy_embed_has_modified_forms (EphyEmbed *embed)
{
	EphyEmbedIface *iface = EPHY_EMBED_GET_IFACE (embed);
	return iface->has_modified_forms (embed);
}
