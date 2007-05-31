/*
 *  Copyright © 2005 Christian Persch
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
 *  $Id: EphyRedirectChannel.cpp 7025 2007-05-10 17:39:37Z xrcalvar $
 */

#include "mozilla-config.h"
#include "config.h"

#include "EphyRedirectChannel.h"

NS_IMPL_ISUPPORTS2 (EphyWrappedChannel, nsIRequest, nsIChannel)

NS_IMETHODIMP
EphyRedirectChannel::SetLoadFlags(nsLoadFlags aFlags)
{
	return mChannel->SetLoadFlags (aFlags | LOAD_REPLACE);
}
