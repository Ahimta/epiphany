/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Chimera code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt  <hyatt@netscape.com>
 *   Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "MozillaPrivate.h"
#include "MozDownload.h"
#include "EphyHeaderSniffer.h"

#include "ephy-embed-single.h"
#include "ephy-embed-shell.h"
#include "ephy-file-chooser.h"
#include "ephy-prefs.h"
#include "ephy-gui.h"
#include "eel-gconf-extensions.h"
#include "ephy-debug.h"

#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <nsIChannel.h>
#include <nsIHttpChannel.h>
#include <nsIURL.h>
#include <nsIPrefService.h>
#include <nsIMIMEService.h>
#include <nsIMIMEInfo.h>
#include <nsIDOMHTMLDocument.h>
#include <nsIDownload.h>
#include <nsIMIMEHeaderParam.h>
#include <nsIWindowWatcher.h>

EphyHeaderSniffer::EphyHeaderSniffer (nsIWebBrowserPersist* aPersist, MozillaEmbedPersist *aEmbedPersist,
		nsIFile* aFile, nsIURI* aURL, nsIDOMDocument* aDocument, nsIInputStream* aPostData)
: mPersist(aPersist)
, mEmbedPersist(aEmbedPersist)
, mTmpFile(aFile)
, mURL(aURL)
, mOriginalURI(nsnull)
, mDocument(aDocument)
, mPostData(aPostData)
{
        LOG ("EphyHeaderSniffer ctor (%p)", this)

        nsCOMPtr<nsIWindowWatcher> watcher
                (do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
	if (!watcher) return;

	watcher->GetNewAuthPrompter (nsnull, getter_AddRefs (mAuthPrompt));
}

EphyHeaderSniffer::~EphyHeaderSniffer()
{
	LOG ("EphyHeaderSniffer dtor (%p)", this)
}

NS_IMPL_ISUPPORTS2(EphyHeaderSniffer, nsIWebProgressListener, nsIAuthPrompt)

NS_IMETHODIMP
EphyHeaderSniffer::HandleContent ()
{
	EphyEmbedSingle *single;
	gboolean handled = FALSE;
	nsEmbedCString uriSpec;

	if (mPostData) return NS_ERROR_FAILURE;

	mURL->GetSpec (uriSpec);	
	single = EPHY_EMBED_SINGLE (ephy_embed_shell_get_embed_single (embed_shell));
	g_signal_emit_by_name (single, "handle_content", mContentType.get(),
			       uriSpec.get(), &handled);

	return handled ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
EphyHeaderSniffer::OnStateChange (nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, 
                                  PRUint32 aStatus)
{  
	if (aStateFlags & nsIWebProgressListener::STATE_START)
	{
		/* be sure to keep it alive while we save since it owns
		   us as a listener and keep ourselves alive */
		nsCOMPtr<nsIWebBrowserPersist> kungFuDeathGrip(mPersist);
		nsCOMPtr<nsIWebProgressListener> kungFuSuicideGrip(this);
    
		nsresult rv;
		nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest, &rv);
		if (!channel) return rv;
		channel->GetContentType(mContentType);
    
		nsCOMPtr<nsIURI> origURI;
		channel->GetOriginalURI(getter_AddRefs(origURI));
    
		nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
		if (httpChannel)
		{
			httpChannel->GetResponseHeader(nsEmbedCString("content-disposition"),
						       mContentDisposition);
		}
    
		mPersist->CancelSave();

		PRBool exists;
		mTmpFile->Exists(&exists);
		if (exists)
		{
			mTmpFile->Remove(PR_FALSE);
		}

		rv = HandleContent ();
		if (NS_SUCCEEDED (rv)) return NS_OK;

		rv = PerformSave(origURI);
		if (NS_FAILED(rv))
		{
			/* FIXME put up some UI */
      
		}
	}

	return NS_OK;
}

NS_IMETHODIMP 
EphyHeaderSniffer::OnProgressChange (nsIWebProgress *aWebProgress, 
                                     nsIRequest *aRequest, 
                                     PRInt32 aCurSelfProgress, 
                                     PRInt32 aMaxSelfProgress, 
                                     PRInt32 aCurTotalProgress, 
                                     PRInt32 aMaxTotalProgress)
{
	return NS_OK;
}

NS_IMETHODIMP 
EphyHeaderSniffer::OnLocationChange (nsIWebProgress *aWebProgress, 
				     nsIRequest *aRequest, 
				     nsIURI *location)
{
	return NS_OK;
}

NS_IMETHODIMP 
EphyHeaderSniffer::OnStatusChange (nsIWebProgress *aWebProgress, 
				   nsIRequest *aRequest, 
				   nsresult aStatus, 
				   const PRUnichar *aMessage)
{
	return NS_OK;
}

NS_IMETHODIMP 
EphyHeaderSniffer::OnSecurityChange (nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
	return NS_OK;
}

static void
filechooser_response_cb (EphyFileChooser *dialog, gint response, EphyHeaderSniffer* sniffer)
{
	if (response == GTK_RESPONSE_ACCEPT)
	{
		char *filename;
		GtkWidget *parent = NULL; // FIXME!

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		if (ephy_gui_confirm_overwrite_file (parent, filename) == FALSE)
		{
			g_free (filename);
			return;
		}

		nsCOMPtr<nsILocalFile> destFile = do_CreateInstance (NS_LOCAL_FILE_CONTRACTID);
		if (destFile)
		{
			destFile->InitWithNativePath (nsEmbedCString (filename));

			sniffer->InitiateDownload (destFile);
		}

		g_free (filename);
	}

	// FIXME how to inform user of failed save ?

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

nsresult EphyHeaderSniffer::PerformSave (nsIURI* inOriginalURI)
{
	nsresult rv;
	EmbedPersistFlags flags;
	PRBool askDownloadDest;

	mOriginalURI = inOriginalURI;

	flags = ephy_embed_persist_get_flags (EPHY_EMBED_PERSIST (mEmbedPersist));
	askDownloadDest = flags & EMBED_PERSIST_ASK_DESTINATION;

	nsEmbedString defaultFileName;

	if (!defaultFileName.Length() && mContentDisposition.Length())
	{
		/* 1 Use the HTTP header suggestion. */
		nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar =
			do_GetService("@mozilla.org/network/mime-hdrparam;1");     
		
		if (mimehdrpar)
		{
			nsEmbedCString fallbackCharset;
			if (mURL)
			{
				mURL->GetOriginCharset(fallbackCharset);
			}
			
			nsEmbedString fileName;
			
			rv = mimehdrpar->GetParameter (mContentDisposition, "filename",
						       fallbackCharset, PR_TRUE, nsnull,
						       fileName);
			if (NS_FAILED(rv) || !fileName.Length())
			{
				rv = mimehdrpar->GetParameter (mContentDisposition, "name",
							       fallbackCharset, PR_TRUE, nsnull,
							       fileName);
			}

			if (NS_SUCCEEDED(rv) && fileName.Length())
			{
				defaultFileName = fileName;
			}
		}
	}
    
	if (!defaultFileName.Length())
	{
		/* 2 For file URLs, use the file name. */

		nsCOMPtr<nsIURL> url(do_QueryInterface(mURL));
		if (url)
		{
			nsEmbedCString fileNameCString;
			url->GetFileName(fileNameCString);
			NS_CStringToUTF16 (fileNameCString, NS_CSTRING_ENCODING_UTF8,
					   defaultFileName);
		}
	}
    
	if (!defaultFileName.Length() && mDocument)
	{
		/* 3 Use the title of the document. */

		nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(mDocument));
		if (htmlDoc)
		{
			htmlDoc->GetTitle(defaultFileName);
		}
	}
    
	if (defaultFileName.Length() && mURL)
	{
		/* 4 Use the host. */
		nsEmbedCString hostName;
		mURL->GetHost(hostName);
		NS_CStringToUTF16 (hostName, NS_CSTRING_ENCODING_UTF8,
				   defaultFileName);
	}
    
	/* 5 One last case to handle about:blank and other untitled pages. */
	if (!defaultFileName.Length())
	{
		NS_CStringToUTF16 (nsEmbedCString(_("Untitled")),
				   NS_CSTRING_ENCODING_UTF8, defaultFileName);
	}
        
	/* Validate the file name to ensure legality. */
	nsEmbedCString cDefaultFileName;
	NS_UTF16ToCString (defaultFileName, NS_CSTRING_ENCODING_UTF8,
			   cDefaultFileName);
	char *default_name = g_strdup (cDefaultFileName.get());
	default_name = g_strdelimit (default_name, "/", ' ');

	const char *key;
	key = ephy_embed_persist_get_persist_key (EPHY_EMBED_PERSIST (mEmbedPersist));

	/* FIXME: do better here by using nsITextToSubURI service, like in
	 * http://lxr.mozilla.org/seamonkey/source/xpfe/communicator/resources/content/contentAreaUtils.js#763
	 */
        char *filename;
        filename = gnome_vfs_unescape_string (default_name, NULL);

        if (!g_utf8_validate (filename, -1, NULL))
        {
                g_free (filename);
                filename = g_strdup (default_name);
        }

	g_free (default_name);

	if (askDownloadDest)
	{
		EphyFileChooser *dialog;
		GtkWindow *window;
		const char *title;

		title = ephy_embed_persist_get_fc_title (EPHY_EMBED_PERSIST (mEmbedPersist));
		window = ephy_embed_persist_get_fc_parent (EPHY_EMBED_PERSIST (mEmbedPersist));

		dialog = ephy_file_chooser_new (title ? title: _("Save"),
						GTK_WIDGET (window),
						GTK_FILE_CHOOSER_ACTION_SAVE,
						key ? key : CONF_STATE_SAVE_DIR,
						EPHY_FILE_FILTER_ALL_SUPPORTED);

		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog),
                                                   filename);

		g_signal_connect (dialog, "response",
				  G_CALLBACK (filechooser_response_cb), this);

		gtk_widget_show (GTK_WIDGET (dialog));

                g_free (filename);
		return NS_OK;
	}
	/* FIXME: how to inform user of failed save ? */

	nsCOMPtr<nsILocalFile> destFile;
	BuildDownloadPath (filename, getter_AddRefs (destFile));
        g_free (filename);
	NS_ENSURE_TRUE (destFile, NS_ERROR_FAILURE);

	return InitiateDownload (destFile);
}

nsresult EphyHeaderSniffer::InitiateDownload (nsILocalFile *aDestFile)
{
	LOG ("Initiating download")

	return InitiateMozillaDownload (mDocument, mURL, aDestFile,
					mContentType.get(), mOriginalURI, mEmbedPersist,
					mPostData, nsnull, -1);
}
