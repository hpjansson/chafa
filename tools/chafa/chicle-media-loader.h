/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2024-2025 Hans Petter Jansson
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

#ifndef __CHICLE_MEDIA_LOADER_H__
#define __CHICLE_MEDIA_LOADER_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct ChicleMediaLoader ChicleMediaLoader;

ChicleMediaLoader *chicle_media_loader_new (const gchar *path,
                                            gint target_width,
                                            gint target_height,
                                            GError **error);
void chicle_media_loader_destroy (ChicleMediaLoader *loader);

gboolean chicle_media_loader_get_is_animation (ChicleMediaLoader *loader);

void chicle_media_loader_goto_first_frame (ChicleMediaLoader *loader);
gboolean chicle_media_loader_goto_next_frame (ChicleMediaLoader *loader);

gconstpointer chicle_media_loader_get_frame_data (ChicleMediaLoader *loader,
                                                  ChafaPixelType *pixel_type_out,
                                                  gint *width_out,
                                                  gint *height_out,
                                                  gint *rowstride_out);
gint chicle_media_loader_get_frame_delay (ChicleMediaLoader *loader);

gchar **chicle_get_loader_names (void);

G_END_DECLS

#endif /* __CHICLE_MEDIA_LOADER_H__ */
