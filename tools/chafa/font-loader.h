/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2022 Hans Petter Jansson
 *
 * This file is part of Chafa, a program that turns images into character art.
 *
 * Chafa is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Chafa is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Chafa.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef __FONT_LOADER_H__
#define __FONT_LOADER_H__

#include <glib.h>
#include "file-mapping.h"

G_BEGIN_DECLS

typedef struct FontLoader FontLoader;

FontLoader *font_loader_new_from_mapping (FileMapping *mapping);
void font_loader_destroy (FontLoader *loader);

gboolean font_loader_get_next_glyph (FontLoader *loader, gunichar *char_out,
                                     gpointer *glyph_out, gint *width_out, gint *height_out);

G_END_DECLS

#endif /* __FONT_LOADER_H__ */
