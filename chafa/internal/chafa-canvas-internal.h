/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2022 Hans Petter Jansson
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

#ifndef __CHAFA_CANVAS_INTERNAL_H__
#define __CHAFA_CANVAS_INTERNAL_H__

#include <glib.h>
#include "chafa.h"
#include "internal/chafa-private.h"
#include "internal/chafa-pixops.h"

G_BEGIN_DECLS

struct ChafaCanvasCell
{
    gunichar c;

    /* Colors can be either packed RGBA or index */
    guint32 fg_color;
    guint32 bg_color;
};

struct ChafaCanvas
{
    gint refs;

    gint width_pixels, height_pixels;
    ChafaPixel *pixels;
    ChafaCanvasCell *cells;
    guint have_alpha : 1;
    guint needs_clear : 1;

    /* Whether to consider inverted symbols; FALSE if using FG only */
    guint consider_inverted : 1;

    /* Whether to extract symbol colors; FALSE if using default colors */
    guint extract_colors : 1;

    /* Whether to quantize colors before calculating error (slower, but
     * yields better results in palettized modes, especially 16/8) */
    guint use_quantized_error : 1;

    ChafaColorPair default_colors;
    guint work_factor_int;

    /* Character to use in cells where fg color == bg color. Typically
     * space, but could be something else depending on the symbol map. */
    gunichar blank_char;

    /* Character to use in cells where fg color == bg color and the color
     * is only legal in FG. Typically 0x2588 (solid block), but could be
     * something else depending on the symbol map. Can be zero if there is
     * no good candidate! */
    gunichar solid_char;

    ChafaCanvasConfig config;

    /* Used when setting pixel data */
    ChafaDither dither;

    /* This is NULL in CHAFA_PIXEL_MODE_SYMBOLS, otherwise one of:
     * (ChafaSixelCanvas *), (ChafaKittyCanvas *), (ChafaIterm2Canvas *) */
    gpointer pixel_canvas;

    /* Our palettes. Kind of a big structure, so they go last. */
    ChafaPalette fg_palette;
    ChafaPalette bg_palette;
};

G_END_DECLS

#endif /* __CHAFA_CANVAS_INTERNAL_H__ */
