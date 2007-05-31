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
 * Portions created by the Initial Developer are Copyright © 2002
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
 * $Id: EphyHeaderSniffer.h 6588 2006-09-13 11:34:25Z chpe $
 */

#ifndef EPHY_HEADER_SNIFFER_H
#define EPHY_HEADER_SNIFFER_H

class nsIDOMDocument;
class nsIFile;
class nsIInputStream;
class nsILocalFile;
class nsIURI;
class nsIWebBrowserPersist;

#include <nsCOMPtr.h>
#include <nsIAuthPrompt.h>
#include <nsIInterfaceRequestor.h>
#include <nsIWebProgressListener.h>

#include "webkit-embed-persist.h"
#include "ephy-embed-single.h"

class EphyHeaderSniffer : public nsIWebProgressListener,
			  public nsIInterfaceRequestor,
			  public nsIAuthPrompt
{
public:
	EphyHeaderSniffer (nsIWebBrowserPersist* aPersist, WebkitEmbedPersist *aEmbedPersist,
		           nsIFile* aFile, nsIURI* aURL, nsIDOMDocument* aDocument,
			   nsIInputStream* aPostData, EphyEmbedSingle *single);
	virtual ~EphyHeaderSniffer ();

	NS_DECL_ISUPPORTS
	NS_DECL_NSIWEBPROGRESSLISTENER
	NS_FORWARD_SAFE_NSIAUTHPROMPT(mAuthPrompt)
	NS_DECL_NSIINTERFACEREQUESTOR

	nsresult InitiateDownload (nsILocalFile *aDestFile);

protected:
	nsresult PerformSave (nsIURI* inOriginalURI);
	nsresult HandleContent ();

private:
	nsIWebBrowserPersist*      mPersist; /* Weak. It owns us as a listener. */
	WebkitEmbedPersist        *mEmbedPersist;
	EphyEmbedSingle		  *mSingle;
	nsCOMPtr<nsIFile>          mTmpFile;
	nsCOMPtr<nsIURI>           mURL;
	nsCOMPtr<nsIURI>           mOriginalURI;
	nsCOMPtr<nsIDOMDocument>   mDocument;
	nsCOMPtr<nsIInputStream>   mPostData;
	nsCString             mContentType;
	nsCString             mContentDisposition;
	nsCOMPtr<nsIAuthPrompt>    mAuthPrompt;
};

#endif /* !EPHY_HEADER_SNIFFER_H */
