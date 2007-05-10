/*
 *  Copyright © 2005 Gustavo Gama
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

#ifndef EPHY_ACTIVATION_H
#define EPHY_ACTIVATION_H

#include "ephy-dbus.h"

G_BEGIN_DECLS

/* activation handlers */
gboolean ephy_activation_load_uri_list		(EphyDbus *ephy_dbus,
						 char **uris,
						 char *options,
						 guint startup_id,
						 GError **error);

gboolean ephy_activation_load_session		(EphyDbus *ephy_dbus,
						 char *session_name,
						 guint user_time,
						 GError **error);

gboolean ephy_activation_open_bookmarks_editor	(EphyDbus *ephy_dbus,
						 guint user_time,
						 GError **error);

G_END_DECLS

#endif 
