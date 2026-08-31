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

#ifndef __CHAFA_DITHER_H__
#define __CHAFA_DITHER_H__

#include <string.h>
#include "internal/chafa-palette.h"

G_BEGIN_DECLS

typedef struct
{
    ChafaDitherMode mode;
    gfloat intensity;
    gint grain_width_shift;
    gint grain_height_shift;

    gint texture_size_shift;
    guint texture_size_mask;
    gint *texture_data;

    /* The texture split into the positive and negative parts of each modifier,
     * as bytes in pixel layout (R, G, B, 0). For SIMD with saturation ops.
     * Only for the ordered and noise modes. */
    guint8 *mods_pos;
    guint8 *mods_neg;
    gint mods_row_stride;  /* Bytes per texture row: 2 * columns * 4 */
}
ChafaDither;

void chafa_dither_init (ChafaDither *dither, ChafaDitherMode mode,
                        gfloat intensity,
                        gint grain_width, gint grain_height);
void chafa_dither_deinit (ChafaDither *dither);
void chafa_dither_copy (const ChafaDither *src, ChafaDither *dest);

/* Apply the dither to n consective pixels of row y starting at column x,
 * reading 4-byte RGBA pixels from src and writing them to dst, which may
 * be the same buffer. The alpha channel is left alone. Ordered and noise
 * modes only. */
void chafa_dither_pixels (const ChafaDither *dither, const guint32 *src, guint32 *dst,
                          gint x, gint y, gint n);

/* Dither a single pixel. */
static inline ChafaColor
chafa_dither_color (const ChafaDither *dither, ChafaColor color, gint x, gint y)
{
    gsize ofs = (gsize) ((y >> dither->grain_height_shift) & dither->texture_size_mask) * dither->mods_row_stride
        + (((x >> dither->grain_width_shift) & dither->texture_size_mask) << 2);
    const guint8 *pos = dither->mods_pos + ofs;
    const guint8 *neg = dither->mods_neg + ofs;
    gint i;

    for (i = 0; i < 3; i++)
    {
        gint c = color.ch [i] + pos [i];

        c = MIN (c, 255);
        c -= neg [i];
        color.ch [i] = MAX (c, 0);
    }

    return color;
}

G_END_DECLS

#endif /* __CHAFA_DITHER_H__ */
