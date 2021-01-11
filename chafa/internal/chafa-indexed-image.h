/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2021 Hans Petter Jansson
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

#ifndef __CHAFA_INDEXED_IMAGE_H__
#define __CHAFA_INDEXED_IMAGE_H__

#include "internal/chafa-palette.h"
#include "internal/chafa-dither.h"

G_BEGIN_DECLS

typedef struct
{
    gint width, height;
    ChafaPalette palette;
    ChafaDither dither;
    guint8 *pixels;
}
ChafaIndexedImage;

ChafaIndexedImage *chafa_indexed_image_new (gint width, gint height,
                                            const ChafaPalette *palette,
                                            const ChafaDither *dither);
void chafa_indexed_image_destroy (ChafaIndexedImage *indexed_image);
void chafa_indexed_image_draw_pixels (ChafaIndexedImage *indexed_image,
                                      ChafaColorSpace color_space,
                                      ChafaPixelType src_pixel_type,
                                      gconstpointer src_pixels,
                                      gint src_width, gint src_height, gint src_rowstride,
                                      gint dest_width, gint dest_height);

G_END_DECLS

#endif /* __CHAFA_INDEXED_IMAGE_H__ */
