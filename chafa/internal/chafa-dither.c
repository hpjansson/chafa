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

/* Split the texture into positive and negative byte modifiers in pixel
 * layout, with each row's columns stored twice so we can align the dither
 * matrix over dest. Then we use saturating add/sub. */
static void
gen_mods (ChafaDither *dither)
{
    gint n = 1 << dither->texture_size_shift;
    gint row, col, ch;

    dither->mods_row_stride = 2 * n * 4;
    dither->mods_pos = g_malloc0 ((gsize) n * dither->mods_row_stride);
    dither->mods_neg = g_malloc0 ((gsize) n * dither->mods_row_stride);

    for (row = 0; row < n; row++)
    {
        for (col = 0; col < n; col++)
        {
            gint texture_index = row * n + col;
            guint8 *pos = dither->mods_pos + (gsize) row * dither->mods_row_stride + col * 4;
            guint8 *neg = dither->mods_neg + (gsize) row * dither->mods_row_stride + col * 4;

            for (ch = 0; ch < 3; ch++)
            {
                gint mod = dither->mode == CHAFA_DITHER_MODE_NOISE
                    ? dither->texture_data [texture_index * TEXTURE_NOISE_N_CHANNELS + ch]
                    : dither->texture_data [texture_index];

                pos [ch] = CLAMP (mod, 0, 255);
                neg [ch] = CLAMP (-mod, 0, 255);
            }

            /* Duplicate */
            memcpy (pos + n * 4, pos, 4);
            memcpy (neg + n * 4, neg, 4);
        }
    }
}

/* Dither one 4-byte RGBA pixel against texture column col of a modifier row */
static inline guint32
dither_pixel_u32 (const guint8 *pos_row, const guint8 *neg_row, guint col, guint32 pixel)
{
    const guint8 *pos = pos_row + col * 4;
    const guint8 *neg = neg_row + col * 4;
    guint8 ch [4];
    gint i;

    memcpy (ch, &pixel, 4);

    for (i = 0; i < 3; i++)
    {
        gint c = ch [i] + pos [i];

        c = MIN (c, 255);
        c -= neg [i];
        ch [i] = MAX (c, 0);
    }

    memcpy (&pixel, ch, 4);
    return pixel;
}

void
chafa_dither_init (ChafaDither *dither, ChafaDitherMode mode,
                   gfloat intensity,
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
        gen_mods (dither);
    }
    else if (mode == CHAFA_DITHER_MODE_NOISE)
    {
        dither->texture_size_shift = 6;
        dither->texture_size_mask = (1 << 6) - 1;
        dither->texture_data = chafa_gen_noise_matrix (dither->intensity * 0.1f);
        gen_mods (dither);
    }
    else if (mode == CHAFA_DITHER_MODE_DIFFUSION)
    {
        dither->intensity = MIN (dither->intensity, 1.0f);
    }
}

void
chafa_dither_deinit (ChafaDither *dither)
{
    g_free (dither->texture_data);
    dither->texture_data = NULL;
    g_free (dither->mods_pos);
    dither->mods_pos = NULL;
    g_free (dither->mods_neg);
    dither->mods_neg = NULL;
}

void
chafa_dither_copy (const ChafaDither *src, ChafaDither *dest)
{
    memcpy (dest, src, sizeof (*dest));
    if (dest->texture_data)
    {
        gsize mods_size = (gsize) (1 << src->texture_size_shift) * src->mods_row_stride;

        if (dest->mode == CHAFA_DITHER_MODE_NOISE)
            dest->texture_data = g_memdup (src->texture_data,
                64 * 64 * TEXTURE_NOISE_N_CHANNELS * sizeof (gint));
        else
            dest->texture_data = g_memdup (src->texture_data, TEXTURE_SIZE * sizeof (gint));

        dest->mods_pos = g_memdup (src->mods_pos, mods_size);
        dest->mods_neg = g_memdup (src->mods_neg, mods_size);
    }
}

void
chafa_dither_pixels (const ChafaDither *dither, const guint32 *src, guint32 *dst,
                     gint x, gint y, gint n)
{
    const gint grain_shift = dither->grain_width_shift;
    const guint col_mask = dither->texture_size_mask;
    gsize row_ofs = (gsize) ((y >> dither->grain_height_shift) & dither->texture_size_mask)
        * dither->mods_row_stride;
    const guint8 *pos_row = dither->mods_pos + row_ofs;
    const guint8 *neg_row = dither->mods_neg + row_ofs;
    gint i = 0;

    g_assert (dither->mode == CHAFA_DITHER_MODE_ORDERED || dither->mode == CHAFA_DITHER_MODE_NOISE);

#ifdef HAVE_AVX2_INTRINSICS
    if (chafa_have_avx2 () && n >= 8)
    {
        gint n_vec;

        /* AVX2 loop must start on a multiple of eight columns, so do any
         * leading pixels here */
        for ( ; i < n && ((x + i) & 7); i++)
            dst [i] = dither_pixel_u32 (pos_row, neg_row, ((x + i) >> grain_shift) & col_mask, src [i]);

        n_vec = (n - i) & ~7;
        if (n_vec > 0)
        {
            chafa_dither_pixels_avx2 (src + i, dst + i,
                                      pos_row, neg_row, grain_shift, col_mask,
                                      ((x + i) >> grain_shift) & col_mask,
                                      n_vec);
            i += n_vec;
        }
    }
#endif

    for ( ; i < n; i++)
        dst [i] = dither_pixel_u32 (pos_row, neg_row, ((x + i) >> grain_shift) & col_mask, src [i]);
}
