/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2022 Hans Petter Jansson
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

#ifndef __MEDIA_LOADER_H__
#define __MEDIA_LOADER_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct MediaLoader MediaLoader;

MediaLoader *media_loader_new (const gchar *path);
void media_loader_destroy (MediaLoader *loader);

gboolean media_loader_get_is_animation (MediaLoader *loader);

void media_loader_goto_first_frame (MediaLoader *loader);
gboolean media_loader_goto_next_frame (MediaLoader *loader);

gconstpointer media_loader_get_frame_data (MediaLoader *loader, ChafaPixelType *pixel_type_out,
                                           gint *width_out, gint *height_out, gint *rowstride_out);
gint media_loader_get_frame_delay (MediaLoader *loader);

G_END_DECLS

#endif /* __MEDIA_LOADER_H__ */
