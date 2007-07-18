/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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
 *  $Id$
 */

#include "mozilla-config.h"
#include "config.h"

#include <nsStringAPI.h>

#include <nsIDOMKeyEvent.h>
#include <nsIDOMMouseEvent.h>
#include <nsIRequest.h>
#include <nsIURI.h>
#include <nsIWebNavigation.h>
#include <nsIWebProgressListener.h>
#include <nsMemory.h>

#include "EphyBrowser.h"
#include "EphyUtils.h"
#include "EventContext.h"

#include "gecko-dom-event.h"
#include "gecko-dom-event-internal.h"

#include "ephy-command-manager.h"
#include "ephy-debug.h"
#include "ephy-embed-shell.h"
#include "ephy-embed-single.h"
#include "ephy-string.h"
#include "mozilla-embed-event.h"

#include "mozilla-embed.h"

static void	mozilla_embed_class_init	(MozillaEmbedClass *klass);
static void	mozilla_embed_init		(MozillaEmbed *gs);
static void	mozilla_embed_destroy		(GtkObject *object);
static void	mozilla_embed_finalize		(GObject *object);
static void	ephy_embed_iface_init		(EphyEmbedIface *iface);

static void mozilla_embed_location_changed_cb	(GeckoEmbed *embed,
						 MozillaEmbed *membed);
static void mozilla_embed_net_state_all_cb	(GeckoEmbed *embed,
						 const char *aURI,
						 gint state,
						 guint status,
						 MozillaEmbed *membed);
static gboolean mozilla_embed_dom_mouse_click_cb(GeckoEmbed *embed,
                                                 GeckoDOMEvent *dom_event,
						 MozillaEmbed *membed);
static gboolean mozilla_embed_dom_mouse_down_cb	(GeckoEmbed *embed,
                                                 GeckoDOMEvent *dom_event,
						 MozillaEmbed *membed);
static gboolean mozilla_embed_dom_key_press_cb	(GeckoEmbed *embed,
                                                 GeckoDOMEvent *dom_event,
						 MozillaEmbed *membed);
static void mozilla_embed_new_window_cb		(GeckoEmbed *embed,
						 GeckoEmbed **newEmbed,
						 guint chrome_mask,
						 MozillaEmbed *membed);
static void mozilla_embed_security_change_cb	(GeckoEmbed *embed,
						 gpointer request,
						 PRUint32 state,
						 MozillaEmbed *membed);
static EphyEmbedSecurityLevel mozilla_embed_security_level (PRUint32 state);

#define MOZILLA_EMBED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), MOZILLA_TYPE_EMBED, MozillaEmbedPrivate))

typedef enum
{
	MOZILLA_EMBED_LOAD_STARTED,
	MOZILLA_EMBED_LOAD_REDIRECTING,
	MOZILLA_EMBED_LOAD_LOADING,
	MOZILLA_EMBED_LOAD_STOPPED
} MozillaEmbedLoadState;

struct MozillaEmbedPrivate
{
	EphyBrowser *browser;
	MozillaEmbedLoadState load_state;
};

#define WINDOWWATCHER_CONTRACTID "@mozilla.org/embedcomp/window-watcher;1"

static GObjectClass *parent_class = NULL;

static void
impl_manager_do_command (EphyCommandManager *manager,
			 const char *command) 
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(manager)->priv;

	mpriv->browser->DoCommand (command);
}

static gboolean
impl_manager_can_do_command (EphyCommandManager *manager,
			     const char *command) 
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(manager)->priv;
	nsresult rv;
	PRBool enabled;

        rv = mpriv->browser->GetCommandState (command, &enabled);

	return NS_SUCCEEDED (rv) ? enabled : FALSE;
}

static void
ephy_command_manager_iface_init (EphyCommandManagerIface *iface)
{
	iface->do_command = impl_manager_do_command;
	iface->can_do_command = impl_manager_can_do_command;
}
	
GType 
mozilla_embed_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0))
        {
                const GTypeInfo our_info =
                {
                        sizeof (MozillaEmbedClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) mozilla_embed_class_init,
                        NULL,
                        NULL, /* class_data */
                        sizeof (MozillaEmbed),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) mozilla_embed_init
                };

		const GInterfaceInfo embed_info =
		{
			(GInterfaceInitFunc) ephy_embed_iface_init,
        		NULL,
        		NULL
     		};

		const GInterfaceInfo ephy_command_manager_info =
		{
			(GInterfaceInitFunc) ephy_command_manager_iface_init,
        		NULL,
        		NULL
     		 };
	
                type = g_type_register_static (GECKO_TYPE_EMBED,
					       "MozillaEmbed",
					       &our_info, 
					       (GTypeFlags)0);
		g_type_add_interface_static (type,
                                   	     EPHY_TYPE_EMBED,
                                   	     &embed_info);
		g_type_add_interface_static (type,
                                   	     EPHY_TYPE_COMMAND_MANAGER,
                                   	     &ephy_command_manager_info);
        }

        return type;
}

static void
mozilla_embed_grab_focus (GtkWidget *widget)
{
	GtkWidget *child;

	child = gtk_bin_get_child (GTK_BIN (widget));

	if (child != NULL)
	{
		gtk_widget_grab_focus (child);
	}
	else
	{
		g_warning ("Need to realize the embed before grabbing focus!\n");
	}
}

static void
impl_close (EphyEmbed *embed) 
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (embed)->priv;

	mpriv->browser->Close ();
}

static void
mozilla_embed_realize (GtkWidget *widget)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (widget)->priv;

	GTK_WIDGET_CLASS (parent_class)->realize (widget);

	/* Initialise our helper class */
	nsresult rv;
	rv = mpriv->browser->Init (GECKO_EMBED (widget));
	if (NS_FAILED (rv))
	{
		g_warning ("EphyBrowser initialization failed for %p\n", widget);
		return;
	}
}

static GObject *
mozilla_embed_constructor (GType type, guint n_construct_properties,
			   GObjectConstructParam *construct_params)
{
	g_object_ref (embed_shell);

	/* we depend on single because of mozilla initialization */
	ephy_embed_shell_get_embed_single (embed_shell);

	return parent_class->constructor (type, n_construct_properties,
					  construct_params);
}

static void
mozilla_embed_class_init (MozillaEmbedClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
     	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass); 
     	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass); 

	parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

	object_class->constructor = mozilla_embed_constructor;
	object_class->finalize = mozilla_embed_finalize;

	gtk_object_class->destroy = mozilla_embed_destroy;

	widget_class->grab_focus = mozilla_embed_grab_focus;
	widget_class->realize = mozilla_embed_realize;

	g_type_class_add_private (object_class, sizeof(MozillaEmbedPrivate));
}

static void
mozilla_embed_init (MozillaEmbed *embed)
{
        embed->priv = MOZILLA_EMBED_GET_PRIVATE (embed);
	embed->priv->browser = new EphyBrowser ();

	g_signal_connect_object (embed, "location",
				 G_CALLBACK (mozilla_embed_location_changed_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "net_state_all",
				 G_CALLBACK (mozilla_embed_net_state_all_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "dom_mouse_click",
				 G_CALLBACK (mozilla_embed_dom_mouse_click_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "dom_mouse_down",
				 G_CALLBACK (mozilla_embed_dom_mouse_down_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "dom-key-press",
				 G_CALLBACK (mozilla_embed_dom_key_press_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "new_window",
				 G_CALLBACK (mozilla_embed_new_window_cb),
				 embed, (GConnectFlags) 0);
	g_signal_connect_object (embed, "security_change",
				 G_CALLBACK (mozilla_embed_security_change_cb),
				 embed, (GConnectFlags) 0);
}

gpointer
_mozilla_embed_get_ephy_browser (MozillaEmbed *embed)
{
	g_return_val_if_fail (embed->priv->browser != NULL, NULL);
	
	return embed->priv->browser;
}

static void
mozilla_embed_destroy (GtkObject *object)
{
	MozillaEmbed *embed = MOZILLA_EMBED (object);

	if (embed->priv->browser)
	{
		embed->priv->browser->Destroy();
	}
	
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
mozilla_embed_finalize (GObject *object)
{
	MozillaEmbed *embed = MOZILLA_EMBED (object);

	if (embed->priv->browser)
	{
        	delete embed->priv->browser;
	       	embed->priv->browser = nsnull;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);

	g_object_unref (embed_shell);
}

static void
impl_load_url (EphyEmbed *embed, 
               const char *url)
{
  gecko_embed_load_url (GECKO_EMBED (embed), url);
}

static char * impl_get_location (EphyEmbed *embed, gboolean toplevel);

static void
impl_load (EphyEmbed *embed, 
           const char *url,
	   EphyEmbedLoadFlags flags,
	   EphyEmbed *preview_embed)
{
	EphyBrowser *browser;

	browser = MOZILLA_EMBED(embed)->priv->browser;
	g_return_if_fail (browser != NULL);

	nsCOMPtr<nsIURI> uri;
	if (preview_embed != NULL)
	{
		EphyBrowser *pbrowser;

		pbrowser = MOZILLA_EMBED(preview_embed)->priv->browser;
		if (pbrowser != NULL)
		{
			pbrowser->GetDocumentURI (getter_AddRefs (uri));
		}
	}

#ifdef HAVE_GECKO_1_8_1
	if (flags & EPHY_EMBED_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP)
	{
		browser->LoadURI (url, nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP, uri);	
	}
	else
#endif /* HAVE_GECKO_1_8_1 */
	{
		browser->LoadURI (url, nsIWebNavigation::LOAD_FLAGS_NONE, uri);	
	}
}

static void
impl_stop_load (EphyEmbed *embed)
{
	gecko_embed_stop_load (GECKO_EMBED(embed));
}

static gboolean
impl_can_go_back (EphyEmbed *embed)
{
	return gecko_embed_can_go_back (GECKO_EMBED(embed));
}

static gboolean
impl_can_go_forward (EphyEmbed *embed)
{
	return gecko_embed_can_go_forward (GECKO_EMBED(embed));
}

static gboolean
mozilla_embed_get_uri_parent (MozillaEmbed *membed,
			      const char *aUri,
			      nsCString &aParent)
{
        nsresult rv;
	nsCString encoding;
	rv = membed->priv->browser->GetEncoding (encoding);
	if (NS_FAILED (rv)) return FALSE;

        nsCOMPtr<nsIURI> uri;
        rv = EphyUtils::NewURI (getter_AddRefs(uri), nsCString(aUri), encoding.get());
        if (NS_FAILED(rv) || !uri) return FALSE;

	/* Don't support going 'up' with chrome url's, mozilla handily
	 * fixes them up for us, so it doesn't work properly, see
	 * rdf/chrome/src/nsChromeProtocolHandler.cpp::NewURI()
	 * (the Canonify() call)
	 */
	nsCString scheme;
	rv = uri->GetScheme (scheme);
	if (NS_FAILED(rv) || !scheme.Length()) return FALSE;
	if (strcmp (scheme.get(), "chrome") == 0) return FALSE;

	nsCString path;
	rv = uri->GetPath(path);
	if (NS_FAILED(rv) || !path.Length()) return FALSE;
	if (strcmp (path.get (), "/") == 0) return FALSE;

	const char *slash = strrchr (path.BeginReading(), '/');
	if (!slash) return FALSE;

	if (slash[1] == '\0')
	{
		/* ends with a slash - a directory, go to parent */
		rv = uri->Resolve (nsCString(".."), aParent);
	}
	else
	{
		/* it's a file, go to the directory */
		rv = uri->Resolve (nsCString("."), aParent);
	}

	return NS_SUCCEEDED (rv);
}

static gboolean
impl_can_go_up (EphyEmbed *embed)
{
	MozillaEmbed *membed = MOZILLA_EMBED (embed);
	char *address;
	gboolean result;

	address = ephy_embed_get_location (embed, TRUE);
	if (address == NULL) return FALSE;

	nsCString parent;
	result = mozilla_embed_get_uri_parent (membed, address, parent);
	g_free (address);

	return result;
}

static GSList *
impl_get_go_up_list (EphyEmbed *embed)
{
	MozillaEmbed *membed = MOZILLA_EMBED (embed);
	GSList *l = NULL;
	char *address, *s;

	address = ephy_embed_get_location (embed, TRUE);
	if (address == NULL) return NULL;

	s = address;
	nsCString parent;
	while (mozilla_embed_get_uri_parent (membed, s, parent))
	{
		s = g_strdup (parent.get());
		l = g_slist_prepend (l, s);
	}

	g_free (address);

	return g_slist_reverse (l);
}

static void
impl_go_back (EphyEmbed *embed)
{
	gecko_embed_go_back (GECKO_EMBED(embed));
}
		
static void
impl_go_forward (EphyEmbed *embed)
{
	gecko_embed_go_forward (GECKO_EMBED(embed));
}

static void
impl_go_up (EphyEmbed *embed)
{
	MozillaEmbed *membed = MOZILLA_EMBED (embed);
	char *uri;

	uri = ephy_embed_get_location (embed, TRUE);
	if (uri == NULL) return;

	gboolean rv;
	nsCString parent_uri;
	rv = mozilla_embed_get_uri_parent (membed, uri, parent_uri);
	g_free (uri);

	g_return_if_fail (rv != FALSE);

	ephy_embed_load_url (embed, parent_uri.get ());
}

static char *
impl_get_title (EphyEmbed *embed)
{
	return gecko_embed_get_title (GECKO_EMBED (embed));
}

static char *
impl_get_link_message (EphyEmbed *embed)
{
	return gecko_embed_get_link_message (GECKO_EMBED (embed));
}

static char *
impl_get_js_status (EphyEmbed *embed)
{
	return gecko_embed_get_js_status (GECKO_EMBED (embed));
}

static char *
impl_get_location (EphyEmbed *embed, 
                   gboolean toplevel)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;

	nsCOMPtr<nsIURI> uri;
	if (toplevel)
	{
		rv = mpriv->browser->GetDocumentURI (getter_AddRefs (uri));
	}
	else
	{
		rv = mpriv->browser->GetTargetDocumentURI (getter_AddRefs (uri));
	}

	if (NS_FAILED (rv)) return NULL;

	nsCOMPtr<nsIURI> furi;
	rv = uri->Clone (getter_AddRefs (furi));
	/* Some nsIURI impls return NS_OK even though they didn't put anything in the outparam!! */
	if (NS_FAILED (rv) || !furi) furi.swap(uri);

	/* Hide password part */
	nsCString user;
	furi->GetUsername (user);
	furi->SetUserPass (user);

	nsCString url;
	furi->GetSpec (url);

	return url.Length() ? g_strdup (url.get()) : NULL;
}

static void
impl_reload (EphyEmbed *embed, 
             gboolean force)
{
	guint32 mflags = GECKO_EMBED_FLAG_RELOADNORMAL;

	if (force)
	{
		mflags = GECKO_EMBED_FLAG_RELOADBYPASSPROXYANDCACHE;
	}

	gecko_embed_reload (GECKO_EMBED(embed), mflags);
}

static void
impl_set_zoom (EphyEmbed *embed, 
               float zoom) 
{
	EphyBrowser *browser;
	nsresult rv;

	g_return_if_fail (zoom > 0.0);

	browser = MOZILLA_EMBED(embed)->priv->browser;
	g_return_if_fail (browser != NULL);

	rv = browser->SetZoom (zoom);

	if (NS_SUCCEEDED (rv))
	{
		g_signal_emit_by_name (embed, "ge_zoom_change", zoom);
	}
}

static float
impl_get_zoom (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	float f;

	nsresult rv;	
	rv = mpriv->browser->GetZoom (&f);
	
	if (NS_SUCCEEDED (rv))
	{
		return f;
	}

	return 1.0;
}

static void
impl_scroll_lines (EphyEmbed *embed,
		   int num_lines)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->ScrollLines (num_lines);
}

static void
impl_scroll_pages (EphyEmbed *embed,
		   int num_pages)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->ScrollPages (num_pages);
}

static void
impl_scroll_pixels (EphyEmbed *embed,
		    int dx,
		    int dy)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->ScrollPixels (dx, dy);
}

static int
impl_shistory_n_items (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	int count, index;

	rv = mpriv->browser->GetSHInfo (&count, &index);

	return NS_SUCCEEDED(rv) ? count : 0;
}

static void
impl_shistory_get_nth (EphyEmbed *embed, 
                       int nth,
                       gboolean is_relative,
                       char **aUrl,
                       char **aTitle)
{
	nsresult rv;
        nsCString url;
	PRUnichar *title;
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	if (is_relative)
	{
		nth += ephy_embed_shistory_get_pos (embed);
	}
	
        rv = mpriv->browser->GetSHUrlAtIndex(nth, url);

        *aUrl = (NS_SUCCEEDED (rv) && url.Length()) ? g_strdup(url.get()) : NULL;

	rv = mpriv->browser->GetSHTitleAtIndex(nth, &title);

	if (title)
	{
		nsCString cTitle;
		NS_UTF16ToCString (nsString(title),
				   NS_CSTRING_ENCODING_UTF8, cTitle);
		*aTitle = g_strdup (cTitle.get());
		nsMemory::Free (title);
	}
	else
	{
		*aTitle = NULL;
	}
}

static int
impl_shistory_get_pos (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	int count, index;

	rv = mpriv->browser->GetSHInfo (&count, &index);

	return NS_SUCCEEDED(rv) ? index : 0;
}

static void
impl_shistory_go_nth (EphyEmbed *embed, 
                      int nth)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->GoToHistoryIndex (nth);
}

static void
impl_shistory_copy (EphyEmbed *source,
		    EphyEmbed *dest,
		    gboolean copy_back,
		    gboolean copy_forward,
		    gboolean copy_current)
{
	MozillaEmbedPrivate *spriv = MOZILLA_EMBED(source)->priv;
	MozillaEmbedPrivate *dpriv = MOZILLA_EMBED(dest)->priv;

	spriv->browser->CopySHistory(dpriv->browser, copy_back,
				     copy_forward, copy_current);
}

static void
impl_get_security_level (EphyEmbed *embed,
                         EphyEmbedSecurityLevel *level,
                         char **description)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (embed)->priv;

	if (level) *level = EPHY_EMBED_STATE_IS_UNKNOWN;
	if (description) *description = NULL;

	nsresult rv;
	PRUint32 state;
	nsCString desc;
	rv = mpriv->browser->GetSecurityInfo (&state, desc);
	if (NS_FAILED (rv)) return;

	if (level) *level = mozilla_embed_security_level (state);
	if (description) *description = g_strdup (desc.get());
}

static void
impl_show_page_certificate (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED (embed)->priv;

	mpriv->browser->ShowCertificate ();
}
	
static void
impl_print (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
 
	mpriv->browser->Print ();
}

static void
impl_set_print_preview_mode (EphyEmbed *embed, gboolean preview_mode)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->SetPrintPreviewMode (preview_mode);
}

static int
impl_print_preview_n_pages (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	int num;

	rv = mpriv->browser->PrintPreviewNumPages(&num);

	return NS_SUCCEEDED (rv) ? num : 0;
}

static void
impl_print_preview_navigate (EphyEmbed *embed,
			     EphyEmbedPrintPreviewNavType type,
			     int page)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;

	mpriv->browser->PrintPreviewNavigate(type, page);
}

static void
impl_set_encoding (EphyEmbed *embed,
		   const char *encoding)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	nsCString currEnc;

	g_return_if_fail (encoding != NULL);

	rv = mpriv->browser->GetEncoding (currEnc);
	if (NS_FAILED (rv)) return;

	if (strcmp (currEnc.get(), encoding) != 0 ||
	    encoding[0] == '\0' && !ephy_embed_has_automatic_encoding (embed))
	{
		rv = mpriv->browser->ForceEncoding (encoding);
		if (NS_FAILED (rv)) return;
	}

	gecko_embed_reload (GECKO_EMBED (embed),
			    GECKO_EMBED_FLAG_RELOADCHARSETCHANGE);
}

static char *
impl_get_encoding (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	nsCString encoding;

	rv = mpriv->browser->GetEncoding (encoding);

	if (NS_FAILED (rv) || !encoding.Length())
	{
		return NULL;
	}

	return g_strdup (encoding.get());
}

static gboolean
impl_has_automatic_encoding (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;
	nsCString encoding;

	rv = mpriv->browser->GetForcedEncoding (encoding);

	if (NS_FAILED (rv) || !encoding.Length())
	{
		return TRUE;
	}

	return FALSE;
}

static gboolean
impl_has_modified_forms (EphyEmbed *embed)
{
	MozillaEmbedPrivate *mpriv = MOZILLA_EMBED(embed)->priv;
	nsresult rv;

	PRBool modified;
	rv = mpriv->browser->GetHasModifiedForms (&modified);

	return NS_SUCCEEDED (rv) ? modified : FALSE;
}

static void
mozilla_embed_location_changed_cb (GeckoEmbed *embed,
				   MozillaEmbed *membed)
{
	char *location;

	location = gecko_embed_get_location (embed);
	g_signal_emit_by_name (membed, "ge-location", location);
	g_free (location);
}

static void
update_load_state (MozillaEmbed *membed, gint state)
{
	MozillaEmbedPrivate *priv = membed->priv;

	if (state & GECKO_EMBED_FLAG_IS_DOCUMENT &&
	    state & (GECKO_EMBED_FLAG_START | GECKO_EMBED_FLAG_STOP))
	{
		g_signal_emit_by_name (membed, "ge-document-type",
				       priv->browser->GetDocumentType ());
	}

	if (state & GECKO_EMBED_FLAG_RESTORING &&
	    priv->load_state == MOZILLA_EMBED_LOAD_STARTED)
	{
		priv->load_state = MOZILLA_EMBED_LOAD_LOADING;

		char *address;
		address = gecko_embed_get_location (GECKO_EMBED (membed));
		g_signal_emit_by_name (membed, "ge-content-change", address);
		g_free (address);
	}

	if (state & GECKO_EMBED_FLAG_IS_NETWORK)
	{
		if (state & GECKO_EMBED_FLAG_START)
		{
			priv->load_state = MOZILLA_EMBED_LOAD_STARTED;
		}
		else if (state & GECKO_EMBED_FLAG_STOP)
		{
			priv->load_state = MOZILLA_EMBED_LOAD_STOPPED;
		}
	}
	else if (state & GECKO_EMBED_FLAG_START &&
	         state & GECKO_EMBED_FLAG_IS_REQUEST)
	{
		if (priv->load_state == MOZILLA_EMBED_LOAD_REDIRECTING)
		{
			priv->load_state = MOZILLA_EMBED_LOAD_STARTED;
		}
		else if (priv->load_state != MOZILLA_EMBED_LOAD_LOADING)
		{
			priv->load_state = MOZILLA_EMBED_LOAD_LOADING;

			char *address;
			address = gecko_embed_get_location (GECKO_EMBED (membed));
			g_signal_emit_by_name (membed, "ge_content_change", address);
			g_free (address);
		}
	}
	else if (state & GECKO_EMBED_FLAG_REDIRECTING &&
	         priv->load_state == MOZILLA_EMBED_LOAD_STARTED)
	{
		priv->load_state = MOZILLA_EMBED_LOAD_REDIRECTING;
	}
}

static void
mozilla_embed_net_state_all_cb (GeckoEmbed *embed, const char *aURI,
                                gint state, guint status, 
				MozillaEmbed *membed)
{
	EphyEmbedNetState estate = EPHY_EMBED_STATE_UNKNOWN;
	int i;

	struct
	{
		guint state;
		EphyEmbedNetState embed_state;
	}
	conversion_map [] =
	{
		{ GECKO_EMBED_FLAG_START, EPHY_EMBED_STATE_START },
		{ GECKO_EMBED_FLAG_STOP, EPHY_EMBED_STATE_STOP },
		{ GECKO_EMBED_FLAG_REDIRECTING, EPHY_EMBED_STATE_REDIRECTING },
		{ GECKO_EMBED_FLAG_TRANSFERRING, EPHY_EMBED_STATE_TRANSFERRING },
		{ GECKO_EMBED_FLAG_NEGOTIATING, EPHY_EMBED_STATE_NEGOTIATING },
		{ GECKO_EMBED_FLAG_IS_REQUEST, EPHY_EMBED_STATE_IS_REQUEST },
		{ GECKO_EMBED_FLAG_IS_DOCUMENT, EPHY_EMBED_STATE_IS_DOCUMENT },
		{ GECKO_EMBED_FLAG_IS_NETWORK, EPHY_EMBED_STATE_IS_NETWORK },
		{ GECKO_EMBED_FLAG_RESTORING, EPHY_EMBED_STATE_RESTORING },
		{ 0, EPHY_EMBED_STATE_UNKNOWN }
	};

	for (i = 0; conversion_map[i].state != 0; i++)
	{
		if (state & conversion_map[i].state)
		{
			estate = (EphyEmbedNetState) (estate | conversion_map[i].embed_state);	
		}
	}

	update_load_state (membed, state);
	
	g_signal_emit_by_name (membed, "ge_net_state", aURI, /* FIXME: (gulong) */ estate);
}

static gboolean
mozilla_embed_emit_mouse_signal (MozillaEmbed *embed,
				 GeckoDOMEvent *dom_event,
				 const char *signal_name)
{
	MozillaEmbedPrivate *mpriv = embed->priv;
	MozillaEmbedEvent *info;
	EventContext event_context;
	gint return_value = FALSE;
	nsresult rv;

	if (dom_event == NULL) return FALSE;

        nsCOMPtr<nsIDOMEvent> domEvent (gecko_dom_event_get_I (dom_event));
	nsCOMPtr<nsIDOMMouseEvent> ev (do_QueryInterface (domEvent));
	NS_ENSURE_TRUE (ev, FALSE);
	nsCOMPtr<nsIDOMEvent> dev = do_QueryInterface (ev);
	NS_ENSURE_TRUE (dev, FALSE);

	info = mozilla_embed_event_new (static_cast<gpointer>(dev));

	event_context.Init (mpriv->browser);
        rv = event_context.GetMouseEventInfo (ev, MOZILLA_EMBED_EVENT (info));
	if (NS_FAILED (rv))
	{
		g_object_unref (info);
		return FALSE;
	}

	nsCOMPtr<nsIDOMDocument> domDoc;
	rv = event_context.GetTargetDocument (getter_AddRefs(domDoc));
	if (NS_SUCCEEDED (rv))
	{
		mpriv->browser->PushTargetDocument (domDoc);

		g_signal_emit_by_name (embed, signal_name, 
				       info, &return_value); 
		mpriv->browser->PopTargetDocument ();
	}

	g_object_unref (info);

	return return_value;
}

static gboolean
mozilla_embed_dom_mouse_click_cb (GeckoEmbed *embed,
				  GeckoDOMEvent *dom_event,
				  MozillaEmbed *membed)
{
	return mozilla_embed_emit_mouse_signal (membed, dom_event,
						"ge_dom_mouse_click");
}

static gboolean
mozilla_embed_dom_mouse_down_cb (GeckoEmbed *embed,
			         GeckoDOMEvent *dom_event,
				 MozillaEmbed *membed)
{
	return mozilla_embed_emit_mouse_signal (membed, dom_event,
						"ge_dom_mouse_down");
}

static gint
mozilla_embed_dom_key_press_cb (GeckoEmbed *embed,
				GeckoDOMEvent *dom_event,
				MozillaEmbed *membed)
{
	gint retval = FALSE;

	if (dom_event == NULL) return FALSE;

        nsCOMPtr<nsIDOMEvent> domEvent (gecko_dom_event_get_I (dom_event));
	nsCOMPtr<nsIDOMKeyEvent> ev (do_QueryInterface (domEvent));
	NS_ENSURE_TRUE (ev, FALSE);

	if (!EventContext::CheckKeyPress (ev)) return FALSE;

	GdkEvent *event = gtk_get_current_event ();
	if (event == NULL) return FALSE; /* shouldn't happen! */

	g_return_val_if_fail (GDK_KEY_PRESS == event->type, FALSE);

	g_signal_emit_by_name (embed, "ge-search-key-press", event, &retval);

	gdk_event_free (event);

	return retval;
}

EphyEmbedChrome
_mozilla_embed_translate_chrome (GeckoEmbedChromeFlags flags)
{
	static const struct
	{
		guint mozilla_flag;
		guint ephy_flag;
	}
	conversion_map [] =
	{
		{ GECKO_EMBED_FLAG_MENUBARON, EPHY_EMBED_CHROME_MENUBAR },
		{ GECKO_EMBED_FLAG_TOOLBARON, EPHY_EMBED_CHROME_TOOLBAR },
		{ GECKO_EMBED_FLAG_STATUSBARON, EPHY_EMBED_CHROME_STATUSBAR },
		{ GECKO_EMBED_FLAG_PERSONALTOOLBARON, EPHY_EMBED_CHROME_BOOKMARKSBAR },
	};

	guint mask = 0, i;

	for (i = 0; i < G_N_ELEMENTS (conversion_map); i++)
	{
		if (flags & conversion_map[i].mozilla_flag)
		{
			mask |= conversion_map[i].ephy_flag;
		}
	}

	return (EphyEmbedChrome) mask;
}

static void
mozilla_embed_new_window_cb (GeckoEmbed *embed,
			     GeckoEmbed **newEmbed,
			     guint chrome_mask, 
			     MozillaEmbed *membed)
{
	GeckoEmbedChromeFlags chrome = (GeckoEmbedChromeFlags) chrome_mask;
	EphyEmbed *new_embed = NULL;
	GObject *single;
	EphyEmbedChrome mask;

	if (chrome & GECKO_EMBED_FLAG_OPENASCHROME)
	{
		*newEmbed = _mozilla_embed_new_xul_dialog ();
		return;
	}

	mask = _mozilla_embed_translate_chrome (chrome);

	single = ephy_embed_shell_get_embed_single (embed_shell);
	g_signal_emit_by_name (single, "new-window", embed, mask,
			       &new_embed);

	g_assert (new_embed != NULL);

	gecko_embed_set_chrome_mask (GECKO_EMBED (new_embed), chrome);

	g_signal_emit_by_name (membed, "ge-new-window", new_embed);

	*newEmbed = GECKO_EMBED (new_embed);
}

static void
mozilla_embed_security_change_cb (GeckoEmbed *embed,
				  gpointer requestptr,
				  PRUint32 state,
				  MozillaEmbed *membed)
{
	g_signal_emit_by_name (membed, "ge_security_change",
			       mozilla_embed_security_level (state));
}

static EphyEmbedSecurityLevel
mozilla_embed_security_level (PRUint32 state)
{
	EphyEmbedSecurityLevel level;

	switch (state)
        {
        case nsIWebProgressListener::STATE_IS_INSECURE:
                level = EPHY_EMBED_STATE_IS_INSECURE;
                break;
        case nsIWebProgressListener::STATE_IS_BROKEN:
                level = EPHY_EMBED_STATE_IS_BROKEN;
                break;
        case nsIWebProgressListener::STATE_IS_SECURE|
             nsIWebProgressListener::STATE_SECURE_HIGH:
                level = EPHY_EMBED_STATE_IS_SECURE_HIGH;
                break;
        case nsIWebProgressListener::STATE_IS_SECURE|
             nsIWebProgressListener::STATE_SECURE_MED:
                level = EPHY_EMBED_STATE_IS_SECURE_MED;
                break;
        case nsIWebProgressListener::STATE_IS_SECURE|
             nsIWebProgressListener::STATE_SECURE_LOW:
                level = EPHY_EMBED_STATE_IS_SECURE_LOW;
                break;
        default:
                level = EPHY_EMBED_STATE_IS_UNKNOWN;
                break;
        }
	return level;
}

static void
ephy_embed_iface_init (EphyEmbedIface *iface)
{
	iface->load_url = impl_load_url; 
	iface->load = impl_load; 
	iface->stop_load = impl_stop_load;
	iface->can_go_back = impl_can_go_back;
	iface->can_go_forward =impl_can_go_forward;
	iface->can_go_up = impl_can_go_up;
	iface->get_go_up_list = impl_get_go_up_list;
	iface->go_back = impl_go_back;
	iface->go_forward = impl_go_forward;
	iface->go_up = impl_go_up;
	iface->get_title = impl_get_title;
	iface->get_location = impl_get_location;
	iface->get_link_message = impl_get_link_message;
	iface->get_js_status = impl_get_js_status;
	iface->reload = impl_reload;
	iface->set_zoom = impl_set_zoom;
	iface->get_zoom = impl_get_zoom;
	iface->scroll_lines = impl_scroll_lines;
	iface->scroll_pages = impl_scroll_pages;
	iface->scroll_pixels = impl_scroll_pixels;
	iface->shistory_n_items = impl_shistory_n_items;
	iface->shistory_get_nth = impl_shistory_get_nth;
	iface->shistory_get_pos = impl_shistory_get_pos;
	iface->shistory_go_nth = impl_shistory_go_nth;
	iface->shistory_copy = impl_shistory_copy;
	iface->get_security_level = impl_get_security_level;
	iface->show_page_certificate = impl_show_page_certificate;
	iface->close = impl_close;
	iface->set_encoding = impl_set_encoding;
	iface->get_encoding = impl_get_encoding;
	iface->has_automatic_encoding = impl_has_automatic_encoding;
	iface->print = impl_print;
	iface->set_print_preview_mode = impl_set_print_preview_mode;
	iface->print_preview_n_pages = impl_print_preview_n_pages;
	iface->print_preview_navigate = impl_print_preview_navigate;
	iface->has_modified_forms = impl_has_modified_forms;
}

static void
xul_visibility_cb (GtkWidget *embed, gboolean visibility, GtkWidget *window)
{
	if (visibility)
	{
		gtk_widget_show (window);
	}
	else
	{
		gtk_widget_hide (window);
	}
}

static void
xul_size_to_cb (GtkWidget *embed, gint width, gint height, gpointer dummy)
{
	gtk_widget_set_size_request (embed, width, height);
}

static void
xul_new_window_cb (GeckoEmbed *embed,
		   GeckoEmbed **retval,
		   guint chrome_mask,
		   gpointer dummy)
{
        g_assert (chrome_mask & GECKO_EMBED_FLAG_OPENASCHROME);

        *retval = _mozilla_embed_new_xul_dialog ();
}

static void
xul_title_cb (GeckoEmbed *embed,
	      GtkWindow *window)
{
	char *title;

	title = gecko_embed_get_title (embed);
	gtk_window_set_title (window, title);
	g_free (title);
}

GeckoEmbed *
_mozilla_embed_new_xul_dialog (void)
{
	GtkWidget *window, *embed;

	g_object_ref (embed_shell);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_object_set_data_full (G_OBJECT (window), "EmbedShellRef",
			        embed_shell,
				(GDestroyNotify) g_object_unref);
	g_signal_connect_object (embed_shell, "prepare_close",
				 G_CALLBACK (gtk_widget_destroy), window,
				 (GConnectFlags) G_CONNECT_SWAPPED);

	embed = gecko_embed_new ();
	gtk_widget_show (embed);
	gtk_container_add (GTK_CONTAINER (window), embed);

	g_signal_connect_object (embed, "destroy_browser",
				 G_CALLBACK (gtk_widget_destroy),
				 window, G_CONNECT_SWAPPED);
	g_signal_connect_object (embed, "visibility",
				 G_CALLBACK (xul_visibility_cb),
				 window, (GConnectFlags) 0);
	g_signal_connect_object (embed, "size_to",
				 G_CALLBACK (xul_size_to_cb),
				 NULL, (GConnectFlags) 0);
	g_signal_connect_object (embed, "new_window",
				 G_CALLBACK (xul_new_window_cb),
				 NULL, (GConnectFlags) 0);
	g_signal_connect_object (embed, "title",
				 G_CALLBACK (xul_title_cb),
				 window, (GConnectFlags) 0);

	return GECKO_EMBED (embed);
}
