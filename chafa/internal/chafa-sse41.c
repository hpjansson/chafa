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

#include "config.h"

#include <smmintrin.h>
#include "chafa.h"
#include "internal/chafa-private.h"

gint
calc_error_sse41 (const ChafaPixel *pixels, const ChafaColorPair *color_pair, const guint8 *cov)
{
    const guint32 *u32p0 = (const guint32 *) pixels;
    const guint32 *u32p1 = (const guint32 *) color_pair->colors;
    __m128i err4 = { 0 };
    const gint32 *e = (gint32 *) &err4;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        __m128i t0, t1, t;

        t0 = _mm_cvtepu8_epi32 (_mm_cvtsi32_si128 (u32p0 [i]));
        t1 = _mm_cvtepu8_epi32 (_mm_cvtsi32_si128 (u32p1 [cov [i]]));

        t = t0 - t1;
        t = _mm_mullo_epi32 (t, t);
        err4 += t;
    }

    return e [0] + e [1] + e [2];
}
