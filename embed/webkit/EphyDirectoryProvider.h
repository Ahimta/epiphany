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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id: EphyDirectoryProvider.h 7025 2007-05-10 17:39:37Z xrcalvar $
 */
 
#ifndef EPHY_DIRECTORY_PROVIDER_H
#define EPHY_DIRECTORY_PROVIDER_H
    
#include <nsIDirectoryService.h>

class EphyDirectoryProvider : public nsIDirectoryServiceProvider2
{
  public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIDIRECTORYSERVICEPROVIDER
	NS_DECL_NSIDIRECTORYSERVICEPROVIDER2

	EphyDirectoryProvider() { }
	virtual ~EphyDirectoryProvider() { }
};

#endif /* EPHY_DIRECTORY_PROVIDER_H */
