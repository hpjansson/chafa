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

#include "config.h"

#include <string.h>
#include <glib.h>

#include "chafa.h"
#include "internal/chafa-dither.h"
#include "internal/chafa-private.h"

#define TEXTURE_DIM_SHIFT 4
#define TEXTURE_DIM (1 << (TEXTURE_DIM_SHIFT))
#define TEXTURE_SIZE ((TEXTURE_DIM) * (TEXTURE_DIM))
#define TEXTURE_NOISE_N_CHANNELS 3

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

    if (mode == CHAFA_DITHER_MODE_ORDERED)
    {
        dither->texture_size_shift = TEXTURE_DIM_SHIFT;
        dither->texture_size_mask = TEXTURE_DIM - 1;
        dither->texture_data = chafa_gen_bayer_matrix (TEXTURE_DIM, intensity);
    }
    else if (mode == CHAFA_DITHER_MODE_NOISE)
    {
        dither->texture_size_shift = 6;
        dither->texture_size_mask = (1 << 6) - 1;
        dither->texture_data = chafa_gen_noise_matrix (dither->intensity * 0.1);
    }
    else if (mode == CHAFA_DITHER_MODE_DIFFUSION)
    {
        dither->intensity = MIN (dither->intensity, 1.0);
    }
}

void
chafa_dither_deinit (ChafaDither *dither)
{
    g_free (dither->texture_data);
    dither->texture_data = NULL;
}

void
chafa_dither_copy (const ChafaDither *src, ChafaDither *dest)
{
    memcpy (dest, src, sizeof (*dest));
    if (dest->texture_data)
    {
        if (dest->mode == CHAFA_DITHER_MODE_NOISE)
            dest->texture_data = g_memdup (src->texture_data,
                64 * 64 * TEXTURE_NOISE_N_CHANNELS * sizeof (gint));
        else
            dest->texture_data = g_memdup (src->texture_data, TEXTURE_SIZE * sizeof (gint));
    }
}

ChafaColor
chafa_dither_color (const ChafaDither *dither, ChafaColor color, gint x, gint y)
{
    gint texture_index = (((y >> dither->grain_height_shift) & dither->texture_size_mask)
                          << dither->texture_size_shift)
        + ((x >> dither->grain_width_shift) & dither->texture_size_mask);
    gint i;

    if (dither->mode == CHAFA_DITHER_MODE_ORDERED)
    {
        gint16 texture_mod = dither->texture_data [texture_index];

        for (i = 0; i < 3; i++)
        {
            gint16 c;

            c = (gint16) color.ch [i] + texture_mod;
            c = CLAMP (c, 0, 255);
            color.ch [i] = c;
        }
    }
    else if (dither->mode == CHAFA_DITHER_MODE_NOISE)
    {
        for (i = 0; i < 3; i++)
        {
            gint16 texture_mod = dither->texture_data [texture_index * TEXTURE_NOISE_N_CHANNELS + i];
            gint16 c;

            c = (gint16) color.ch [i] + texture_mod;
            c = CLAMP (c, 0, 255);
            color.ch [i] = c;
        }
    }
    else
    {
        g_assert_not_reached ();
    }

    return color;
}
