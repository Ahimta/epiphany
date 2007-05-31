/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Copyright © 2003 Christian Persch
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
 *  $Id: EphyContentPolicy.h 7025 2007-05-10 17:39:37Z xrcalvar $
 */

#ifndef EPHY_CONTENT_POLICY_H
#define EPHY_CONTENT_POLICY_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtkwidget.h>

#include <nsIContentPolicy.h>

#include "ephy-embed.h"

class nsIComponentManager;
class nsIFile;
struct nsModuleComponentInfo;

#define EPHY_CONTENT_POLICY_CONTRACTID	"@gnome.org/projects/epiphany/epiphany-content-policy;1"
#define EPHY_CONTENT_POLICY_CLASSNAME	"Epiphany Content Policy Class"

#define EPHY_CONTENT_POLICY_CID				\
{ /* 6bb60b15-b7bd-4023-a19e-ab691bc3fb43 */		\
  0x6bb60b15,						\
  0xb7bd,						\
  0x4023,						\
  { 0xa1, 0x9e, 0xab, 0x69, 0x1b, 0xc3, 0xfb, 0x43 }	\
}

class EphyContentPolicy : public nsIContentPolicy
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICONTENTPOLICY
	
    EphyContentPolicy();

    static NS_METHOD Register (nsIComponentManager* aComponentManager,
			       nsIFile* aPath,
			       const char* aRegistryLocation,
			       const char* aComponentType,
			       const nsModuleComponentInfo* aInfo);

    static NS_METHOD Unregister (nsIComponentManager* aComponentManager,
				 nsIFile* aPath,
				 const char* aRegistryLocation,
				 const nsModuleComponentInfo* aInfo);

  private:
    ~EphyContentPolicy();

    static GtkWidget *GetEmbedFromContext (nsISupports *aContext);

    gboolean mLocked;
    GSList *mSafeProtocols;
};

#endif
