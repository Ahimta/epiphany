/*
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
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

#ifndef EPHY_NAVIGATION_ACTION_H
#define EPHY_NAVIGATION_ACTION_H

#include "ephy-link-action.h"

G_BEGIN_DECLS

#define EPHY_TYPE_NAVIGATION_ACTION            (ephy_navigation_action_get_type ())
#define EPHY_NAVIGATION_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_NAVIGATION_ACTION, EphyNavigationAction))
#define EPHY_NAVIGATION_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_NAVIGATION_ACTION, EphyNavigationActionClass))
#define EPHY_IS_NAVIGATION_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_NAVIGATION_ACTION))
#define EPHY_IS_NAVIGATION_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EPHY_TYPE_NAVIGATION_ACTION))
#define EPHY_NAVIGATION_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), EPHY_TYPE_NAVIGATION_ACTION, EphyNavigationActionClass))

typedef struct _EphyNavigationAction		EphyNavigationAction;
typedef struct _EphyNavigationActionPrivate	EphyNavigationActionPrivate;
typedef struct _EphyNavigationActionClass	EphyNavigationActionClass;

typedef enum
{
	EPHY_NAVIGATION_DIRECTION_UP,
	EPHY_NAVIGATION_DIRECTION_BACK,
	EPHY_NAVIGATION_DIRECTION_FORWARD
} EphyNavigationDirection;

struct _EphyNavigationAction
{
	EphyLinkAction parent;
	
	/*< private >*/
	EphyNavigationActionPrivate *priv;
};

struct _EphyNavigationActionClass
{
	EphyLinkActionClass parent_class;
};

GType ephy_navigation_action_get_type (void);

G_END_DECLS

#endif
