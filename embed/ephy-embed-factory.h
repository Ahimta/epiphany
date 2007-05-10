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
 *
 *  $Id$
 */

#ifndef EPHY_EMBED_FACTORY_H
#define EPHY_EMBED_FACTORY_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

GObject	*ephy_embed_factory_new_object (GType type);

G_END_DECLS

#endif
