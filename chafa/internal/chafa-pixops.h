/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020-2021 Hans Petter Jansson
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

#ifndef __CHAFA_PIXOPS_H__
#define __CHAFA_PIXOPS_H__

#include <glib.h>
#include "internal/chafa-private.h"

G_BEGIN_DECLS

void chafa_prepare_pixel_data_for_symbols (const ChafaPalette *palette,
                                           const ChafaDither *dither,
                                           ChafaColorSpace color_space,
                                           gboolean preprocessing_enabled,
                                           gint work_factor,
                                           ChafaPixelType src_pixel_type,
                                           gconstpointer src_pixels,
                                           gint src_width,
                                           gint src_height,
                                           gint src_rowstride,
                                           ChafaPixel *dest_pixels,
                                           gint dest_width,
                                           gint dest_height);

void chafa_sort_pixel_index_by_channel (guint8 *index,
                                        const ChafaPixel *pixels, gint n_pixels,
                                        gint ch);

G_END_DECLS

#endif /* __CHAFA_PIXOPS_H__ */
