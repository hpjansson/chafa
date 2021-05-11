/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2021 Hans Petter Jansson
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

#include <nmmintrin.h>
#include "chafa.h"
#include "internal/chafa-private.h"

gint
chafa_pop_count_u64_builtin (guint64 v)
{
#if defined(HAVE_POPCNT64_INTRINSICS)
    return (gint) _mm_popcnt_u64 (v);
#else /* HAVE_POPCNT32_INTRINSICS */
    const guint32 *w = (const guint32 *) &v;
    return (gint) _mm_popcnt_u32(w[0]) + _mm_popcnt_u32(w[1]);
#endif
}

void
chafa_pop_count_vu64_builtin (const guint64 *vv, gint *vc, gint n)
{
    while (n--)
    {
#if defined(HAVE_POPCNT64_INTRINSICS)
        *(vc++) = _mm_popcnt_u64 (*(vv++));
#else /* HAVE_POPCNT32_INTRINSICS */
        const guint32 *w = (const guint32 *)vv;
        *(vc++) = _mm_popcnt_u32(w[0]) + _mm_popcnt_u32(w[1]);
        vv++;
#endif
    }
}

void
chafa_hamming_distance_vu64_builtin (guint64 a, const guint64 *vb, gint *vc, gint n)
{
#if defined(HAVE_POPCNT64_INTRINSICS)
    while (n >= 4)
    {
        n -= 4;
        *(vc++) = _mm_popcnt_u64 (a ^ *(vb++));
        *(vc++) = _mm_popcnt_u64 (a ^ *(vb++));
        *(vc++) = _mm_popcnt_u64 (a ^ *(vb++));
        *(vc++) = _mm_popcnt_u64 (a ^ *(vb++));
    }

    while (n--)
    {
        *(vc++) = _mm_popcnt_u64 (a ^ *(vb++));
    }
#else /* HAVE_POPCNT32_INTRINSICS */
    const guint32 *aa = (const guint32 *) &a;
    const guint32 *wb = (const guint32 *) vb;

    while (n--)
    {
        *(vc++) = _mm_popcnt_u32 (aa [0] ^ wb [0]) + _mm_popcnt_u32 (aa [1] ^ wb [1]);
        wb += 2;
    }
#endif
}

/* Two bitmaps per item (a points to a pair, vb points to array of pairs) */
void
chafa_hamming_distance_2_vu64_builtin (const guint64 *a, const guint64 *vb, gint *vc, gint n)
{
#if defined(HAVE_POPCNT64_INTRINSICS)
    while (n >= 4)
    {
        n -= 4;
        *vc = _mm_popcnt_u64 (a [0] ^ *(vb++));
        *(vc++) += _mm_popcnt_u64 (a [1] ^ *(vb++));
        *vc = _mm_popcnt_u64 (a [0] ^ *(vb++));
        *(vc++) += _mm_popcnt_u64 (a [1] ^ *(vb++));
        *vc = _mm_popcnt_u64 (a [0] ^ *(vb++));
        *(vc++) += _mm_popcnt_u64 (a [1] ^ *(vb++));
        *vc = _mm_popcnt_u64 (a [0] ^ *(vb++));
        *(vc++) += _mm_popcnt_u64 (a [1] ^ *(vb++));
    }

    while (n--)
    {
        *vc = _mm_popcnt_u64 (a [0] ^ *(vb++));
        *(vc++) += _mm_popcnt_u64 (a [1] ^ *(vb++));
    }
#else /* HAVE_POPCNT32_INTRINSICS */
    const guint32 *aa = (const guint32 *) a;
    const guint32 *wb = (const guint32 *) vb;

    while (n--)
    {
        *(vc++) = _mm_popcnt_u32 (aa [0] ^ wb [0]) + _mm_popcnt_u32 (aa [1] ^ wb [1])
            + _mm_popcnt_u32 (aa [2] ^ wb [2]) + _mm_popcnt_u32 (aa [3] ^ wb [3]);
        wb += 4;
    }
#endif
}
