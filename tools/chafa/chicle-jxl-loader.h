/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2024-2025 oupson
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

#ifndef __CHICLE_JXL_LOADER_H__
#define __CHICLE_JXL_LOADER_H__

#include "chicle-file-mapping.h"
#include <glib.h>

G_BEGIN_DECLS

typedef struct ChicleJxlLoader ChicleJxlLoader;

ChicleJxlLoader *chicle_jxl_loader_new_from_mapping (ChicleFileMapping *mapping);
void chicle_jxl_loader_destroy (ChicleJxlLoader *loader);

gboolean chicle_jxl_loader_get_is_animation (ChicleJxlLoader *loader);

gconstpointer chicle_jxl_loader_get_frame_data (ChicleJxlLoader *loader,
                                                ChafaPixelType *pixel_type_out,
                                                gint *width_out,
                                                gint *height_out,
                                                gint *rowstride_out);
gint chicle_jxl_loader_get_frame_delay (ChicleJxlLoader *loader);

void chicle_jxl_loader_goto_first_frame (ChicleJxlLoader *loader);
gboolean chicle_jxl_loader_goto_next_frame (ChicleJxlLoader *loader);

G_END_DECLS

#endif /* __CHICLE_JXL_LOADER_H__ */
