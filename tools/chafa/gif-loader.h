/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2024 Hans Petter Jansson
 *
 * This file is part of Chafa, a program that shows pictures on text terminals.
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

#ifndef __GIF_LOADER_H__
#define __GIF_LOADER_H__

#include <glib.h>
#include "file-mapping.h"

G_BEGIN_DECLS

typedef struct GifLoader GifLoader;

GifLoader *gif_loader_new_from_mapping (FileMapping *mapping);
void gif_loader_destroy (GifLoader *loader);

gboolean gif_loader_get_is_animation (GifLoader *loader);

gconstpointer gif_loader_get_frame_data (GifLoader *loader, ChafaPixelType *pixel_type_out,
                                         gint *width_out, gint *height_out, gint *rowstride_out);
gint gif_loader_get_frame_delay (GifLoader *loader);

void gif_loader_goto_first_frame (GifLoader *loader);
gboolean gif_loader_goto_next_frame (GifLoader *loader);

G_END_DECLS

#endif /* __GIF_LOADER_H__ */
