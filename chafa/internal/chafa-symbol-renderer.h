/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2025-2026 Hans Petter Jansson
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

#ifndef __CHAFA_SYMBOL_RENDERER_H__
#define __CHAFA_SYMBOL_RENDERER_H__

#include "chafa.h"

G_BEGIN_DECLS

typedef struct
{
    ChafaCanvas *canvas;
    gint x, y;
    gint width, height;
    gpointer rgba_image;
}
ChafaSymbolRenderer;

ChafaSymbolRenderer *chafa_symbol_renderer_new (ChafaCanvas *canvas,
						gint x, gint y,
						gint width, gint height);
void chafa_symbol_renderer_destroy (ChafaSymbolRenderer *symbol_renderer);

void chafa_symbol_renderer_draw_all_pixels (ChafaSymbolRenderer *symbol_renderer,
					    ChafaPixelType src_pixel_type,
					    gconstpointer src_pixels,
					    gint src_width, gint src_height, gint src_rowstride,
					    ChafaAlign halign, ChafaAlign valign,
					    ChafaTuck tuck,
					    gfloat quality);

G_END_DECLS

#endif /* __CHAFA_SYMBOL_RENDERER_H__ */
