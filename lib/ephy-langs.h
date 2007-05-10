/*
 *  Copyright © 2000, 2003 Marco Pesenti Gritti
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

#ifndef EPHY_LANGS_H
#define EPHY_LANGS_H

#include <glib.h>

G_BEGIN_DECLS

#define ISO_639_DOMAIN	"iso_639"
#define ISO_3166_DOMAIN	"iso_3166"

typedef struct
{
	char *title;
	char *code;
} EphyFontsLanguageInfo;

const EphyFontsLanguageInfo *ephy_font_languages	 (void);

guint			     ephy_font_n_languages	 (void);

void			     ephy_langs_append_languages (GArray *array);

void			     ephy_langs_sanitise	 (GArray *array);

char			   **ephy_langs_get_languages	 (void);

GHashTable		    *ephy_langs_iso_639_table	 (void);

GHashTable		    *ephy_langs_iso_3166_table   (void);

G_END_DECLS

#endif
