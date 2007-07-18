/*
 *  Copyright © 2000-2004 Marco Pesenti Gritti
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

#ifndef EPHY_BROWSER_H
#define EPHY_BROWSER_H

#include "ephy-embed.h"
#include <gtk/gtkwidget.h>

#include <nsCOMPtr.h>
#include <nsIDOMContextMenuListener.h>
#include <nsIDOMDocument.h>
#include <nsIDOMEventListener.h>
#include <nsIDOMEventTarget.h>
#include <nsIDOMWindow.h>
#include <nsIRequest.h>
#include <nsISHistory.h>
#include <nsIWebBrowserFocus.h>
#include <nsIWebBrowser.h>
#include <nsIWebNavigation.h>
#include <nsPIDOMWindow.h>

#ifdef ALLOW_PRIVATE_API
#include <nsIContentViewer.h>
#endif

#ifdef HAVE_MOZILLA_PSM
#include <nsISecureBrowserUI.h>
#endif

#include "gecko-embed.h"

class EphyBrowser;

class EphyEventListener : public nsIDOMEventListener
{
public:
	NS_DECL_ISUPPORTS

	// nsIDOMEventListener
	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) = 0;

	EphyEventListener(EphyBrowser *aOwner) : mOwner(aOwner) { };
	virtual ~EphyEventListener() { };

protected:
	EphyBrowser *mOwner;
};

class EphyDOMLinkEventListener : public EphyEventListener
{
public:
	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

	EphyDOMLinkEventListener(EphyBrowser *aOwner) : EphyEventListener(aOwner) { };
private:
	nsresult GetDocURI (nsIDOMElement *aElement,
			    nsIURI **aDocURI);

};

class EphyPopupBlockEventListener : public EphyEventListener
{
public:
	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

	EphyPopupBlockEventListener(EphyBrowser *aOwner) : EphyEventListener(aOwner) { };
};

class EphyModalAlertEventListener : public EphyEventListener
{
public:
	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

	EphyModalAlertEventListener(EphyBrowser *aOwner) : EphyEventListener(aOwner) { };
};

class EphyMiscDOMEventsListener : public EphyEventListener
{
public:
	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

	EphyMiscDOMEventsListener(EphyBrowser *aOwner) : EphyEventListener(aOwner) { };
};

class EphyDOMScrollEventListener : public EphyEventListener
{
public:
	NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

	EphyDOMScrollEventListener(EphyBrowser *aOwner) : EphyEventListener(aOwner) { };
};

class EphyContextMenuListener : public nsIDOMContextMenuListener
{
public:
        NS_DECL_ISUPPORTS

	// nsIDOMContextMenuListener
        NS_IMETHOD ContextMenu(nsIDOMEvent *aEvent);
        NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);

	EphyContextMenuListener(EphyBrowser *aOwner) : mOwner(aOwner) { };
	virtual ~EphyContextMenuListener() { };

protected:
        EphyBrowser *mOwner;
};

class EphyBrowser
{
friend class EphyEventListener;
friend class EphyDOMLinkEventListener;
friend class EphyMiscDOMEventsListener;
friend class EphyDOMScrollEventListener;
friend class EphyPopupBlockEventListener;
friend class EphyModalAlertEventListener;
friend class EphyContextMenuListener;
public:
	EphyBrowser();
	~EphyBrowser();

	nsresult Init (GeckoEmbed *mozembed);
	nsresult Destroy (void);

	nsresult DoCommand (const char *command);
	nsresult GetCommandState (const char *command, PRBool *enabled);

	nsresult SetZoom (float aTextZoom);
	nsresult GetZoom (float *aTextZoom);

	nsresult ScrollLines (PRInt32 aNumLines);
	nsresult ScrollPages (PRInt32 aNumPages);
	nsresult ScrollPixels (PRInt32 aDeltaX, PRInt32 aDeltaY);

	nsresult Print ();
	nsresult SetPrintPreviewMode (PRBool previewMode);
	nsresult PrintPreviewNumPages (int *numPages);
	nsresult PrintPreviewNavigate(PRInt16 navType, PRInt32 pageNum);

	nsresult GetPageDescriptor(nsISupports **aPageDescriptor);

	nsresult GetSHInfo (PRInt32 *count, PRInt32 *index);
	nsresult GetSHTitleAtIndex (PRInt32 index, PRUnichar **title);
	nsresult GetSHUrlAtIndex (PRInt32 index, nsACString &url);
	nsresult GoToHistoryIndex (PRInt16 index);

	nsresult ForceEncoding (const char *encoding);
	nsresult GetEncoding (nsACString &encoding);
	nsresult GetForcedEncoding (nsACString &encoding);

	nsresult PushTargetDocument (nsIDOMDocument *domDoc);
	nsresult PopTargetDocument ();

	nsresult GetDocument (nsIDOMDocument **aDOMDocument);
	nsresult GetTargetDocument (nsIDOMDocument **aDOMDocument);
	nsresult GetDocumentURI (nsIURI **aURI);
	nsresult GetTargetDocumentURI (nsIURI **aURI);
	nsresult GetDOMWindow (nsIDOMWindow **window);
	nsresult GetPIDOMWindow(nsPIDOMWindow **aPIWin);

	nsresult GetHasModifiedForms (PRBool *modified);

	nsresult GetSecurityInfo (PRUint32 *aState, nsACString &aDescription);
	nsresult ShowCertificate ();

	nsresult CopySHistory (EphyBrowser *dest, PRBool copy_back,
	                       PRBool copy_forward, PRBool copy_current);

	nsresult Close ();

	nsresult LoadURI(const char *aURI, 
			 PRUint32 aLoadFlags = nsIWebNavigation::LOAD_FLAGS_NONE, 
			 nsIURI *aURI = nsnull);

        EphyEmbedDocumentType GetDocumentType ();

	nsCOMPtr<nsIWebBrowser> mWebBrowser;
private:
	GtkWidget *mEmbed;

	nsCOMPtr<nsIWebBrowserFocus> mWebBrowserFocus;
	nsCOMPtr<nsIDOMDocument> mTargetDocument;
	nsCOMPtr<nsIDOMEventTarget> mEventTarget;
	nsCOMPtr<nsIDOMWindow> mDOMWindow;
	EphyDOMLinkEventListener *mDOMLinkEventListener;
	EphyMiscDOMEventsListener *mMiscDOMEventsListener;
	EphyDOMScrollEventListener *mDOMScrollEventListener;
	EphyPopupBlockEventListener *mPopupBlockEventListener;
	EphyModalAlertEventListener *mModalAlertListener;
	EphyContextMenuListener *mContextMenuListener;
	PRBool mInitialized;
#ifdef HAVE_MOZILLA_PSM
	nsCOMPtr<nsISecureBrowserUI> mSecurityInfo;
#endif

	nsresult GetListener (void);
	nsresult AttachListeners (void);
	nsresult DetachListeners (void);
	nsresult GetSHistory (nsISHistory **aSHistory);
	nsresult GetContentViewer (nsIContentViewer **aViewer);
	nsresult GetDocumentHasModifiedForms (nsIDOMDocument *aDomDoc, PRUint32 *aNumTextFields, PRBool *aHasTextArea);
	PRBool   CompareFormsText (nsAString &aDefaultText, nsAString &aUserText);
};

#endif /* !EPHY_BROWSER_H */
