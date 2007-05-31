/* 
 *  Copyright © 2006 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id: AutoWindowModalState.cpp 7025 2007-05-10 17:39:37Z xrcalvar $
 */

#include <mozilla-config.h>
#include "config.h"

#include "AutoWindowModalState.h"

AutoWindowModalState::AutoWindowModalState (nsIDOMWindow *aWindow)
{
#ifdef HAVE_GECKO_1_8_1
  if (aWindow) {
    mWindow = do_QueryInterface (aWindow);
    NS_ASSERTION (mWindow, "Should have a window here!");
  }

  if (mWindow) {
#ifdef HAVE_GECKO_1_9
    mWindow->EnterModalState ();
#else
    nsCOMPtr<nsPIDOMWindow_MOZILLA_1_8_BRANCH> window (do_QueryInterface (mWindow));
    NS_ENSURE_TRUE (window, );

    window->EnterModalState ();
#endif /* HAVE_GECKO_1_9 */
  }
#endif /* HAVE_GECKO_1_8_1 */
}

AutoWindowModalState::~AutoWindowModalState()
{
#ifdef HAVE_GECKO_1_8_1
  if (mWindow) {
#ifdef HAVE_GECKO_1_9
    mWindow->LeaveModalState ();
#else
    nsCOMPtr<nsPIDOMWindow_MOZILLA_1_8_BRANCH> window (do_QueryInterface (mWindow));
    NS_ENSURE_TRUE (window, );

    window->LeaveModalState ();
#endif /* HAVE_GECKO_1_9 */
  }
#endif /* HAVE_GECKO_1_8_1 */
}
