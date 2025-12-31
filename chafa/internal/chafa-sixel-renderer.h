/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2025 Hans Petter Jansson
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

#ifndef __CHAFA_SIXEL_RENDERER_H__
#define __CHAFA_SIXEL_RENDERER_H__

#include "chafa.h"

G_BEGIN_DECLS

typedef struct
{
    gint width, height;
    ChafaColorSpace color_space;
    ChafaIndexedImage *image;
}
ChafaSixelRenderer;

ChafaSixelRenderer *chafa_sixel_renderer_new (gint width, gint height,
                                              ChafaColorSpace color_space,
                                              const ChafaPalette *palette,
                                              const ChafaDither *dither);
void chafa_sixel_renderer_destroy (ChafaSixelRenderer *sixel_renderer);

void chafa_sixel_renderer_draw_all_pixels (ChafaSixelRenderer *sixel_renderer,
                                           ChafaPixelType src_pixel_type,
                                           gconstpointer src_pixels,
                                           gint src_width, gint src_height, gint src_rowstride,
                                           ChafaAlign halign, ChafaAlign valign,
                                           ChafaTuck tuck,
                                           gfloat quality);
void chafa_sixel_renderer_build_ansi (ChafaSixelRenderer *sixel_renderer, ChafaTermInfo *term_info,
                                      GString *out_str, ChafaPassthrough passthrough);

G_END_DECLS

#endif /* __CHAFA_SIXEL_RENDERER_H__ */
