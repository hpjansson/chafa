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

#include "config.h"
#include "chafa.h"
#include "internal/chafa-dither.h"
#include "internal/chafa-private.h"

#define BAYER_MATRIX_DIM_SHIFT 4
#define BAYER_MATRIX_DIM (1 << (BAYER_MATRIX_DIM_SHIFT))
#define BAYER_MATRIX_SIZE ((BAYER_MATRIX_DIM) * (BAYER_MATRIX_DIM))

static gint
calc_grain_shift (gint size)
{
    switch (size)
    {
        case 1:
            return 0;
        case 2:
            return 1;
        case 4:
            return 2;
        case 8:
            return 3;
        default:
            g_assert_not_reached ();
    }

    return 0;
}

void
chafa_dither_init (ChafaDither *dither, ChafaDitherMode mode,
                   gdouble intensity,
                   gint grain_width, gint grain_height)
{
    memset (dither, 0, sizeof (*dither));

    dither->mode = mode;
    dither->intensity = intensity;
    dither->grain_width_shift = calc_grain_shift (grain_width);
    dither->grain_height_shift = calc_grain_shift (grain_height);
    dither->bayer_size_shift = BAYER_MATRIX_DIM_SHIFT;
    dither->bayer_size_mask = BAYER_MATRIX_DIM - 1;

    if (mode == CHAFA_DITHER_MODE_ORDERED)
    {
        dither->bayer_matrix = chafa_gen_bayer_matrix (BAYER_MATRIX_DIM, intensity);
    }
    else if (mode == CHAFA_DITHER_MODE_DIFFUSION)
    {
        dither->intensity = MIN (dither->intensity, 1.0);
    }
}

void
chafa_dither_deinit (ChafaDither *dither)
{
    g_free (dither->bayer_matrix);
    dither->bayer_matrix = NULL;
}

void
chafa_dither_copy (const ChafaDither *src, ChafaDither *dest)
{
    memcpy (dest, src, sizeof (*dest));
    if (dest->bayer_matrix)
        dest->bayer_matrix = g_memdup (src->bayer_matrix, BAYER_MATRIX_SIZE * sizeof (gint));
}

ChafaColor
chafa_dither_color_ordered (const ChafaDither *dither, ChafaColor color, gint x, gint y)
{
    gint bayer_index = (((y >> dither->grain_height_shift) & dither->bayer_size_mask)
                        << dither->bayer_size_shift)
                       + ((x >> dither->grain_width_shift) & dither->bayer_size_mask);
    gint16 bayer_mod = dither->bayer_matrix [bayer_index];
    gint i;

    for (i = 0; i < 3; i++)
    {
        gint16 c;

        c = (gint16) color.ch [i] + bayer_mod;
        c = CLAMP (c, 0, 255);
        color.ch [i] = c;
    }

    return color;
}
