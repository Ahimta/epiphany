/*
 *  GtkNSSKeyPairDialogs.cpp
 *
 *  Copyright (C) 2003 Crispin Flowerday <gnome@flowerday.cx>
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

/*
 * This file provides Gtk implementations of the mozilla Generating Key Pair
 * dialogs.
 *
 * This implementation takes some liberties with the mozilla API. Although the
 * API requires a nsIDomWindowInternal, it only actually calls the Close()
 * function on that class. Therefore we provide a dummy class that only 
 * implements that function (it just sets a flag). 
 *
 * Periodically we check to see whether the dialog should have been closed. If
 * it should be closed, then the key generation has finished, so close the dialog
 * (using gtk_dialog_response), and return.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MOZILLA_PSM

#include "mozilla-version.h"

#include "MozillaPrivate.h"

#include <nsIServiceManager.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIKeygenThread.h>
#include <nsIDOMWindow.h>

#ifdef ALLOW_PRIVATE_API
#include "nsIDOMWindowInternal.h"
#endif

#include "gtk/gtkdialog.h"
#include "gtk/gtkprogressbar.h"
#include "gtk/gtkimage.h"
#include "gtk/gtkstock.h"
#include "gtk/gtklabel.h"
#include "gtk/gtkhbox.h"
#include "gtk/gtkvbox.h"
#include "gtk/gtkmain.h"

#include <libgnome/gnome-i18n.h>

#include "GtkNSSKeyPairDialogs.h"
#include "ephy-debug.h"

GtkNSSKeyPairDialogs::GtkNSSKeyPairDialogs ()
{
	LOG ("GtkNSSKeyPairDialogs ctor (%p)", this)
}

GtkNSSKeyPairDialogs::~GtkNSSKeyPairDialogs ()
{
	LOG ("GtkNSSKeyPairDialogs dtor (%p)", this)
}

NS_IMPL_ISUPPORTS1 (GtkNSSKeyPairDialogs, 
		    nsIGeneratingKeypairInfoDialogs)


/* ------------------------------------------------------------
 * A dummy implementation of nsIDomWindowInternal so that
 * we can use the correctly get callbacks from the
 * nsIKeygenThread */
class KeyPairHelperWindow : public nsIDOMWindowInternal
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIDOMWINDOWINTERNAL
	NS_DECL_NSIDOMWINDOW
#if (MOZILLA_CHECK_VERSION4 (1, 7, MOZILLA_RC, 3) && !MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)) || MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 2)
	NS_DECL_NSIDOMWINDOW2
#endif

	KeyPairHelperWindow();
	virtual ~KeyPairHelperWindow();

	gboolean close_called;
};

#if (MOZILLA_CHECK_VERSION4 (1, 7, MOZILLA_RC, 3) && !MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)) || MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 2)
NS_IMPL_ISUPPORTS3(KeyPairHelperWindow, nsIDOMWindowInternal, nsIDOMWindow, nsIDOMWindow2)
#else
NS_IMPL_ISUPPORTS2(KeyPairHelperWindow, nsIDOMWindowInternal, nsIDOMWindow)
#endif

KeyPairHelperWindow::KeyPairHelperWindow()
{
	close_called = FALSE;
}

NS_IMETHODIMP KeyPairHelperWindow::Close()
{
	/* This is called in a different thread, so just set a flag, dont
	 * call the dialog_response directly */
	close_called = TRUE;
	return NS_OK;
}

/* ------------------------------------------------------------ */
static void
begin_busy (GtkWidget *widget)
{
	static GdkCursor *cursor = NULL;

	if (cursor == NULL) cursor = gdk_cursor_new (GDK_WATCH);

	if (!GTK_WIDGET_REALIZED (widget)) gtk_widget_realize (GTK_WIDGET(widget));

	gdk_window_set_cursor (GTK_WIDGET (widget)->window, cursor);
	while (gtk_events_pending ()) gtk_main_iteration ();
}

static void
end_busy (GtkWidget *widget)
{
	gdk_window_set_cursor (GTK_WIDGET(widget)->window, NULL);
}


struct KeyPairInfo
{
	GtkWidget *progress;
	GtkWidget *dialog;
	KeyPairHelperWindow *helper;
};


static gboolean
generating_timeout_cb (KeyPairInfo *info)
{
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (info->progress));

	if (info->helper->close_called)
	{
		gtk_dialog_response (GTK_DIALOG (info->dialog), GTK_RESPONSE_OK);
	}
	return TRUE;
}


/* void displayGeneratingKeypairInfo (in nsIInterfaceRequestor ctx, in nsIKeygenTh
read runnable); */
NS_IMETHODIMP
GtkNSSKeyPairDialogs::DisplayGeneratingKeypairInfo (nsIInterfaceRequestor *ctx,
						    nsIKeygenThread *runnable)
{
	GtkWidget *dialog, *progress, *label, *vbox;
	gint timeout_id;


	nsCOMPtr<nsIDOMWindow> parent = do_GetInterface (ctx);
	GtkWidget *gparent = MozillaFindGtkParent (parent);

	dialog = gtk_dialog_new_with_buttons ("", GTK_WINDOW (gparent),
					      GTK_DIALOG_NO_SEPARATOR, NULL);

	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox, TRUE, TRUE, 0);

	label = gtk_label_new (NULL);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	
	char *msg = g_strdup_printf ("<span weight=\"bold\" size=\"larger\">%s</span>\n\n%s",
				     _("Generating Private Key."),
				     _("Please wait while a new private key is "
				       "generated. This process could take a few minutes." ));
	gtk_label_set_markup (GTK_LABEL(label), msg);
	g_free (msg);

	progress = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (vbox), progress, TRUE, TRUE, 0);

	/* Create a helper class that just waits for close events
	 * from the other thread */
	KeyPairHelperWindow *helper = new KeyPairHelperWindow;

	KeyPairInfo callback_data = { progress, dialog, helper };
	timeout_id = g_timeout_add (100, (GSourceFunc)generating_timeout_cb, &callback_data);

	gtk_widget_show_all (dialog);
	gtk_widget_hide (GTK_DIALOG (dialog)->action_area);

	begin_busy (dialog);
	runnable->StartKeyGeneration (helper);
	int res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res != GTK_RESPONSE_OK && helper->close_called == FALSE)
	{
		/* Ignore the already_closed flag, our nsIDOMWindowInterna::Close
		 * function just sets a flag, it doesn't close the window, so we
		 * dont have a race condition */
		PRBool already_closed = FALSE;
		runnable->UserCanceled (&already_closed);
	}

	g_source_remove (timeout_id);
	end_busy (dialog);
	gtk_widget_destroy (dialog);
	delete helper;
	return NS_OK;
}


/*************************************************************
 * Misc functions for the nsIDomWindowInternal implementation
 * that arn't needed for our purposes
 *************************************************************/

#define MOZ_NOT_IMPLEMENTED { g_warning ("not implemented: " G_STRLOC); \
                              return NS_ERROR_NOT_IMPLEMENTED; }

KeyPairHelperWindow::~KeyPairHelperWindow()
{
}

/* readonly attribute nsIDOMWindowInternal window; */
NS_IMETHODIMP KeyPairHelperWindow::GetWindow(nsIDOMWindowInternal * *aWindow)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMWindowInternal self; */
NS_IMETHODIMP KeyPairHelperWindow::GetSelf(nsIDOMWindowInternal * *aSelf)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMNavigator navigator; */
NS_IMETHODIMP KeyPairHelperWindow::GetNavigator(nsIDOMNavigator * *aNavigator)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMScreen screen; */
NS_IMETHODIMP KeyPairHelperWindow::GetScreen(nsIDOMScreen * *aScreen)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMHistory history; */
NS_IMETHODIMP KeyPairHelperWindow::GetHistory(nsIDOMHistory * *aHistory)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMWindow content; */
NS_IMETHODIMP KeyPairHelperWindow::GetContent(nsIDOMWindow * *aContent)
{
    MOZ_NOT_IMPLEMENTED
}

/* [noscript] readonly attribute nsIPrompt prompter; */
NS_IMETHODIMP KeyPairHelperWindow::GetPrompter(nsIPrompt * *aPrompter)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMBarProp menubar; */
NS_IMETHODIMP KeyPairHelperWindow::GetMenubar(nsIDOMBarProp * *aMenubar)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMBarProp toolbar; */
NS_IMETHODIMP KeyPairHelperWindow::GetToolbar(nsIDOMBarProp * *aToolbar)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMBarProp locationbar; */
NS_IMETHODIMP KeyPairHelperWindow::GetLocationbar(nsIDOMBarProp * *aLocationbar)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMBarProp personalbar; */
NS_IMETHODIMP KeyPairHelperWindow::GetPersonalbar(nsIDOMBarProp * *aPersonalbar)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMBarProp statusbar; */
NS_IMETHODIMP KeyPairHelperWindow::GetStatusbar(nsIDOMBarProp * *aStatusbar)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMBarProp directories; */
NS_IMETHODIMP KeyPairHelperWindow::GetDirectories(nsIDOMBarProp * *aDirectories)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute boolean closed; */
NS_IMETHODIMP KeyPairHelperWindow::GetClosed(PRBool *aClosed)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMCrypto crypto; */
NS_IMETHODIMP KeyPairHelperWindow::GetCrypto(nsIDOMCrypto * *aCrypto)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMPkcs11 pkcs11; */
NS_IMETHODIMP KeyPairHelperWindow::GetPkcs11(nsIDOMPkcs11 * *aPkcs11)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIControllers controllers; */
NS_IMETHODIMP KeyPairHelperWindow::GetControllers(nsIControllers * *aControllers)
{
    MOZ_NOT_IMPLEMENTED
}

/* attribute nsIDOMWindowInternal opener; */
NS_IMETHODIMP KeyPairHelperWindow::GetOpener(nsIDOMWindowInternal * *aOpener)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetOpener(nsIDOMWindowInternal * aOpener)
{
    MOZ_NOT_IMPLEMENTED
}

/* attribute DOMString status; */
NS_IMETHODIMP KeyPairHelperWindow::GetStatus(nsAString & aStatus)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetStatus(const nsAString & aStatus)
{
    MOZ_NOT_IMPLEMENTED
}

/* attribute DOMString defaultStatus; */
NS_IMETHODIMP KeyPairHelperWindow::GetDefaultStatus(nsAString & aDefaultStatus)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetDefaultStatus(const nsAString & aDefaultStatus)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMLocation location; */
NS_IMETHODIMP KeyPairHelperWindow::GetLocation(nsIDOMLocation * *aLocation)
{
    MOZ_NOT_IMPLEMENTED
}

/* attribute long innerWidth; */
NS_IMETHODIMP KeyPairHelperWindow::GetInnerWidth(PRInt32 *aInnerWidth)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetInnerWidth(PRInt32 aInnerWidth)
{
    MOZ_NOT_IMPLEMENTED
}

/* attribute long innerHeight; */
NS_IMETHODIMP KeyPairHelperWindow::GetInnerHeight(PRInt32 *aInnerHeight)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetInnerHeight(PRInt32 aInnerHeight)
{
    MOZ_NOT_IMPLEMENTED
}

/* attribute long outerWidth; */
NS_IMETHODIMP KeyPairHelperWindow::GetOuterWidth(PRInt32 *aOuterWidth)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetOuterWidth(PRInt32 aOuterWidth)
{
    MOZ_NOT_IMPLEMENTED
}

/* attribute long outerHeight; */
NS_IMETHODIMP KeyPairHelperWindow::GetOuterHeight(PRInt32 *aOuterHeight)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetOuterHeight(PRInt32 aOuterHeight)
{
    MOZ_NOT_IMPLEMENTED
}

/* attribute long screenX; */
NS_IMETHODIMP KeyPairHelperWindow::GetScreenX(PRInt32 *aScreenX)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetScreenX(PRInt32 aScreenX)
{
    MOZ_NOT_IMPLEMENTED
}

/* attribute long screenY; */
NS_IMETHODIMP KeyPairHelperWindow::GetScreenY(PRInt32 *aScreenY)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetScreenY(PRInt32 aScreenY)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute long pageXOffset; */
NS_IMETHODIMP KeyPairHelperWindow::GetPageXOffset(PRInt32 *aPageXOffset)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute long pageYOffset; */
NS_IMETHODIMP KeyPairHelperWindow::GetPageYOffset(PRInt32 *aPageYOffset)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute long scrollMaxX; */
NS_IMETHODIMP KeyPairHelperWindow::GetScrollMaxX(PRInt32 *aScrollMaxX)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute long scrollMaxY; */
NS_IMETHODIMP KeyPairHelperWindow::GetScrollMaxY(PRInt32 *aScrollMaxY)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute unsigned long length; */
NS_IMETHODIMP KeyPairHelperWindow::GetLength(PRUint32 *aLength)
{
    MOZ_NOT_IMPLEMENTED
}

/* attribute boolean fullScreen; */
NS_IMETHODIMP KeyPairHelperWindow::GetFullScreen(PRBool *aFullScreen)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetFullScreen(PRBool aFullScreen)
{
    MOZ_NOT_IMPLEMENTED
}

/* void alert (in DOMString text); */
NS_IMETHODIMP KeyPairHelperWindow::Alert(const nsAString & text)
{
    MOZ_NOT_IMPLEMENTED
}

/* boolean confirm (in DOMString text); */
NS_IMETHODIMP KeyPairHelperWindow::Confirm(const nsAString & text, PRBool *_retval)
{
    MOZ_NOT_IMPLEMENTED
}

/* DOMString prompt (in DOMString aMessage, in DOMString aInitial, in DOMString aTitle, in unsigned long aSavePassword); */
NS_IMETHODIMP KeyPairHelperWindow::Prompt(const nsAString & aMessage, const nsAString & aInitial, const nsAString & aTitle, PRUint32 aSavePassword, nsAString & _retval)
{
    MOZ_NOT_IMPLEMENTED
}

/* void focus (); */
NS_IMETHODIMP KeyPairHelperWindow::Focus()
{
    MOZ_NOT_IMPLEMENTED
}

/* void blur (); */
NS_IMETHODIMP KeyPairHelperWindow::Blur()
{
    MOZ_NOT_IMPLEMENTED
}

/* void back (); */
NS_IMETHODIMP KeyPairHelperWindow::Back()
{
    MOZ_NOT_IMPLEMENTED
}

/* void forward (); */
NS_IMETHODIMP KeyPairHelperWindow::Forward()
{
    MOZ_NOT_IMPLEMENTED
}

/* void home (); */
NS_IMETHODIMP KeyPairHelperWindow::Home()
{
    MOZ_NOT_IMPLEMENTED
}

/* void stop (); */
NS_IMETHODIMP KeyPairHelperWindow::Stop()
{
    MOZ_NOT_IMPLEMENTED
}

/* void print (); */
NS_IMETHODIMP KeyPairHelperWindow::Print()
{
    MOZ_NOT_IMPLEMENTED
}

/* void moveTo (in long xPos, in long yPos); */
NS_IMETHODIMP KeyPairHelperWindow::MoveTo(PRInt32 xPos, PRInt32 yPos)
{
    MOZ_NOT_IMPLEMENTED
}

/* void moveBy (in long xDif, in long yDif); */
NS_IMETHODIMP KeyPairHelperWindow::MoveBy(PRInt32 xDif, PRInt32 yDif)
{
    MOZ_NOT_IMPLEMENTED
}

/* void resizeTo (in long width, in long height); */
NS_IMETHODIMP KeyPairHelperWindow::ResizeTo(PRInt32 width, PRInt32 height)
{
    MOZ_NOT_IMPLEMENTED
}

/* void resizeBy (in long widthDif, in long heightDif); */
NS_IMETHODIMP KeyPairHelperWindow::ResizeBy(PRInt32 widthDif, PRInt32 heightDif)
{
    MOZ_NOT_IMPLEMENTED
}

/* void scroll (in long xScroll, in long yScroll); */
NS_IMETHODIMP KeyPairHelperWindow::Scroll(PRInt32 xScroll, PRInt32 yScroll)
{
    MOZ_NOT_IMPLEMENTED
}

/* [noscript] nsIDOMWindow open (in DOMString url, in DOMString name, in DOMString options); */
NS_IMETHODIMP KeyPairHelperWindow::Open(const nsAString & url, const nsAString & name, const nsAString & options, nsIDOMWindow **_retval)
{
    MOZ_NOT_IMPLEMENTED
}

/* [noscript] nsIDOMWindow openDialog (in DOMString url, in DOMString name, in DOMString options, in nsISupports aExtraArgument); */
NS_IMETHODIMP KeyPairHelperWindow::OpenDialog(const nsAString & url, const nsAString & name, const nsAString & options, nsISupports *aExtraArgument, nsIDOMWindow **_retval)
{
    MOZ_NOT_IMPLEMENTED
}

/* void updateCommands (in DOMString action); */
NS_IMETHODIMP KeyPairHelperWindow::UpdateCommands(const nsAString & action)
{
    MOZ_NOT_IMPLEMENTED
}

/* [noscript] boolean find (in DOMString str, in boolean caseSensitive, in boolean backwards, in boolean wrapAround, in boolean wholeWord, in boolean searchInFrames, in boolean showDialog); */
NS_IMETHODIMP KeyPairHelperWindow::Find(const nsAString & str, PRBool caseSensitive, PRBool backwards, PRBool wrapAround, PRBool wholeWord, PRBool searchInFrames, PRBool showDialog, PRBool *_retval)
{
    MOZ_NOT_IMPLEMENTED
}

/* DOMString atob (in DOMString aAsciiString); */
NS_IMETHODIMP KeyPairHelperWindow::Atob(const nsAString & aAsciiString, nsAString & _retval)
{
    MOZ_NOT_IMPLEMENTED
}

/* DOMString btoa (in DOMString aBase64Data); */
NS_IMETHODIMP KeyPairHelperWindow::Btoa(const nsAString & aBase64Data, nsAString & _retval)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMElement frameElement; */
NS_IMETHODIMP KeyPairHelperWindow::GetFrameElement(nsIDOMElement * *aFrameElement)
{
    MOZ_NOT_IMPLEMENTED
}


/* readonly attribute nsIDOMDocument document; */
NS_IMETHODIMP KeyPairHelperWindow::GetDocument(nsIDOMDocument * *aDocument)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMWindow parent; */
NS_IMETHODIMP KeyPairHelperWindow::GetParent(nsIDOMWindow * *aParent)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMWindow top; */
NS_IMETHODIMP KeyPairHelperWindow::GetTop(nsIDOMWindow * *aTop)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute nsIDOMBarProp scrollbars; */
NS_IMETHODIMP KeyPairHelperWindow::GetScrollbars(nsIDOMBarProp * *aScrollbars)
{
    MOZ_NOT_IMPLEMENTED
}

/* [noscript] readonly attribute nsIDOMWindowCollection frames; */
NS_IMETHODIMP KeyPairHelperWindow::GetFrames(nsIDOMWindowCollection * *aFrames)
{
    MOZ_NOT_IMPLEMENTED
}

/* attribute DOMString name; */
NS_IMETHODIMP KeyPairHelperWindow::GetName(nsAString & aName)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetName(const nsAString & aName)
{
    MOZ_NOT_IMPLEMENTED
}

/* [noscript] attribute float textZoom; */
NS_IMETHODIMP KeyPairHelperWindow::GetTextZoom(float *aTextZoom)
{
    MOZ_NOT_IMPLEMENTED
}
NS_IMETHODIMP KeyPairHelperWindow::SetTextZoom(float aTextZoom)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute long scrollX; */
NS_IMETHODIMP KeyPairHelperWindow::GetScrollX(PRInt32 *aScrollX)
{
    MOZ_NOT_IMPLEMENTED
}

/* readonly attribute long scrollY; */
NS_IMETHODIMP KeyPairHelperWindow::GetScrollY(PRInt32 *aScrollY)
{
    MOZ_NOT_IMPLEMENTED
}

/* void scrollTo (in long xScroll, in long yScroll); */
NS_IMETHODIMP KeyPairHelperWindow::ScrollTo(PRInt32 xScroll, PRInt32 yScroll)
{
    MOZ_NOT_IMPLEMENTED
}

/* void scrollBy (in long xScrollDif, in long yScrollDif); */
NS_IMETHODIMP KeyPairHelperWindow::ScrollBy(PRInt32 xScrollDif, PRInt32 yScrollDif)
{
    MOZ_NOT_IMPLEMENTED
}

/* nsISelection getSelection (); */
NS_IMETHODIMP KeyPairHelperWindow::GetSelection(nsISelection **_retval)
{
    MOZ_NOT_IMPLEMENTED
}

/* void scrollByLines (in long numLines); */
NS_IMETHODIMP KeyPairHelperWindow::ScrollByLines(PRInt32 numLines)
{
    MOZ_NOT_IMPLEMENTED
}

/* void scrollByPages (in long numPages); */
NS_IMETHODIMP KeyPairHelperWindow::ScrollByPages(PRInt32 numPages)
{
    MOZ_NOT_IMPLEMENTED
}

/* void sizeToContent (); */
NS_IMETHODIMP KeyPairHelperWindow::SizeToContent()
{
    MOZ_NOT_IMPLEMENTED
}

#if (MOZILLA_CHECK_VERSION4 (1, 7, MOZILLA_RC, 3) && !MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 1)) || MOZILLA_CHECK_VERSION4 (1, 8, MOZILLA_ALPHA, 2)

NS_IMETHODIMP KeyPairHelperWindow::GetWindowRoot(nsIDOMEventTarget * *aWindowRoot)
{
    MOZ_NOT_IMPLEMENTED
}

#endif

#endif
