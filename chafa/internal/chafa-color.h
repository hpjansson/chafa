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

#ifndef __CHAFA_COLOR_H__
#define __CHAFA_COLOR_H__

#include <glib.h>
#include "chafa.h"

G_BEGIN_DECLS

#define CHAFA_PALETTE_INDEX_TRANSPARENT 256
#define CHAFA_PALETTE_INDEX_FG 257
#define CHAFA_PALETTE_INDEX_BG 258

#define CHAFA_PALETTE_INDEX_MAX 259

#define CHAFA_COLOR8_U32(col) (*((guint32 *) (col).ch))

/* Color space agnostic */
typedef struct
{
    guint8 ch [4];
}
ChafaColor;

/* BG/FG indexes must be 0 and 1 respectively, corresponding to
 * coverage bitmap values */
#define CHAFA_COLOR_PAIR_BG 0
#define CHAFA_COLOR_PAIR_FG 1

typedef struct
{
    ChafaColor colors [2];
}
ChafaColorPair;

typedef struct
{
    union
    {
        guint32 u32;
        ChafaColor col;
    } u;
}
ChafaColorConv;

static inline ChafaColor
chafa_color8_fetch_from_rgba8 (gconstpointer p)
{
    const guint32 *p32 = (const guint32 *) p;
    ChafaColorConv cc;
    cc.u.u32 = *p32;
    return cc.u.col;
}

static inline void
chafa_color8_store_to_rgba8 (ChafaColor col, gpointer p)
{
    guint32 *p32 = (guint32 *) p;
    ChafaColorConv cc;
    cc.u.col = col;
    *p32 = cc.u.u32;
}

static inline ChafaColor
chafa_color_average_2 (ChafaColor color_a, ChafaColor color_b)
{
    ChafaColor avg = { 0 };

    CHAFA_COLOR8_U32 (avg) =
        ((CHAFA_COLOR8_U32 (color_a) >> 1) & 0x7f7f7f7f)
        + ((CHAFA_COLOR8_U32 (color_b) >> 1) & 0x7f7f7f7f);

    return avg;
}

typedef struct
{
    ChafaColor col [CHAFA_COLOR_SPACE_MAX];
}
ChafaPaletteColor;

typedef struct
{
    gint16 ch [4];
}
ChafaColorAccum;

typedef struct
{
    ChafaColor col;
}
ChafaPixel;

/* Color selection candidate pair */

typedef struct
{
    gint16 index [2];
    gint error [2];
}
ChafaColorCandidates;

/* Internal API */

guint32 chafa_pack_color (const ChafaColor *color) G_GNUC_PURE;
void chafa_unpack_color (guint32 packed, ChafaColor *color_out);

#define chafa_color_accum_add(d, s) \
G_STMT_START { \
  (d)->ch [0] += (s)->ch [0]; (d)->ch [1] += (s)->ch [1]; (d)->ch [2] += (s)->ch [2]; (d)->ch [3] += (s)->ch [3]; \
} G_STMT_END

#define chafa_color_diff_fast(col_a, col_b) \
    (((gint) (col_b)->ch [0] - (gint) (col_a)->ch [0]) * ((gint) (col_b)->ch [0] - (gint) (col_a)->ch [0]) \
  + ((gint) (col_b)->ch [1] - (gint) (col_a)->ch [1]) * ((gint) (col_b)->ch [1] - (gint) (col_a)->ch [1]) \
  + ((gint) (col_b)->ch [2] - (gint) (col_a)->ch [2]) * ((gint) (col_b)->ch [2] - (gint) (col_a)->ch [2]))

void chafa_color_accum_div_scalar (ChafaColorAccum *color, gint scalar);

void chafa_color_rgb_to_din99d (const ChafaColor *rgb, ChafaColor *din99);

G_END_DECLS

#endif /* __CHAFA_COLOR_H__ */
