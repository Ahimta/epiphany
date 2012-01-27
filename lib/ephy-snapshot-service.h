/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2012 Igalia S.L.
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
 *  Author: Claudio Saavedra  <csaavedra@igalia.com>
 */

#ifndef _EPHY_SNAPSHOT_SERVICE_H
#define _EPHY_SNAPSHOT_SERVICE_H

#include <gtk/gtk.h>

#include <glib-object.h>

G_BEGIN_DECLS

#define EPHY_TYPE_SNAPSHOT_SERVICE            (ephy_snapshot_service_get_type())
#define EPHY_SNAPSHOT_SERVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EPHY_TYPE_SNAPSHOT_SERVICE, EphySnapshotService))
#define EPHY_SNAPSHOT_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EPHY_TYPE_SNAPSHOT_SERVICE, EphySnapshotServiceClass))
#define EPHY_IS_SNAPSHOT_SERVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EPHY_TYPE_SNAPSHOT_SERVICE))
#define EPHY_IS_SNAPSHOT_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EPHY_TYPE_SNAPSHOT_SERVICE))
#define EPHY_SNAPSHOT_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EPHY_TYPE_SNAPSHOT_SERVICE, EphySnapshotServiceClass))

typedef struct _EphySnapshotService        EphySnapshotService;
typedef struct _EphySnapshotServiceClass   EphySnapshotServiceClass;
typedef struct _EphySnapshotServicePrivate EphySnapshotServicePrivate;

struct _EphySnapshotService
{
  GObject parent;

  /*< private >*/
  EphySnapshotServicePrivate *priv;
};

struct _EphySnapshotServiceClass
{
  GObjectClass parent_class;
};

typedef void (* EphySnapshotServiceCallback) (GdkPixbuf *snapshot,
					      gpointer user_data);

GType                ephy_snapshot_service_get_type (void) G_GNUC_CONST;

EphySnapshotService *ephy_snapshot_service_get_default (void);

void                 ephy_snapshot_service_get_snapshot (EphySnapshotService *service,
							 const char *url,
							 const time_t mtime,
							 EphySnapshotServiceCallback callback,
							 gpointer userdata);

G_END_DECLS

#endif /* _EPHY_SNAPSHOT_SERVICE_H */
