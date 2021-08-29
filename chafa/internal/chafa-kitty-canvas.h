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

#ifndef __CHAFA_KITTY_CANVAS_H__
#define __CHAFA_KITTY_CANVAS_H__

#include "chafa.h"

G_BEGIN_DECLS

typedef struct
{
    gint width, height;
    gpointer rgba_image;
}
ChafaKittyCanvas;

ChafaKittyCanvas *chafa_kitty_canvas_new (gint width, gint height);
void chafa_kitty_canvas_destroy (ChafaKittyCanvas *kitty_canvas);

void chafa_kitty_canvas_draw_all_pixels (ChafaKittyCanvas *kitty_canvas, ChafaPixelType src_pixel_type,
                                         gconstpointer src_pixels,
                                         gint src_width, gint src_height, gint src_rowstride);
void chafa_kitty_canvas_build_ansi (ChafaKittyCanvas *kitty_canvas, ChafaTermInfo *term_info, GString *out_str,
                                    gint width_cells, gint height_cells);

G_END_DECLS

#endif /* __CHAFA_KITTY_CANVAS_H__ */
