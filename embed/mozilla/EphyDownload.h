/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Conrad Carlen <ccarlen@netscape.com>
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
 * ***** END LICENSE BLOCK ***** */

#ifndef EphyDownload_h__
#define EphyDownload_h__

#include "mozilla-embed-persist.h"
#include "nsIDownload.h"
#include "nsIWebProgressListener.h"
#include "nsIHelperAppLauncherDialog.h"
#include "nsIExternalHelperAppService.h"

#include "nsIURI.h"
#include "nsILocalFile.h"
#include "nsIWebBrowserPersist.h"

#include "downloader-view.h"
#include "ephy-embed-shell.h"

//*****************************************************************************
// EphyDownload
//
// Holds information used to display a single download in the UI. This object is
// created in one of two ways:
// (1) By nsExternalHelperAppHandler when Gecko encounters a  MIME type which
//     it doesn't itself handle. In this case, the notifications sent to
//     nsIDownload are controlled by nsExternalHelperAppHandler.
// (2) By the embedding app's file saving code when saving a web page or a link
//     target. See CHeaderSniffer.cpp. In this case, the notifications sent to
//     nsIDownload are controlled by the implementation of nsIWebBrowserPersist.
//*****************************************************************************   

#define EPHY_DOWNLOAD_CID                \
{ /* d2a2f743-f126-4f1f-1234-d4e50490f112 */         \
    0xd2a2f743,                                      \
    0xf126,                                          \
    0x4f1f,                                          \
    {0x12, 0x34, 0xd4, 0xe5, 0x04, 0x90, 0xf1, 0x12} \
}

#define EPHY_DOWNLOAD_CLASSNAME "Ephy's Download Progress Dialog"

class EphyDownload : public nsIDownload,
                     public nsIWebProgressListener
//                     public LBroadcaster
{
public:
    
    // Messages we broadcast to listeners.
    enum {
        msg_OnDLStart           = 57723,    // param is EphyDownload*
        msg_OnDLComplete,                   // param is EphyDownload*
        msg_OnDLProgressChange              // param is MsgOnDLProgressChangeInfo*
    };
       
                            EphyDownload();
    virtual                 ~EphyDownload();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOWNLOAD
    NS_DECL_NSIWEBPROGRESSLISTENER

    virtual void	    SetEmbedPersist (MozillaEmbedPersist *aEmbedPersist);
    virtual void            Cancel();
    virtual void	    Pause();
    virtual void	    Resume();
    virtual void            GetStatus(nsresult& aStatus)
                            { aStatus = mStatus; }

protected:
    nsCOMPtr<nsIURI>        mSource;
    nsCOMPtr<nsILocalFile>  mDestination;
    PRInt64                 mStartTime;
    PRInt64		    mElapsed;
    PRInt32                 mPercentComplete;
    
    bool                    mGotFirstStateChange, mIsNetworkTransfer;
    bool                    mUserCanceled;
    bool 		    mIsPaused;
    nsresult                mStatus;
    
    // These two are mutually exclusive.
    nsCOMPtr<nsIWebBrowserPersist> mWebPersist;
    nsCOMPtr<nsIHelperAppLauncher> mHelperAppLauncher;
    
    PRFloat64 mPriorKRate;
    PRInt32 mRateChanges;
    PRInt32 mRateChangeLimit;
    PRInt64 mLastUpdate;
    PRInt32 mInterval;
    DownloaderView *mDownloaderView;
    MozillaEmbedPersist *mEmbedPersist;
};

#endif // EphyDownload_h__
