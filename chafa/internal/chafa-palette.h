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

#ifndef __CHAFA_PALETTE_H__
#define __CHAFA_PALETTE_H__

#include <glib.h>
#include "internal/chafa-color.h"
#include "internal/chafa-color-table.h"

G_BEGIN_DECLS

typedef enum
{
    CHAFA_PALETTE_TYPE_DYNAMIC_256,
    CHAFA_PALETTE_TYPE_FIXED_256,
    CHAFA_PALETTE_TYPE_FIXED_240,
    CHAFA_PALETTE_TYPE_FIXED_16,
    CHAFA_PALETTE_TYPE_FIXED_8,
    CHAFA_PALETTE_TYPE_FIXED_FGBG
}
ChafaPaletteType;

typedef struct
{
    ChafaPaletteType type;
    ChafaPaletteColor colors [CHAFA_PALETTE_INDEX_MAX];
    ChafaColorTable table [CHAFA_COLOR_SPACE_MAX];
    gint first_color;
    gint n_colors;
    gint alpha_threshold;
    gint transparent_index;

    /* Number of representable levels per color channel, [2..256]. Colors
     * entering the palette are snapped to the nearest level, so lookups
     * operate on what the output can actually display. */
    gint n_channel_levels;
    guint8 channel_level_lut [256];
}
ChafaPalette;

/* Global init */
void chafa_init_palette (void);

void chafa_palette_init (ChafaPalette *palette_out, ChafaPaletteType type);
void chafa_palette_deinit (ChafaPalette *palette);

void chafa_palette_copy (const ChafaPalette *src, ChafaPalette *dest);
void chafa_palette_generate (ChafaPalette *palette_out, gconstpointer pixels, gsize n_pixels,
                             ChafaColorSpace color_space, gfloat quality);

ChafaPaletteType chafa_palette_get_type (const ChafaPalette *palette);

gint chafa_palette_get_first_color (const ChafaPalette *palette);
gint chafa_palette_get_n_colors (const ChafaPalette *palette);

gint chafa_palette_lookup_nearest (const ChafaPalette *palette, ChafaColorSpace color_space,
                                   const ChafaColor *color, ChafaColorCandidates *candidates);

gint chafa_palette_lookup_with_error (const ChafaPalette *palette, ChafaColorSpace color_space,
                                      ChafaColor color, ChafaColorAccum *error_inout);

const ChafaColor *chafa_palette_get_color (const ChafaPalette *palette, ChafaColorSpace color_space,
                                           gint index);
void chafa_palette_set_color (ChafaPalette *palette, gint index, const ChafaColor *color);

gint chafa_palette_get_alpha_threshold (const ChafaPalette *palette);
void chafa_palette_set_alpha_threshold (ChafaPalette *palette, gint alpha_threshold);

gint chafa_palette_get_transparent_index (const ChafaPalette *palette);
void chafa_palette_set_transparent_index (ChafaPalette *palette, gint index);

gint chafa_palette_get_channel_levels (const ChafaPalette *palette);
void chafa_palette_set_channel_levels (ChafaPalette *palette, gint n_levels);

static inline gint
chafa_palette_channel_to_level (const ChafaPalette *palette, gint ch_value)
{
    return (ch_value * (palette->n_channel_levels - 1) + 127) / 255;
}

static inline gint
chafa_palette_level_to_channel (const ChafaPalette *palette, gint level)
{
    return (level * 255 + (palette->n_channel_levels - 1) / 2)
        / (palette->n_channel_levels - 1);
}

/* Snap a color's RGB channels to the palette's channel resolution */
static inline ChafaColor
chafa_palette_snap_color (const ChafaPalette *palette, ChafaColor color)
{
    color.ch [0] = palette->channel_level_lut [color.ch [0]];
    color.ch [1] = palette->channel_level_lut [color.ch [1]];
    color.ch [2] = palette->channel_level_lut [color.ch [2]];
    return color;
}

G_END_DECLS

#endif /* __CHAFA_PALETTE_H__ */
