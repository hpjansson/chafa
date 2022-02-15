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

#ifndef __IM_LOADER_H__
#define __IM_LOADER_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct ImLoader ImLoader;

ImLoader *im_loader_new (const gchar *path);
void im_loader_destroy (ImLoader *loader);

gboolean im_loader_get_is_animation (ImLoader *loader);

gconstpointer im_loader_get_frame_data (ImLoader *loader, ChafaPixelType *pixel_type_out,
                                        gint *width_out, gint *height_out, gint *rowstride_out);
gint im_loader_get_frame_delay (ImLoader *loader);

void im_loader_goto_first_frame (ImLoader *loader);
gboolean im_loader_goto_next_frame (ImLoader *loader);

G_END_DECLS

#endif /* __IM_LOADER_H__ */
