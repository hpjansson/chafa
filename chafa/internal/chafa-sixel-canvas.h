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

#ifndef __CHAFA_SIXEL_CANVAS_H__
#define __CHAFA_SIXEL_CANVAS_H__

#include "chafa.h"

G_BEGIN_DECLS

typedef struct
{
    gint width, height;
    ChafaColorSpace color_space;
    ChafaIndexedImage *image;
}
ChafaSixelCanvas;

ChafaSixelCanvas *chafa_sixel_canvas_new (gint width, gint height,
                                          ChafaColorSpace color_space,
                                          const ChafaPalette *palette,
                                          const ChafaDither *dither);
void chafa_sixel_canvas_destroy (ChafaSixelCanvas *sixel_canvas);

void chafa_sixel_canvas_draw_all_pixels (ChafaSixelCanvas *sixel_canvas, ChafaPixelType src_pixel_type,
                                         gconstpointer src_pixels,
                                         gint src_width, gint src_height, gint src_rowstride);
void chafa_sixel_canvas_build_ansi (ChafaSixelCanvas *sixel_canvas, GString *out_str);

G_END_DECLS

#endif /* __CHAFA_SIXEL_CANVAS_H__ */
