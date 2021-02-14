/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2021 Hans Petter Jansson
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

#ifndef __CHAFA_CANVAS_H__
#define __CHAFA_CANVAS_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

#include <chafa-term-info.h>

G_BEGIN_DECLS

typedef struct ChafaCanvas ChafaCanvas;

CHAFA_AVAILABLE_IN_ALL
ChafaCanvas *chafa_canvas_new (const ChafaCanvasConfig *config);
CHAFA_AVAILABLE_IN_ALL
ChafaCanvas *chafa_canvas_new_similar (ChafaCanvas *orig);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_ref (ChafaCanvas *canvas);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_unref (ChafaCanvas *canvas);

CHAFA_AVAILABLE_IN_ALL
const ChafaCanvasConfig *chafa_canvas_peek_config (ChafaCanvas *canvas);

CHAFA_AVAILABLE_IN_1_2
void chafa_canvas_draw_all_pixels (ChafaCanvas *canvas, ChafaPixelType src_pixel_type,
                                   const guint8 *src_pixels,
                                   gint src_width, gint src_height, gint src_rowstride);
CHAFA_AVAILABLE_IN_1_6
GString *chafa_canvas_print (ChafaCanvas *canvas, ChafaTermInfo *term_info);

CHAFA_AVAILABLE_IN_1_8
gunichar chafa_canvas_get_char_at (ChafaCanvas *canvas, gint x, gint y);
CHAFA_AVAILABLE_IN_1_8
gint chafa_canvas_set_char_at (ChafaCanvas *canvas, gint x, gint y, gunichar c);

CHAFA_AVAILABLE_IN_1_8
void chafa_canvas_get_colors_at (ChafaCanvas *canvas, gint x, gint y,
                                 gint *fg_out, gint *bg_out);
CHAFA_AVAILABLE_IN_1_8
void chafa_canvas_set_colors_at (ChafaCanvas *canvas, gint x, gint y,
                                 gint fg, gint bg);

CHAFA_AVAILABLE_IN_1_8
void chafa_canvas_get_raw_colors_at (ChafaCanvas *canvas, gint x, gint y,
                                     gint *fg_out, gint *bg_out);
CHAFA_AVAILABLE_IN_1_8
void chafa_canvas_set_raw_colors_at (ChafaCanvas *canvas, gint x, gint y,
                                     gint fg, gint bg);

CHAFA_DEPRECATED_IN_1_2
void chafa_canvas_set_contents_rgba8 (ChafaCanvas *canvas, const guint8 *src_pixels,
                                     gint src_width, gint src_height, gint src_rowstride);
CHAFA_DEPRECATED_IN_1_6
GString *chafa_canvas_build_ansi (ChafaCanvas *canvas);

G_END_DECLS

#endif /* __CHAFA_CANVAS_H__ */
