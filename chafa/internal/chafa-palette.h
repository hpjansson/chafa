/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019 Hans Petter Jansson
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

#ifndef __CHAFA_PALETTE_H__
#define __CHAFA_PALETTE_H__

#include <glib.h>
#include "internal/chafa-color.h"

G_BEGIN_DECLS

typedef enum
{
    CHAFA_PALETTE_TYPE_DYNAMIC_256,
    CHAFA_PALETTE_TYPE_FIXED_256,
    CHAFA_PALETTE_TYPE_FIXED_240,
    CHAFA_PALETTE_TYPE_FIXED_16,
    CHAFA_PALETTE_TYPE_FIXED_FGBG
}
ChafaPaletteType;

#define CHAFA_OCT_TREE_INDEX_NULL -1

typedef struct
{
    gint8 branch_bit;
    gint8 n_children;
    guint16 prefix [3];
    gint16 child_index [8];
}
ChafaPaletteOctNode;

typedef struct
{
    ChafaPaletteType type;
    ChafaPaletteColor colors [CHAFA_PALETTE_INDEX_MAX];
    gint n_colors;
    gint alpha_threshold;
    gint16 oct_tree_root [CHAFA_COLOR_SPACE_MAX];
    gint16 oct_tree_first_free [CHAFA_COLOR_SPACE_MAX];
    ChafaPaletteOctNode oct_tree [CHAFA_COLOR_SPACE_MAX] [256];
}
ChafaPalette;

void chafa_palette_init (ChafaPalette *palette_out, ChafaPaletteType type);
void chafa_palette_generate (ChafaPalette *palette_out, gconstpointer pixels, gint n_pixels,
                             ChafaColorSpace color_space, gint alpha_threshold);

gint chafa_palette_lookup_nearest (const ChafaPalette *palette, ChafaColorSpace color_space,
                                   const ChafaColor *color, ChafaColorCandidates *candidates);

const ChafaColor *chafa_palette_get_color (const ChafaPalette *palette, ChafaColorSpace color_space,
                                           gint index);
void chafa_palette_set_color (ChafaPalette *palette, gint index, const ChafaColor *color);

G_END_DECLS

#endif /* __CHAFA_PALETTE_H__ */
