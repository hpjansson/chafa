/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2025 Hans Petter Jansson
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

#include <mmintrin.h>
#include "chafa.h"
#include "internal/chafa-private.h"

void
chafa_extract_cell_mean_colors_mmx (const ChafaPixel *pixels, ChafaColorAccum *accums_out, const guint8 *cov)
{
    __m64 accum [2] = { 0 };
    __m64 m64b;
    gint i;

    m64b = _mm_setzero_si64 ();

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        __m64 *m64p1;
        __m64 m64a;

        m64p1 = &accum [cov [i]];
        m64a = _mm_cvtsi32_si64 (chafa_color8_to_u32 (pixels [i].col));
        m64a = _mm_unpacklo_pi8 (m64a, m64b);
        *m64p1 = _mm_adds_pi16 (*m64p1, m64a);
    }

    ((__m64 *) accums_out) [0] = accum [0];
    ((__m64 *) accums_out) [1] = accum [1];

    _mm_empty ();
}
