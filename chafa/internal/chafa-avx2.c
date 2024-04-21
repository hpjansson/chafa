/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2023 Hans Petter Jansson
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
#include "chafa.h"
#include "internal/chafa-private.h"

gint
calc_error_avx2 (const ChafaPixel *pixels, const ChafaColorPair *color_pair,
                 const guint32 *sym_mask_u32)
{
    __m256i err_8x_u32 = { 0 };
    const gint32 *e = (gint32 *) &err_8x_u32;
    __m128i fg_4x_u32, bg_4x_u32;
    __m256i fg_4x_u64, bg_4x_u64;
    const __m256i *pixels_8x_p = (const __m256i *) pixels;
    const __m256i *sym_mask_8x_p = (const __m256i *) sym_mask_u32;
    gint i;

    fg_4x_u32 = _mm_set1_epi32 (CHAFA_COLOR8_U32 (color_pair->colors [CHAFA_COLOR_PAIR_FG]));
    fg_4x_u64 = _mm256_cvtepu8_epi16 (fg_4x_u32);

    bg_4x_u32 = _mm_set1_epi32 (CHAFA_COLOR8_U32 (color_pair->colors [CHAFA_COLOR_PAIR_BG]));
    bg_4x_u64 = _mm256_cvtepu8_epi16 (bg_4x_u32);

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS / 8; i++)
    {
        __m256i pixels_8x, sym_mask_8x;
        __m256i p0, m0, fg0, bg0, d0;
        __m256i p1, m1, fg1, bg1, d1;

        pixels_8x = _mm256_loadu_si256 (pixels_8x_p);
        pixels_8x_p++;

        sym_mask_8x = _mm256_loadu_si256 (sym_mask_8x_p);
        sym_mask_8x_p++;

        p0 = _mm256_cvtepu8_epi16 (_mm256_extracti128_si256 (pixels_8x, 0));
        m0 = _mm256_cvtepi8_epi16 (_mm256_extracti128_si256 (sym_mask_8x, 0));
        fg0 = _mm256_and_si256 (m0, _mm256_sub_epi16 (fg_4x_u64, p0));
        bg0 = _mm256_andnot_si256 (m0, _mm256_sub_epi16 (bg_4x_u64, p0));
        d0 = _mm256_or_si256 (fg0, bg0);
        d0 = _mm256_mullo_epi16 (d0, d0);
        d0 = _mm256_add_epi32 (_mm256_cvtepu16_epi32 (_mm256_extracti128_si256 (d0, 0)),
                               _mm256_cvtepu16_epi32 (_mm256_extracti128_si256 (d0, 1)));

        p1 = _mm256_cvtepu8_epi16 (_mm256_extracti128_si256 (pixels_8x, 1));
        m1 = _mm256_cvtepi8_epi16 (_mm256_extracti128_si256 (sym_mask_8x, 1));
        fg1 = _mm256_and_si256 (m1, _mm256_sub_epi16 (fg_4x_u64, p1));
        bg1 = _mm256_andnot_si256 (m1, _mm256_sub_epi16 (bg_4x_u64, p1));
        d1 = _mm256_or_si256 (fg1, bg1);
        d1 = _mm256_mullo_epi16 (d1, d1);
        d1 = _mm256_add_epi32 (_mm256_cvtepu16_epi32 (_mm256_extracti128_si256 (d1, 0)),
                               _mm256_cvtepu16_epi32 (_mm256_extracti128_si256 (d1, 1)));

        err_8x_u32 = _mm256_add_epi32 (err_8x_u32, d0);
        err_8x_u32 = _mm256_add_epi32 (err_8x_u32, d1);
    }

    return e [0] + e [1] + e [2] + e [4] + e [5] + e [6];
}

void
calc_colors_avx2 (const ChafaPixel *pixels, ChafaColorAccum *accums_out,
                  const guint32 *sym_mask_u32)
{
    const __m128i *pixels_4x_p = (const __m128i *) pixels;
    const __m128i *sym_mask_4x_p = (const __m128i *) sym_mask_u32;
    __m256i accum_fg = { 0 };
    __m256i accum_bg = { 0 };
    __m128i accum_fg_128;
    __m128i accum_bg_128;
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
    ((guint64 *) accums_out) [0] =
        (guint64) _mm_extract_epi64 (accum_bg_128, 0)
        + (guint64) _mm_extract_epi64 (accum_bg_128, 1);

    accum_fg_128 = _mm_add_epi16 (_mm256_extracti128_si256 (accum_fg, 0),
                                  _mm256_extracti128_si256 (accum_fg, 1));
    ((guint64 *) accums_out) [1] =
        (guint64) _mm_extract_epi64 (accum_fg_128, 0)
        + (guint64) _mm_extract_epi64 (accum_fg_128, 1);
}

/* 32768 divided by index. Divide by zero is defined as zero. */
static const guint16 invdiv16 [257] =
{
    0, 32768, 16384, 10922, 8192, 6553, 5461, 4681, 4096, 3640, 3276,
    2978, 2730, 2520, 2340, 2184, 2048, 1927, 1820, 1724, 1638, 1560,
    1489, 1424, 1365, 1310, 1260, 1213, 1170, 1129, 1092, 1057, 1024,
    992, 963, 936, 910, 885, 862, 840, 819, 799, 780, 762, 744, 728,
    712, 697, 682, 668, 655, 642, 630, 618, 606, 595, 585, 574, 564,
    555, 546, 537, 528, 520, 512, 504, 496, 489, 481, 474, 468, 461,
    455, 448, 442, 436, 431, 425, 420, 414, 409, 404, 399, 394, 390,
    385, 381, 376, 372, 368, 364, 360, 356, 352, 348, 344, 341, 337,
    334, 330, 327, 324, 321, 318, 315, 312, 309, 306, 303, 300, 297,
    295, 292, 289, 287, 284, 282, 280, 277, 275, 273, 270, 268, 266,
    264, 262, 260, 258, 256, 254, 252, 250, 248, 246, 244, 242, 240,
    239, 237, 235, 234, 232, 230, 229, 227, 225, 224, 222, 221, 219,
    218, 217, 215, 214, 212, 211, 210, 208, 207, 206, 204, 203, 202,
    201, 199, 198, 197, 196, 195, 193, 192, 191, 190, 189, 188, 187,
    186, 185, 184, 183, 182, 181, 180, 179, 178, 177, 176, 175, 174,
    173, 172, 171, 170, 169, 168, 168, 167, 166, 165, 164, 163, 163,
    162, 161, 160, 159, 159, 158, 157, 156, 156, 155, 154, 153, 153,
    152, 151, 151, 150, 149, 148, 148, 147, 146, 146, 145, 144, 144,
    143, 143, 142, 141, 141, 140, 140, 139, 138, 138, 137, 137, 136,
    135, 135, 134, 134, 133, 133, 132, 132, 131, 131, 130, 130, 129,
    129, 128, 128
};

/* Divisor must be in the range [0..256] inclusive. */
void
chafa_color_accum_div_scalar_avx2 (ChafaColorAccum *accum, guint16 divisor)
{
    __m128i accum_128, divisor_128;

    accum_128 = _mm_loadu_si64 ((guint64 *) accum);
    divisor_128 = _mm_set1_epi16 (invdiv16 [divisor]);
    accum_128 = _mm_mulhrs_epi16 (accum_128, divisor_128);
    *((guint64 *) accum) = _mm_extract_epi64 (accum_128, 0);
}
