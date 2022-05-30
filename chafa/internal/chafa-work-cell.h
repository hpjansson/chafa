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

#ifndef __CHAFA_WORK_CELL_H__
#define __CHAFA_WORK_CELL_H__

G_BEGIN_DECLS

typedef struct ChafaWorkCell ChafaWorkCell;

struct ChafaWorkCell
{
    ChafaPixel pixels [CHAFA_SYMBOL_N_PIXELS];
    guint8 pixels_sorted_index [4] [CHAFA_SYMBOL_N_PIXELS];
    guint8 have_pixels_sorted_by_channel [4];
    gint dominant_channel;
};

/* Currently unused */
typedef enum
{
    CHAFA_PICK_CONSIDER_INVERTED = (1 << 0),
    CHAFA_PICK_HAVE_ALPHA = (1 << 1)
}
ChafaPickFlags;

void chafa_work_cell_init (ChafaWorkCell *wcell, const ChafaPixel *src_image,
                           gint src_width, gint cx, gint cy);

void chafa_work_cell_get_mean_colors_for_symbol (const ChafaWorkCell *wcell, const ChafaSymbol *sym,
                                                 ChafaColorPair *color_pair_out);
void chafa_work_cell_get_median_colors_for_symbol (ChafaWorkCell *wcell, const ChafaSymbol *sym,
                                                   ChafaColorPair *color_pair_out);
void chafa_work_cell_get_contrasting_color_pair (ChafaWorkCell *wcell, ChafaColorPair *color_pair_out);
void chafa_work_cell_calc_mean_color (const ChafaWorkCell *wcell, ChafaColor *color_out);
guint64 chafa_work_cell_to_bitmap (const ChafaWorkCell *wcell, const ChafaColorPair *color_pair);

G_END_DECLS

#endif /* __CHAFA_WORK_CELL_H__ */
