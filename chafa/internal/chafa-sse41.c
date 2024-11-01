/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2024 Hans Petter Jansson
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

#include <smmintrin.h>
#include "chafa.h"
#include "internal/chafa-private.h"

gint
calc_error_sse41 (const ChafaPixel *pixels, const ChafaColorPair *color_pair, const guint8 *cov)
{
    const guint32 *u32p0 = (const guint32 *) pixels;
    guint32 cpair_u32 [2];
    __m128i err = { 0 };
    gint i;

    memcpy (&cpair_u32 [0], color_pair->colors [0].ch, sizeof (guint32));
    memcpy (&cpair_u32 [1], color_pair->colors [1].ch, sizeof (guint32));

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        __m128i t0, t1, t;

        t0 = _mm_cvtepu8_epi32 (_mm_cvtsi32_si128 (u32p0 [i]));
        t1 = _mm_cvtepu8_epi32 (_mm_cvtsi32_si128 (cpair_u32 [cov [i]]));

        t = t0 - t1;
        t = _mm_mullo_epi32 (t, t);
        err += t;
    }

    /* FIXME: Perhaps not ideal */
    return _mm_extract_epi32 (err, 0) + _mm_extract_epi32 (err, 1)
        + _mm_extract_epi32 (err, 2) + _mm_extract_epi32 (err, 3);
}
