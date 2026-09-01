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

#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>
#include "chafa.h"
#include "internal/chafa-private.h"

/* _mm_extract_epi64() (pextrq) is not available in 32-bit mode. Work around
 * it. This needs to be a macro, as the compiler expects an integer constant
 * for n. */
#if defined __x86_64__ && !defined __ILP32__
# define extract_128_epi64(i, n) _mm_extract_epi64 ((i), (n))
#else
# define extract_128_epi64(i, n) \
    ((((guint64) _mm_extract_epi32 ((i), (n) * 2 + 1)) << 32) \
     | _mm_extract_epi32 ((i), (n) * 2))
#endif

gint
chafa_calc_cell_error_avx2 (const ChafaPixel *pixels, const ChafaColorPair *color_pair,
                            const guint32 *sym_mask_u32)
{
    __m256i err_8x_u32 = { 0 };
    __m128i err_4x_u32;
    __m128i fg_4x_u32, bg_4x_u32;
    __m256i fg_4x_u64, bg_4x_u64;
    const __m128i *pixels_4x_p = (const __m128i *) pixels;
    const __m128i *sym_mask_4x_p = (const __m128i *) sym_mask_u32;
    const __m256i rgb_mask = _mm256_set_epi16 (0, -1, -1, -1, 0, -1, -1, -1,
                                               0, -1, -1, -1, 0, -1, -1, -1);
    gint i;

    fg_4x_u32 = _mm_set1_epi32 (chafa_color8_to_u32 (color_pair->colors [CHAFA_COLOR_PAIR_FG]));
    fg_4x_u64 = _mm256_cvtepu8_epi16 (fg_4x_u32);

    bg_4x_u32 = _mm_set1_epi32 (chafa_color8_to_u32 (color_pair->colors [CHAFA_COLOR_PAIR_BG]));
    bg_4x_u64 = _mm256_cvtepu8_epi16 (bg_4x_u32);

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS / 4; i++)
    {
        __m128i pixels_4x, sym_mask_4x;
        __m256i p0, m0, fg0, bg0, d0;

        pixels_4x = _mm_loadu_si128 (pixels_4x_p++);
        sym_mask_4x = _mm_loadu_si128 (sym_mask_4x_p++);

        p0 = _mm256_cvtepu8_epi16 (pixels_4x);
        m0 = _mm256_cvtepi8_epi16 (sym_mask_4x);
        fg0 = _mm256_and_si256 (m0, _mm256_sub_epi16 (fg_4x_u64, p0));
        bg0 = _mm256_andnot_si256 (m0, _mm256_sub_epi16 (bg_4x_u64, p0));
        d0 = _mm256_or_si256 (fg0, bg0);

        /* Drop the alpha lane */
        d0 = _mm256_and_si256 (d0, rgb_mask);

        d0 = _mm256_madd_epi16 (d0, d0);

        err_8x_u32 = _mm256_add_epi32 (err_8x_u32, d0);
    }

    err_4x_u32 = _mm_add_epi32 (_mm256_extracti128_si256 (err_8x_u32, 0),
                                _mm256_extracti128_si256 (err_8x_u32, 1));
    err_4x_u32 = _mm_hadd_epi32 (err_4x_u32, err_4x_u32);
    err_4x_u32 = _mm_hadd_epi32 (err_4x_u32, err_4x_u32);

    return _mm_extract_epi32 (err_4x_u32, 0);
}

void
chafa_extract_cell_mean_colors_avx2 (const ChafaPixel *pixels, ChafaColorAccum *accums_out,
                                     const guint32 *sym_mask_u32)
{
    const __m128i *pixels_4x_p = (const __m128i *) pixels;
    const __m128i *sym_mask_4x_p = (const __m128i *) sym_mask_u32;
    __m256i accum_fg = { 0 };
    __m256i accum_bg = { 0 };
    __m128i accum_fg_128;
    __m128i accum_bg_128;
    guint64 accums_u64 [2];
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS / 4; i++)
    {
        __m128i pixels_4x, sym_mask_4x;

        pixels_4x = _mm_loadu_si128 (pixels_4x_p++);
        sym_mask_4x = _mm_loadu_si128 (sym_mask_4x_p++);

        accum_fg = _mm256_add_epi16 (accum_fg,
            _mm256_cvtepu8_epi16 (_mm_and_si128 (sym_mask_4x, pixels_4x)));
        accum_bg = _mm256_add_epi16 (accum_bg,
            _mm256_cvtepu8_epi16 (_mm_andnot_si128 (sym_mask_4x, pixels_4x)));
    }

    accum_bg_128 = _mm_add_epi16 (_mm256_extracti128_si256 (accum_bg, 0),
                                  _mm256_extracti128_si256 (accum_bg, 1));
    accums_u64 [0] =
        extract_128_epi64 (accum_bg_128, 0)
        + extract_128_epi64 (accum_bg_128, 1);

    accum_fg_128 = _mm_add_epi16 (_mm256_extracti128_si256 (accum_fg, 0),
                                  _mm256_extracti128_si256 (accum_fg, 1));
    accums_u64 [1] =
        extract_128_epi64 (accum_fg_128, 0)
        + extract_128_epi64 (accum_fg_128, 1);

    memcpy (accums_out, accums_u64, 2 * sizeof (guint64));
}

/* ceil (2^24 / index). Divide by zero is defined as zero. */
static const guint32 invdiv24 [257] =
{
    0x0000000, 0x1000000, 0x0800000, 0x0555556, 0x0400000, 0x0333334, 0x02aaaab, 0x024924a,
    0x0200000, 0x01c71c8, 0x019999a, 0x01745d2, 0x0155556, 0x013b13c, 0x0124925, 0x0111112,
    0x0100000, 0x00f0f10, 0x00e38e4, 0x00d7944, 0x00ccccd, 0x00c30c4, 0x00ba2e9, 0x00b2165,
    0x00aaaab, 0x00a3d71, 0x009d89e, 0x0097b43, 0x0092493, 0x008d3dd, 0x0088889, 0x0084211,
    0x0080000, 0x007c1f1, 0x0078788, 0x0075076, 0x0071c72, 0x006eb3f, 0x006bca2, 0x006906a,
    0x0066667, 0x0063e71, 0x0061862, 0x005f418, 0x005d175, 0x005b05c, 0x00590b3, 0x0057263,
    0x0055556, 0x0053979, 0x0051eb9, 0x0050506, 0x004ec4f, 0x004d488, 0x004bda2, 0x004a791,
    0x004924a, 0x0047dc2, 0x00469ef, 0x00456c8, 0x0044445, 0x004325d, 0x0042109, 0x0041042,
    0x0040000, 0x003f040, 0x003e0f9, 0x003d227, 0x003c3c4, 0x003b5cd, 0x003a83b, 0x0039b0b,
    0x0038e39, 0x00381c1, 0x00375a0, 0x00369d1, 0x0035e51, 0x003531e, 0x0034835, 0x0033d92,
    0x0033334, 0x0032917, 0x0031f39, 0x0031598, 0x0030c31, 0x0030304, 0x002fa0c, 0x002f14a,
    0x002e8bb, 0x002e05d, 0x002d82e, 0x002d02e, 0x002c85a, 0x002c0b1, 0x002b932, 0x002b1db,
    0x002aaab, 0x002a3a1, 0x0029cbd, 0x00295fb, 0x0028f5d, 0x00288e0, 0x0028283, 0x0027c46,
    0x0027628, 0x0027028, 0x0026a44, 0x002647d, 0x0025ed1, 0x0025940, 0x00253c9, 0x0024e6b,
    0x0024925, 0x00243f7, 0x0023ee1, 0x00239e1, 0x00234f8, 0x0023024, 0x0022b64, 0x00226ba,
    0x0022223, 0x0021d9f, 0x002192f, 0x00214d1, 0x0021085, 0x0020c4a, 0x0020821, 0x0020409,
    0x0020000, 0x001fc08, 0x001f820, 0x001f447, 0x001f07d, 0x001ecc1, 0x001e914, 0x001e574,
    0x001e1e2, 0x001de5e, 0x001dae7, 0x001d77c, 0x001d41e, 0x001d0cc, 0x001cd86, 0x001ca4c,
    0x001c71d, 0x001c3f9, 0x001c0e1, 0x001bdd3, 0x001bad0, 0x001b7d7, 0x001b4e9, 0x001b204,
    0x001af29, 0x001ac58, 0x001a98f, 0x001a6d1, 0x001a41b, 0x001a16e, 0x0019ec9, 0x0019c2e,
    0x001999a, 0x001970f, 0x001948c, 0x0019210, 0x0018f9d, 0x0018d31, 0x0018acc, 0x001886f,
    0x0018619, 0x00183ca, 0x0018182, 0x0017f41, 0x0017d06, 0x0017ad3, 0x00178a5, 0x001767e,
    0x001745e, 0x0017243, 0x001702f, 0x0016e20, 0x0016c17, 0x0016a14, 0x0016817, 0x001661f,
    0x001642d, 0x0016240, 0x0016059, 0x0015e76, 0x0015c99, 0x0015ac1, 0x00158ee, 0x001571f,
    0x0015556, 0x0015391, 0x00151d1, 0x0015016, 0x0014e5f, 0x0014cac, 0x0014afe, 0x0014954,
    0x00147af, 0x001460d, 0x0014470, 0x00142d7, 0x0014142, 0x0013fb1, 0x0013e23, 0x0013c9a,
    0x0013b14, 0x0013992, 0x0013814, 0x0013699, 0x0013522, 0x00133af, 0x001323f, 0x00130d2,
    0x0012f69, 0x0012e03, 0x0012ca0, 0x0012b41, 0x00129e5, 0x001288c, 0x0012736, 0x00125e3,
    0x0012493, 0x0012346, 0x00121fc, 0x00120b5, 0x0011f71, 0x0011e2f, 0x0011cf1, 0x0011bb5,
    0x0011a7c, 0x0011946, 0x0011812, 0x00116e1, 0x00115b2, 0x0011486, 0x001135d, 0x0011236,
    0x0011112, 0x0010ff0, 0x0010ed0, 0x0010db3, 0x0010c98, 0x0010b7f, 0x0010a69, 0x0010954,
    0x0010843, 0x0010733, 0x0010625, 0x001051a, 0x0010411, 0x001030a, 0x0010205, 0x0010102,
    0x0010000
};

/* Divisor must be in the range [0..256] inclusive. */
void
chafa_color_accum_div_scalar_avx2 (ChafaColorAccum *accum, guint16 divisor)
{
    __m128i accum_128, divisor_128;
    guint64 accum_u64;

    /* Not using _mm_loadu_si64() here because it's not available on
     * older versions of GCC. The opcode is the same. */
    accum_128 = _mm_loadl_epi64 ((const __m128i *) accum);

    accum_128 = _mm_cvtepi16_epi32 (accum_128);
    divisor_128 = _mm_set1_epi32 (invdiv24 [divisor]);
    accum_128 = _mm_srli_epi32 (_mm_mullo_epi32 (accum_128, divisor_128), 24);
    accum_128 = _mm_packs_epi32 (accum_128, accum_128);

    accum_u64 = extract_128_epi64 (accum_128, 0);
    memcpy (accum, &accum_u64, sizeof (guint64));
}

/* Each candidate's distance is packed with its index as
 * (distance << 14) | index, so a single unsigned min yields both the
 * smallest distance and the lowest index as a tie-breaker. The distance
 * is at most 4 * 255^2 < 2^18, leaving 14 bits for the index. */
#define PACKED_INDEX_BITS 14
#define PACKED_INDEX_MASK ((1 << PACKED_INDEX_BITS) - 1)

/* Returns packed (distance, index) of the closest element */
static inline guint32
find_nearest_u32_packed (const guint32 *array, gint n, guint32 want)
{
    const __m256i lane_ids = _mm256_setr_epi32 (0, 1, 2, 3, 4, 5, 6, 7);
    const __m256i even_mask = _mm256_set1_epi32 (0x00ff00ff);
    const __m256i all_ones = _mm256_set1_epi32 (-1);
    const __m256i want_32 = _mm256_set1_epi32 (want);
    const __m256i want_even = _mm256_and_si256 (want_32, even_mask);
    const __m256i want_odd = _mm256_srli_epi16 (want_32, 8);
    __m256i best = all_ones;
    __m256i idx = lane_ids;
    __m128i best_128;
    gint i;

    g_assert (n > 0);
    g_assert (n <= (1 << PACKED_INDEX_BITS));

    for (i = 0; i + 8 <= n; i += 8)
    {
        __m256i c, even, odd, dist;

        c = _mm256_loadu_si256 ((const __m256i *) (array + i));

        /* Split each u32 into two 16-bit pairs. Bytes 0 and 2 in the even
         * halves, bytes 1 and 3 in the odd halves. The differences fit in
         * i16, and madd then sums the two squares of each pair into the i32
         * slot of the color they came from, keeping the array's order. */
        even = _mm256_sub_epi16 (_mm256_and_si256 (c, even_mask), want_even);
        odd = _mm256_sub_epi16 (_mm256_srli_epi16 (c, 8), want_odd);
        dist = _mm256_add_epi32 (_mm256_madd_epi16 (even, even),
                                 _mm256_madd_epi16 (odd, odd));

        best = _mm256_min_epu32 (best,
                                 _mm256_or_si256 (_mm256_slli_epi32 (dist, PACKED_INDEX_BITS), idx));
        idx = _mm256_add_epi32 (idx, _mm256_set1_epi32 (8));
    }

    if (i < n)
    {
        /* Epilogue: Load only the valid lanes and disqualify the rest with the
         * maximum packed value. */
        __m256i valid, c, even, odd, dist, packed;

        valid = _mm256_cmpgt_epi32 (_mm256_set1_epi32 (n - i), lane_ids);
        c = _mm256_maskload_epi32 ((const gint *) (array + i), valid);

        even = _mm256_sub_epi16 (_mm256_and_si256 (c, even_mask), want_even);
        odd = _mm256_sub_epi16 (_mm256_srli_epi16 (c, 8), want_odd);
        dist = _mm256_add_epi32 (_mm256_madd_epi16 (even, even),
                                 _mm256_madd_epi16 (odd, odd));

        packed = _mm256_or_si256 (_mm256_slli_epi32 (dist, PACKED_INDEX_BITS), idx);
        packed = _mm256_or_si256 (packed, _mm256_andnot_si256 (valid, all_ones));
        best = _mm256_min_epu32 (best, packed);
    }

    /* Horizontal min over the eight lanes */
    best_128 = _mm_min_epu32 (_mm256_castsi256_si128 (best),
                              _mm256_extracti128_si256 (best, 1));
    best_128 = _mm_min_epu32 (best_128, _mm_shuffle_epi32 (best_128, _MM_SHUFFLE (1, 0, 3, 2)));
    best_128 = _mm_min_epu32 (best_128, _mm_shuffle_epi32 (best_128, _MM_SHUFFLE (2, 3, 0, 1)));

    return _mm_cvtsi128_si32 (best_128);
}

/* Find the element of an array of u32 that is closest to a wanted value,
 * treating each u32 as four unsigned bytes and using the sum of squared
 * per-byte differences as the distance. Byte order and the meaning of
 * the fourth byte left to the caller (zero it in both the array and the
 * wanted value to compare three channels only).
 *
 * Returns the index of the closest element. Ties go to the lowest index.
 * n must be in the range [1..16384]. */
gint
chafa_find_nearest_u32_avx2 (const guint32 *array, gint n, guint32 want)
{
    return find_nearest_u32_packed (array, n, want) & PACKED_INDEX_MASK;
}

/* Like chafa_find_nearest_u32_avx2(), but also returns the distance of the
 * closest element in *dist_out. */
gint
chafa_find_nearest_u32_dist_avx2 (const guint32 *array, gint n, guint32 want, gint *dist_out)
{
    guint32 packed = find_nearest_u32_packed (array, n, want);

    *dist_out = packed >> PACKED_INDEX_BITS;
    return packed & PACKED_INDEX_MASK;
}

/* Dither n pixels (a multiple of eight, aligned to eight) using one row of
 * byte modifiers. */
void
chafa_dither_pixels_avx2 (const guint32 *src, guint32 *dst,
                          const guint8 *pos_row, const guint8 *neg_row,
                          gint grain_shift, guint col_mask, guint col,
                          gint n)
{
    /* Which of the eight loaded columns each of the eight pixels uses */
    static const gint32 perm_tables [4] [8] =
    {
        { 0, 1, 2, 3, 4, 5, 6, 7 },
        { 0, 0, 1, 1, 2, 2, 3, 3 },
        { 0, 0, 0, 0, 1, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0 }
    };
    const __m256i perm = _mm256_loadu_si256 ((const __m256i *) perm_tables [grain_shift]);
    const guint cols_per_vec = 8 >> grain_shift;
    gint i;

    g_assert (grain_shift >= 0 && grain_shift <= 3);
    g_assert ((n & 7) == 0);

    for (i = 0; i < n; i += 8)
    {
        __m256i pos = _mm256_permutevar8x32_epi32 (_mm256_loadu_si256 (
            (const __m256i *) (pos_row + col * 4)), perm);
        __m256i neg = _mm256_permutevar8x32_epi32 (_mm256_loadu_si256 (
            (const __m256i *) (neg_row + col * 4)), perm);
        __m256i px = _mm256_loadu_si256 ((const __m256i *) (src + i));

        px = _mm256_subs_epu8 (_mm256_adds_epu8 (px, pos), neg);
        _mm256_storeu_si256 ((__m256i *) (dst + i), px);

        col = (col + cols_per_vec) & col_mask;
    }
}
