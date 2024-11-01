/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright © 2019-2024 Hans Petter Jansson. See COPYING for details. */

#include <assert.h>
#include <stdlib.h> /* malloc, free, alloca */
#include <string.h> /* memset */
#include <limits.h>
#include "smolscale-private.h"

/* ---------------------- *
 * Context initialization *
 * ---------------------- */

/* Linear precalc array:
 *
 * Each sample is extracted from a pair of adjacent pixels. The sample precalc
 * consists of the first pixel's index, followed by its sample fraction [0..256].
 * The second sample is implicitly taken at index+1 and weighted as 256-fraction.
 *       _   _   _
 * In   |_| |_| |_|
 *        \_/ \_/   <- two samples per output pixel
 * Out    |_| |_|
 *
 * When halving,
 *       _   _   _
 * In   |_| |_| |_|
 *        \_/ \_/   <- four samples per output pixel
 *        |_| |_|
 *          \_/     <- halving
 * Out      |_|
 */

static void
precalc_linear_range (uint16_t *array_out,
                      int first_index, int last_index,
                      uint64_t first_sample_ofs, uint64_t sample_step,
                      int sample_ofs_px_max,
                      int32_t dest_clip_before_px,
                      int *array_i_inout)
{
    uint64_t sample_ofs;
    int i;

    sample_ofs = first_sample_ofs;

    for (i = first_index; i < last_index; i++)
    {
        uint16_t sample_ofs_px = sample_ofs / SMOL_BILIN_MULTIPLIER;

        if (sample_ofs_px >= sample_ofs_px_max - 1)
        {
            if (i >= dest_clip_before_px)
            {
                array_out [(*array_i_inout) * 2] = sample_ofs_px_max - 2;
                array_out [(*array_i_inout) * 2 + 1] = 0;
                (*array_i_inout)++;
            }
            continue;
        }

        if (i >= dest_clip_before_px)
        {
            array_out [(*array_i_inout) * 2] = sample_ofs_px;
            array_out [(*array_i_inout) * 2 + 1] = SMOL_SMALL_MUL
                - ((sample_ofs / (SMOL_BILIN_MULTIPLIER / SMOL_SMALL_MUL)) % SMOL_SMALL_MUL);
            (*array_i_inout)++;
        }

        sample_ofs += sample_step;
    }
}

static void
precalc_bilinear_array (uint16_t *array,
                        uint64_t src_dim_spx,
                        uint64_t dest_ofs_spx,
                        uint64_t dest_dim_spx,
                        uint32_t dest_dim_prehalving_px,
                        unsigned int n_halvings,
                        int32_t dest_clip_before_px)
{
    uint32_t src_dim_px = SMOL_SPX_TO_PX (src_dim_spx);
    uint64_t first_sample_ofs [3];
    uint64_t sample_step;
    int i = 0;

    assert (src_dim_px > 1);

    dest_ofs_spx %= SMOL_SUBPIXEL_MUL;

    if (src_dim_spx > dest_dim_spx)
    {
        /* Minification */
        sample_step = ((uint64_t) src_dim_spx * SMOL_BILIN_MULTIPLIER) / dest_dim_spx;
        first_sample_ofs [0] = (sample_step - SMOL_BILIN_MULTIPLIER) / 2;
        first_sample_ofs [1] = ((sample_step - SMOL_BILIN_MULTIPLIER) / 2)
            + ((sample_step * (SMOL_SUBPIXEL_MUL - dest_ofs_spx) * (1 << n_halvings)) / SMOL_SUBPIXEL_MUL);
    }
    else
    {
        /* Magnification */
        sample_step = ((src_dim_spx - SMOL_SUBPIXEL_MUL) * SMOL_BILIN_MULTIPLIER)
            / (dest_dim_spx > SMOL_SUBPIXEL_MUL ? (dest_dim_spx - SMOL_SUBPIXEL_MUL) : 1);
        first_sample_ofs [0] = 0;
        first_sample_ofs [1] = (sample_step * (SMOL_SUBPIXEL_MUL - dest_ofs_spx)) / SMOL_SUBPIXEL_MUL;
    }

    first_sample_ofs [2] = (((uint64_t) src_dim_spx * SMOL_BILIN_MULTIPLIER) / SMOL_SUBPIXEL_MUL)
        + ((sample_step - SMOL_BILIN_MULTIPLIER) / 2)
        - sample_step * (1U << n_halvings);

    /* Left fringe */
    precalc_linear_range (array,
                          0,
                          1 << n_halvings,
                          first_sample_ofs [0],
                          sample_step,
                          src_dim_px,
                          dest_clip_before_px,
                          &i);

    /* Main range */
    precalc_linear_range (array,
                          1 << n_halvings,
                          dest_dim_prehalving_px - (1 << n_halvings),
                          first_sample_ofs [1],
                          sample_step,
                          src_dim_px,
                          dest_clip_before_px,
                          &i);

    /* Right fringe */
    precalc_linear_range (array,
                          dest_dim_prehalving_px - (1 << n_halvings),
                          dest_dim_prehalving_px,
                          first_sample_ofs [2],
                          sample_step,
                          src_dim_px,
                          dest_clip_before_px,
                          &i);
}

static void
precalc_boxes_array (uint32_t *array,
                     uint32_t *span_step,
                     uint32_t *span_mul,
                     uint32_t src_dim_spx,
                     int32_t dest_dim,
                     uint32_t dest_ofs_spx,
                     uint32_t dest_dim_spx,
                     int32_t dest_clip_before_px)
{
    uint64_t fracF, frac_stepF;
    uint64_t f;
    uint64_t stride;
    uint64_t a, b;
    int i, dest_i;

    dest_ofs_spx %= SMOL_SUBPIXEL_MUL;

    /* Output sample can't be less than a pixel. Fringe opacity is applied in
     * a separate step. FIXME: May cause wrong subpixel distribution -- revisit. */
    if (dest_dim_spx < 256)
        dest_dim_spx = 256;

    frac_stepF = ((uint64_t) src_dim_spx * SMOL_BIG_MUL) / (uint64_t) dest_dim_spx;
    fracF = 0;

    stride = frac_stepF / (uint64_t) SMOL_BIG_MUL;
    f = (frac_stepF / SMOL_SMALL_MUL) % SMOL_SMALL_MUL;

    /* We divide by (b + 1) instead of just (b) to avoid overflows in
     * scale_128bpp_half(), which would affect horizontal box scaling. The
     * fudge factor counters limited precision in the inverted division
     * operation. It causes 16-bit values to undershoot by less than 127/65535
     * (<.2%). Since the final output is 8-bit, and rounding neutralizes the
     * error, this doesn't matter. */

    a = (SMOL_BOXES_MULTIPLIER * 255);
    b = ((stride * 255) + ((f * 255) / 256));
    *span_step = frac_stepF / SMOL_SMALL_MUL;
    *span_mul = (a + (b / 2)) / (b + 1);

    /* Left fringe */
    i = 0;
    dest_i = 0;

    if (dest_i >= dest_clip_before_px)
        array [i++] = 0;

    /* Main range */
    fracF = ((frac_stepF * (SMOL_SUBPIXEL_MUL - dest_ofs_spx)) / SMOL_SUBPIXEL_MUL);
    for (dest_i = 1; dest_i < dest_dim - 1; dest_i++)
    {
        if (dest_i >= dest_clip_before_px)
            array [i++] = fracF / SMOL_SMALL_MUL;
        fracF += frac_stepF;
    }

    /* Right fringe */
    if (dest_dim > 1 && dest_i >= dest_clip_before_px)
        array [i++] = (((uint64_t) src_dim_spx * SMOL_SMALL_MUL - frac_stepF) / SMOL_SMALL_MUL);
}

static void
init_dim (SmolDim *dim)
{
    if (dim->filter_type == SMOL_FILTER_ONE || dim->filter_type == SMOL_FILTER_COPY)
    {
    }
    else if (dim->filter_type == SMOL_FILTER_BOX)
    {
        precalc_boxes_array (dim->precalc,
                             &dim->span_step,
                             &dim->span_mul,
                             dim->src_size_spx,
                             dim->placement_size_px,
                             dim->placement_ofs_spx,
                             dim->placement_size_spx,
                             dim->clip_before_px);
    }
    else /* SMOL_FILTER_BILINEAR_?H */
    {
        precalc_bilinear_array (dim->precalc,
                                dim->src_size_spx,
                                dim->placement_ofs_spx,
                                dim->placement_size_prehalving_spx,
                                dim->placement_size_prehalving_px,
                                dim->n_halvings,
                                dim->clip_before_px);
    }
}

static void
init_horizontal (SmolScaleCtx *scale_ctx)
{
    init_dim (&scale_ctx->hdim);
}

static void
init_vertical (SmolScaleCtx *scale_ctx)
{
    init_dim (&scale_ctx->vdim);
}

/* ---------------------- *
 * sRGB/linear conversion *
 * ---------------------- */

static void
from_srgb_pixel_xxxa_128bpp (uint64_t * SMOL_RESTRICT pixel_inout)
{
    uint64_t part;

    part = pixel_inout [0];
    pixel_inout [0] =
        ((uint64_t) _smol_from_srgb_lut [part >> 32] << 32)
        | _smol_from_srgb_lut [part & 0xff];

    part = pixel_inout [1];
    pixel_inout [1] =
        ((uint64_t) _smol_from_srgb_lut [part >> 32] << 32)
        | ((part & 0xffffffff) << 3) | 7;
}

static void
to_srgb_pixel_xxxa_128bpp (const uint64_t *pixel_in, uint64_t *pixel_out)
{
    pixel_out [0] =
        (((uint64_t) _smol_to_srgb_lut [pixel_in [0] >> 32]) << 32)
        | _smol_to_srgb_lut [pixel_in [0] & 0xffff];

    pixel_out [1] =
        (((uint64_t) _smol_to_srgb_lut [pixel_in [1] >> 32]) << 32)
        | (pixel_in [1] & 0xffffffff);  /* FIXME: No need to preserve alpha? */
}

/* Fetches alpha from linear pixel. Input alpha is in the range [0x000..0x7ff].
 * Returned alpha is in the range [0x00..0xff], rounded towards 0xff. */
static SMOL_INLINE uint8_t
get_alpha_from_linear_xxxa_128bpp (const uint64_t * SMOL_RESTRICT pixel_in)
{
    uint16_t alpha = (pixel_in [1] + 7) >> 3;
    return (uint8_t) (alpha - (alpha >> 8)); /* Turn 0x100 into 0xff */
}

/* ----------------- *
 * Premultiplication *
 * ----------------- */

static SMOL_INLINE void
premul_u_to_p8_128bpp (uint64_t * SMOL_RESTRICT inout,
                       uint16_t alpha)
{
    inout [0] = ((inout [0] * (alpha + 1)) >> 8) & 0x000000ff000000ff;
    inout [1] = ((inout [1] * (alpha + 1)) >> 8) & 0x000000ff000000ff;
}

static SMOL_INLINE void
unpremul_p8_to_u_128bpp (const uint64_t *in,
                         uint64_t *out,
                         uint8_t alpha)
{
    out [0] = ((in [0] * _smol_inv_div_p8_lut [alpha])
               >> INVERTED_DIV_SHIFT_P8) & 0x000000ff000000ff;
    out [1] = ((in [1] * _smol_inv_div_p8_lut [alpha])
               >> INVERTED_DIV_SHIFT_P8) & 0x000000ff000000ff;
}

static SMOL_INLINE uint64_t
premul_u_to_p8_64bpp (const uint64_t in,
                      uint16_t alpha)
{
    return ((in  * (alpha + 1)) >> 8) & 0x00ff00ff00ff00ff;
}

static SMOL_INLINE uint64_t
unpremul_p8_to_u_64bpp (const uint64_t in,
                        uint8_t alpha)
{
    uint64_t in_128bpp [2];
    uint64_t dest_128bpp [2];

    in_128bpp [0] = (in & 0x000000ff000000ff);
    in_128bpp [1] = (in & 0x00ff000000ff0000) >> 16;

    unpremul_p8_to_u_128bpp (in_128bpp, dest_128bpp, alpha);

    return dest_128bpp [0] | (dest_128bpp [1] << 16);
}

static SMOL_INLINE void
premul_ul_to_p8l_128bpp (uint64_t * SMOL_RESTRICT inout,
                         uint16_t alpha)
{
    inout [0] = ((inout [0] * (alpha + 1)) >> 8) & 0x000007ff000007ff;
    inout [1] = (((inout [1] * (alpha + 1)) >> 8) & 0x000007ff00000000)
        | (inout [1] & 0x000007ff);
}

static SMOL_INLINE void
unpremul_p8l_to_ul_128bpp (const uint64_t *in,
                           uint64_t *out,
                           uint8_t alpha)
{
    out [0] = ((in [0] * _smol_inv_div_p8l_lut [alpha])
               >> INVERTED_DIV_SHIFT_P8L) & 0x000007ff000007ff;
    out [1] = ((in [1] * _smol_inv_div_p8l_lut [alpha])
               >> INVERTED_DIV_SHIFT_P8L) & 0x000007ff000007ff;
}

static SMOL_INLINE void
premul_u_to_p16_128bpp (uint64_t *inout,
                        uint8_t alpha)
{
    inout [0] = inout [0] * ((uint16_t) alpha + 2);
    inout [1] = inout [1] * ((uint16_t) alpha + 2);
}

static SMOL_INLINE void
unpremul_p16_to_u_128bpp (const uint64_t * SMOL_RESTRICT in,
                          uint64_t * SMOL_RESTRICT out,
                          uint8_t alpha)
{
    out [0] = ((in [0] * _smol_inv_div_p16_lut [alpha])
               >> INVERTED_DIV_SHIFT_P16) & 0x000000ff000000ffULL;
    out [1] = ((in [1] * _smol_inv_div_p16_lut [alpha])
               >> INVERTED_DIV_SHIFT_P16) & 0x000000ff000000ffULL;
}

static SMOL_INLINE void
premul_ul_to_p16l_128bpp (uint64_t *inout,
                          uint8_t alpha)
{
    inout [0] = inout [0] * ((uint16_t) alpha + 2);
    inout [1] = inout [1] * ((uint16_t) alpha + 2);
}

static SMOL_INLINE void
unpremul_p16l_to_ul_128bpp (const uint64_t * SMOL_RESTRICT in,
                            uint64_t * SMOL_RESTRICT out,
                            uint8_t alpha)
{
    out [0] = ((in [0] * _smol_inv_div_p16l_lut [alpha])
               >> INVERTED_DIV_SHIFT_P16L) & 0x000007ff000007ffULL;
    out [1] = ((in [1] * _smol_inv_div_p16l_lut [alpha])
               >> INVERTED_DIV_SHIFT_P16L) & 0x000007ff000007ffULL;
}

/* --------- *
 * Repacking *
 * --------- */

/* It's nice to be able to shift by a negative amount */
#define SHIFT_S(in, s) ((s >= 0) ? (in) << (s) : (in) >> -(s))

/* This is kind of bulky (~13 x86 insns), but it's about the same as using
 * unions, and we don't have to worry about endianness. */
#define PACK_FROM_1234_64BPP(in, a, b, c, d) \
    ((SHIFT_S ((in), ((a) - 1) * 16 + 8 - 32) & 0xff000000) \
     | (SHIFT_S ((in), ((b) - 1) * 16 + 8 - 40) & 0x00ff0000) \
     | (SHIFT_S ((in), ((c) - 1) * 16 + 8 - 48) & 0x0000ff00) \
     | (SHIFT_S ((in), ((d) - 1) * 16 + 8 - 56) & 0x000000ff))

#define PACK_FROM_1234_128BPP(in, a, b, c, d) \
    ((SHIFT_S ((in [((a) - 1) >> 1]), (((a) - 1) & 1) * 32 + 24 - 32) & 0xff000000) \
     | (SHIFT_S ((in [((b) - 1) >> 1]), (((b) - 1) & 1) * 32 + 24 - 40) & 0x00ff0000) \
     | (SHIFT_S ((in [((c) - 1) >> 1]), (((c) - 1) & 1) * 32 + 24 - 48) & 0x0000ff00) \
     | (SHIFT_S ((in [((d) - 1) >> 1]), (((d) - 1) & 1) * 32 + 24 - 56) & 0x000000ff))

#define SWAP_2_AND_3(n) ((n) == 2 ? 3 : (n) == 3 ? 2 : n)

#define PACK_FROM_1324_64BPP(in, a, b, c, d) \
    ((SHIFT_S ((in), (SWAP_2_AND_3 (a) - 1) * 16 + 8 - 32) & 0xff000000) \
     | (SHIFT_S ((in), (SWAP_2_AND_3 (b) - 1) * 16 + 8 - 40) & 0x00ff0000) \
     | (SHIFT_S ((in), (SWAP_2_AND_3 (c) - 1) * 16 + 8 - 48) & 0x0000ff00) \
     | (SHIFT_S ((in), (SWAP_2_AND_3 (d) - 1) * 16 + 8 - 56) & 0x000000ff))

/* ---------------------- *
 * Repacking: 24/32 -> 64 *
 * ---------------------- */

static SMOL_INLINE uint64_t
unpack_pixel_123_p8_to_132a_p8_64bpp (const uint8_t *p)
{
    return ((uint64_t) p [0] << 48) | ((uint32_t) p [1] << 16)
        | ((uint64_t) p [2] << 32) | 0xff;
}

SMOL_REPACK_ROW_DEF (123,  24,  8, PREMUL8, COMPRESSED,
                     1324, 64, 64, PREMUL8, COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = unpack_pixel_123_p8_to_132a_p8_64bpp (src_row);
        src_row += 3;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE uint64_t
unpack_pixel_1234_p8_to_1324_p8_64bpp (uint32_t p)
{
    return (((uint64_t) p & 0xff00ff00) << 24) | (p & 0x00ff00ff);
}

SMOL_REPACK_ROW_DEF (1234, 32, 32, PREMUL8, COMPRESSED,
                     1324, 64, 64, PREMUL8, COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = unpack_pixel_1234_p8_to_1324_p8_64bpp (*(src_row++));
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE uint64_t
unpack_pixel_1234_p8_to_3241_p8_64bpp (uint32_t p)
{
    return (((uint64_t) p & 0x0000ff00) << 40)
        | (((uint64_t) p & 0x00ff00ff) << 16) | (p >> 24);
}

SMOL_REPACK_ROW_DEF (1234, 32, 32, PREMUL8, COMPRESSED,
                     3241, 64, 64, PREMUL8, COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = unpack_pixel_1234_p8_to_3241_p8_64bpp (*(src_row++));
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE uint64_t
unpack_pixel_1234_p8_to_2431_p8_64bpp (uint32_t p)
{
    uint64_t p64 = p;

    return ((p64 & 0x00ff00ff) << 32) | ((p64 & 0x0000ff00) << 8)
        | ((p64 & 0xff000000) >> 24);
}

SMOL_REPACK_ROW_DEF (1234, 32, 32, PREMUL8, COMPRESSED,
                     2431, 64, 64, PREMUL8, COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = unpack_pixel_1234_p8_to_2431_p8_64bpp (*(src_row++));
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE uint64_t
unpack_pixel_a234_u_to_324a_p8_64bpp (uint32_t p)
{
    uint64_t p64 = (((uint64_t) p & 0x0000ff00) << 40) | (((uint64_t) p & 0x00ff00ff) << 16);
    uint8_t alpha = p >> 24;

    return (premul_u_to_p8_64bpp (p64, alpha) & 0xffffffffffffff00ULL) | alpha;
}

SMOL_REPACK_ROW_DEF (1234, 32, 32, UNASSOCIATED, COMPRESSED,
                     3241, 64, 64, PREMUL8,      COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = unpack_pixel_a234_u_to_324a_p8_64bpp (*(src_row++));
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE uint64_t
unpack_pixel_1234_u_to_2431_p8_64bpp (uint32_t p)
{
    uint64_t p64 = (((uint64_t) p & 0x00ff00ff) << 32) | (((uint64_t) p & 0x0000ff00) << 8);
    uint8_t alpha = p >> 24;

    return (premul_u_to_p8_64bpp (p64, alpha) & 0xffffffffffffff00ULL) | alpha;
}

SMOL_REPACK_ROW_DEF (1234, 32, 32, UNASSOCIATED, COMPRESSED,
                     2431, 64, 64, PREMUL8,      COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = unpack_pixel_1234_u_to_2431_p8_64bpp (*(src_row++));
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE uint64_t
unpack_pixel_123a_u_to_132a_p8_64bpp (uint32_t p)
{
    uint64_t p64 = (((uint64_t) p & 0xff00ff00) << 24) | (p & 0x00ff0000);
    uint8_t alpha = p & 0xff;

    return (premul_u_to_p8_64bpp (p64, alpha) & 0xffffffffffffff00ULL) | alpha;
}

SMOL_REPACK_ROW_DEF (1234, 32, 32, UNASSOCIATED, COMPRESSED,
                     1324, 64, 64, PREMUL8,      COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = unpack_pixel_123a_u_to_132a_p8_64bpp (*(src_row++));
    }
} SMOL_REPACK_ROW_DEF_END

/* ----------------------- *
 * Repacking: 24/32 -> 128 *
 * ----------------------- */

static SMOL_INLINE void
unpack_pixel_123_p8_to_123a_p8_128bpp (const uint8_t *in,
                                       uint64_t *out)
{
    out [0] = ((uint64_t) in [0] << 32) | in [1];
    out [1] = ((uint64_t) in [2] << 32) | 0xff;
}

SMOL_REPACK_ROW_DEF (123,   24,  8, PREMUL8, COMPRESSED,
                     1234, 128, 64, PREMUL8, COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        unpack_pixel_123_p8_to_123a_p8_128bpp (src_row, dest_row);
        src_row += 3;
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (123,   24,  8, PREMUL8, COMPRESSED,
                     1234, 128, 64, PREMUL8, LINEAR) {
    while (dest_row != dest_row_max)
    {
        uint8_t alpha;
        unpack_pixel_123_p8_to_123a_p8_128bpp (src_row, dest_row);
        alpha = dest_row [1];
        from_srgb_pixel_xxxa_128bpp (dest_row);
        dest_row [1] = (dest_row [1] & 0xffffffff00000000) | (alpha << 3) | 7;
        src_row += 3;
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_123a_p8_to_123a_p8_128bpp (uint32_t p,
                                        uint64_t *out)
{
    uint64_t p64 = p;
    out [0] = ((p64 & 0xff000000) << 8) | ((p64 & 0x00ff0000) >> 16);
    out [1] = ((p64 & 0x0000ff00) << 24) | (p64 & 0x000000ff);
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, PREMUL8, COMPRESSED,
                     1234, 128, 64, PREMUL8, COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        unpack_pixel_123a_p8_to_123a_p8_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234,  32, 32, PREMUL8, COMPRESSED,
                     1234, 128, 64, PREMUL8, LINEAR) {
    while (dest_row != dest_row_max)
    {
        uint8_t alpha;
        unpack_pixel_123a_p8_to_123a_p8_128bpp (*(src_row++), dest_row);
        alpha = dest_row [1];
        from_srgb_pixel_xxxa_128bpp (dest_row);
        dest_row [1] = (dest_row [1] & 0xffffffff00000000) | (alpha << 3) | 7;
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_a234_p8_to_234a_p8_128bpp (uint32_t p,
                                        uint64_t *out)
{
    uint64_t p64 = p;
    out [0] = ((p64 & 0x00ff0000) << 16) | ((p64 & 0x0000ff00) >> 8);
    out [1] = ((p64 & 0x000000ff) << 32) | ((p64 & 0xff000000) >> 24);
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, PREMUL8, COMPRESSED,
                     2341, 128, 64, PREMUL8, COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        unpack_pixel_a234_p8_to_234a_p8_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234,  32, 32, PREMUL8, COMPRESSED,
                     2341, 128, 64, PREMUL8, LINEAR) {
    while (dest_row != dest_row_max)
    {
        uint8_t alpha;
        unpack_pixel_a234_p8_to_234a_p8_128bpp (*(src_row++), dest_row);
        alpha = dest_row [1];
        from_srgb_pixel_xxxa_128bpp (dest_row);
        dest_row [1] = (dest_row [1] & 0xffffffff00000000) | (alpha << 3) | 7;
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_a234_u_to_234a_p8_128bpp (uint32_t p,
                                       uint64_t *out)
{
    uint64_t p64 = (((uint64_t) p & 0x00ff00ff) << 32) | (((uint64_t) p & 0x0000ff00) << 8);
    uint8_t alpha = p >> 24;

    p64 = (premul_u_to_p8_64bpp (p64, alpha) & 0xffffffffffffff00) | alpha;
    out [0] = (p64 >> 16) & 0x000000ff000000ff;
    out [1] = p64 & 0x000000ff000000ff;
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     2341, 128, 64, PREMUL8,      COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        unpack_pixel_a234_u_to_234a_p8_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_a234_u_to_234a_pl_128bpp (uint32_t p,
                                       uint64_t *out)
{
    uint64_t p64 = p;
    uint8_t alpha = p >> 24;

    out [0] = ((p64 & 0x00ff0000) << 16) | ((p64 & 0x0000ff00) >> 8);
    out [1] = ((p64 & 0x000000ff) << 32) | alpha;

    from_srgb_pixel_xxxa_128bpp (out);
    premul_ul_to_p8l_128bpp (out, alpha);
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     2341, 128, 64, PREMUL8,      LINEAR) {
    while (dest_row != dest_row_max)
    {
        unpack_pixel_a234_u_to_234a_pl_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_a234_u_to_234a_p16_128bpp (uint32_t p,
                                        uint64_t *out)
{
    uint64_t p64 = p;
    uint8_t alpha = p >> 24;

    out [0] = ((p64 & 0x00ff0000) << 16) | ((p64 & 0x0000ff00) >> 8);
    out [1] = ((p64 & 0x000000ff) << 32);

    premul_u_to_p16_128bpp (out, alpha);
    out [1] |= (((uint16_t) alpha) << 8) | alpha;
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     2341, 128, 64, PREMUL16,     COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        unpack_pixel_a234_u_to_234a_p16_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_a234_u_to_234a_p16l_128bpp (uint32_t p,
                                         uint64_t *out)
{
    uint64_t p64 = p;
    uint8_t alpha = p >> 24;

    out [0] = ((p64 & 0x00ff0000) << 16) | ((p64 & 0x0000ff00) >> 8);
    out [1] = ((p64 & 0x000000ff) << 32);

    from_srgb_pixel_xxxa_128bpp (out);
    out [0] *= alpha;
    out [1] *= alpha;

    out [1] = (out [1] & 0xffffffff00000000ULL) | (alpha << 8) | alpha;
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     2341, 128, 64, PREMUL16,     LINEAR) {
    while (dest_row != dest_row_max)
    {
        unpack_pixel_a234_u_to_234a_p16l_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_123a_u_to_123a_p8_128bpp (uint32_t p,
                                       uint64_t *out)
{
    uint64_t p64 = (((uint64_t) p & 0xff00ff00) << 24) | (p & 0x00ff0000);
    uint8_t alpha = p;

    p64 = (premul_u_to_p8_64bpp (p64, alpha) & 0xffffffffffffff00ULL) | alpha;
    out [0] = (p64 >> 16) & 0x000000ff000000ff;
    out [1] = p64 & 0x000000ff000000ff;
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     1234, 128, 64, PREMUL8,      COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        unpack_pixel_123a_u_to_123a_p8_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_123a_u_to_123a_pl_128bpp (uint32_t p,
                                       uint64_t *out)
{
    uint64_t p64 = p;
    uint8_t alpha = p;

    out [0] = ((p64 & 0xff000000) << 8) | ((p64 & 0x00ff0000) >> 16);
    out [1] = ((p64 & 0x0000ff00) << 24) | alpha;

    from_srgb_pixel_xxxa_128bpp (out);
    premul_ul_to_p8l_128bpp (out, alpha);
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     1234, 128, 64, PREMUL8,      LINEAR) {
    while (dest_row != dest_row_max)
    {
        unpack_pixel_123a_u_to_123a_pl_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_123a_u_to_123a_p16_128bpp (uint32_t p,
                                        uint64_t *out)
{
    uint64_t p64 = p;
    uint8_t alpha = p;

    out [0] = ((p64 & 0xff000000) << 8) | ((p64 & 0x00ff0000) >> 16);
    out [1] = ((p64 & 0x0000ff00) << 24);

    premul_u_to_p16_128bpp (out, alpha);
    out [1] |= (((uint16_t) alpha) << 8) | alpha;
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     1234, 128, 64, PREMUL16,     COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        unpack_pixel_123a_u_to_123a_p16_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_123a_u_to_123a_p16l_128bpp (uint32_t p,
                                         uint64_t *out)
{
    uint64_t p64 = p;
    uint8_t alpha = p;

    out [0] = ((p64 & 0xff000000) << 8) | ((p64 & 0x00ff0000) >> 16);
    out [1] = ((p64 & 0x0000ff00) << 24);

    from_srgb_pixel_xxxa_128bpp (out);
    premul_ul_to_p16l_128bpp (out, alpha);

    out [1] = (out [1] & 0xffffffff00000000ULL) | ((uint16_t) alpha << 8) | alpha;
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     1234, 128, 64, PREMUL16,     LINEAR) {
    while (dest_row != dest_row_max)
    {
        unpack_pixel_123a_u_to_123a_p16l_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

/* ---------------------- *
 * Repacking: 64 -> 24/32 *
 * ---------------------- */

static SMOL_INLINE uint32_t
pack_pixel_1234_p8_to_1324_p8_64bpp (uint64_t in)
{
    return in | (in >> 24);
}

SMOL_REPACK_ROW_DEF (1234, 64, 64, PREMUL8,       COMPRESSED,
                     132,  24,  8, PREMUL8,       COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint32_t p = pack_pixel_1234_p8_to_1324_p8_64bpp (*(src_row++));
        *(dest_row++) = p >> 24;
        *(dest_row++) = p >> 16;
        *(dest_row++) = p >> 8;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 64, 64, PREMUL8,       COMPRESSED,
                     132,  24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint8_t alpha = *src_row;
        uint64_t t = (unpremul_p8_to_u_64bpp (*src_row, alpha) & 0xffffffffffffff00ULL) | alpha;
        uint32_t p = pack_pixel_1234_p8_to_1324_p8_64bpp (t);
        *(dest_row++) = p >> 24;
        *(dest_row++) = p >> 16;
        *(dest_row++) = p >> 8;
        src_row++;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 64, 64, PREMUL8,       COMPRESSED,
                     231,  24,  8, PREMUL8,       COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint32_t p = pack_pixel_1234_p8_to_1324_p8_64bpp (*(src_row++));
        *(dest_row++) = p >> 8;
        *(dest_row++) = p >> 16;
        *(dest_row++) = p >> 24;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 64, 64, PREMUL8,       COMPRESSED,
                     231,  24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint8_t alpha = *src_row;
        uint64_t t = (unpremul_p8_to_u_64bpp (*src_row, alpha) & 0xffffffffffffff00ULL) | alpha;
        uint32_t p = pack_pixel_1234_p8_to_1324_p8_64bpp (t);
        *(dest_row++) = p >> 8;
        *(dest_row++) = p >> 16;
        *(dest_row++) = p >> 24;
        src_row++;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 64, 64, PREMUL8,       COMPRESSED,
                     324,  24,  8, PREMUL8,       COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint32_t p = pack_pixel_1234_p8_to_1324_p8_64bpp (*(src_row++));
        *(dest_row++) = p >> 16;
        *(dest_row++) = p >> 8;
        *(dest_row++) = p;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 64, 64, PREMUL8,       COMPRESSED,
                     324,  24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint8_t alpha = *src_row >> 24;
        uint64_t t = (unpremul_p8_to_u_64bpp (*src_row, alpha) & 0xffffffffffffff00ULL) | alpha;
        uint32_t p = pack_pixel_1234_p8_to_1324_p8_64bpp (t);
        *(dest_row++) = p >> 16;
        *(dest_row++) = p >> 8;
        *(dest_row++) = p;
        src_row++;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 64, 64, PREMUL8,       COMPRESSED,
                     423,  24,  8, PREMUL8,       COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint32_t p = pack_pixel_1234_p8_to_1324_p8_64bpp (*(src_row++));
        *(dest_row++) = p;
        *(dest_row++) = p >> 8;
        *(dest_row++) = p >> 16;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 64, 64, PREMUL8,       COMPRESSED,
                     423,  24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint8_t alpha = *src_row >> 24;
        uint64_t t = (unpremul_p8_to_u_64bpp (*src_row, alpha) & 0xffffffffffffff00ULL) | alpha;
        uint32_t p = pack_pixel_1234_p8_to_1324_p8_64bpp (t);
        *(dest_row++) = p;
        *(dest_row++) = p >> 8;
        *(dest_row++) = p >> 16;
        src_row++;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 64, 64, PREMUL8,       COMPRESSED,
                     1324, 32, 32, PREMUL8,       COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = pack_pixel_1234_p8_to_1324_p8_64bpp (*(src_row++));
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 64, 64, PREMUL8,       COMPRESSED,
                     1324, 32, 32, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint8_t alpha = *src_row;
        uint64_t t = (unpremul_p8_to_u_64bpp (*src_row, alpha) & 0xffffffffffffff00ULL) | alpha;
        *(dest_row++) = pack_pixel_1234_p8_to_1324_p8_64bpp (t);
        src_row++;
    }
} SMOL_REPACK_ROW_DEF_END

#define DEF_REPACK_FROM_1234_64BPP_TO_32BPP(a, b, c, d) \
    SMOL_REPACK_ROW_DEF (1234,       64, 64, PREMUL8,       COMPRESSED, \
                         a##b##c##d, 32, 32, PREMUL8,       COMPRESSED) { \
        while (dest_row != dest_row_max) \
        { \
            *(dest_row++) = PACK_FROM_1234_64BPP (*src_row, a, b, c, d); \
            src_row++; \
        } \
    } SMOL_REPACK_ROW_DEF_END \
    SMOL_REPACK_ROW_DEF (1234,       64, 64, PREMUL8,       COMPRESSED, \
                         a##b##c##d, 32, 32, UNASSOCIATED,  COMPRESSED) { \
        while (dest_row != dest_row_max) \
        { \
            uint8_t alpha = *src_row; \
            uint64_t t = (unpremul_p8_to_u_64bpp (*src_row, alpha) & 0xffffffffffffff00ULL) | alpha; \
            *(dest_row++) = PACK_FROM_1234_64BPP (t, a, b, c, d); \
            src_row++; \
        } \
    } SMOL_REPACK_ROW_DEF_END

DEF_REPACK_FROM_1234_64BPP_TO_32BPP (1, 4, 2, 3)
DEF_REPACK_FROM_1234_64BPP_TO_32BPP (2, 3, 1, 4)
DEF_REPACK_FROM_1234_64BPP_TO_32BPP (4, 1, 3, 2)
DEF_REPACK_FROM_1234_64BPP_TO_32BPP (4, 2, 3, 1)

/* ----------------------- *
 * Repacking: 128 -> 24/32 *
 * ----------------------- */

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL8,       COMPRESSED,
                     123,   24,  8, PREMUL8,       COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = *src_row >> 32;
        *(dest_row++) = *(src_row++);
        *(dest_row++) = *(src_row++) >> 32;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL8,       LINEAR,
                     123,   24,  8, PREMUL8,       COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint64_t t [2];
        uint8_t alpha = get_alpha_from_linear_xxxa_128bpp (src_row);
        unpremul_p8l_to_ul_128bpp (src_row, t, alpha);
        to_srgb_pixel_xxxa_128bpp (src_row, t);
        *(dest_row++) = t [0] >> 32;
        *(dest_row++) = t [0];
        *(dest_row++) = t [1] >> 32;
        src_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL8,       COMPRESSED,
                     123,   24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint64_t t [2];
        uint8_t alpha = src_row [1];
        unpremul_p8_to_u_128bpp (src_row, t, alpha);
        t [1] = (t [1] & 0xffffffff00000000ULL) | alpha;
        *(dest_row++) = t [0] >> 32;
        *(dest_row++) = t [0];
        *(dest_row++) = t [1] >> 32;
        src_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL8,       LINEAR,
                     123,   24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint64_t t [2];
        uint8_t alpha = get_alpha_from_linear_xxxa_128bpp (src_row);
        unpremul_p8l_to_ul_128bpp (src_row, t, alpha);
        to_srgb_pixel_xxxa_128bpp (t, t);
        t [1] = (t [1] & 0xffffffff00000000ULL) | alpha;
        *(dest_row++) = t [0] >> 32;
        *(dest_row++) = t [0];
        *(dest_row++) = t [1] >> 32;
        src_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL16,      COMPRESSED,
                     123,   24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint64_t t [2];
        uint8_t alpha = src_row [1] >> 8;
        unpremul_p16_to_u_128bpp (src_row, t, alpha);
        t [1] = (t [1] & 0xffffffff00000000ULL) | alpha;
        *(dest_row++) = t [0] >> 32;
        *(dest_row++) = t [0];
        *(dest_row++) = t [1] >> 32;
        src_row += 2;
    } \
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL16,      LINEAR,
                     123,   24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint64_t t [2];
        uint8_t alpha = src_row [1] >> 8;
        unpremul_p16_to_u_128bpp (src_row, t, alpha);
        to_srgb_pixel_xxxa_128bpp (t, t);
        t [1] = (t [1] & 0xffffffff00000000ULL) | alpha;
        *(dest_row++) = t [0] >> 32;
        *(dest_row++) = t [0];
        *(dest_row++) = t [1] >> 32;
        src_row += 2;
    } \
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL8,       COMPRESSED,
                     321,   24,  8, PREMUL8,       COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = src_row [1] >> 32;
        *(dest_row++) = src_row [0];
        *(dest_row++) = src_row [0] >> 32;
        src_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL8,       LINEAR,
                     321,   24,  8, PREMUL8,       COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint64_t t [2];
        uint8_t alpha = get_alpha_from_linear_xxxa_128bpp (src_row);
        unpremul_p8l_to_ul_128bpp (src_row, t, alpha);
        to_srgb_pixel_xxxa_128bpp (t, t);
        *(dest_row++) = t [1] >> 32;
        *(dest_row++) = t [0];
        *(dest_row++) = t [0] >> 32;
        src_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL8,       COMPRESSED,
                     321,   24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint64_t t [2];
        uint8_t alpha = src_row [1];
        unpremul_p8_to_u_128bpp (src_row, t, alpha);
        t [1] = (t [1] & 0xffffffff00000000ULL) | alpha;
        *(dest_row++) = t [1] >> 32;
        *(dest_row++) = t [0];
        *(dest_row++) = t [0] >> 32;
        src_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL8,       LINEAR,
                     321,   24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint64_t t [2];
        uint8_t alpha = get_alpha_from_linear_xxxa_128bpp (src_row);
        unpremul_p8l_to_ul_128bpp (src_row, t, alpha);
        to_srgb_pixel_xxxa_128bpp (t, t);
        t [1] = (t [1] & 0xffffffff00000000ULL) | alpha;
        *(dest_row++) = t [1] >> 32;
        *(dest_row++) = t [0];
        *(dest_row++) = t [0] >> 32;
        src_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL16,      COMPRESSED,
                     321,   24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint64_t t [2];
        uint8_t alpha = src_row [1] >> 8;
        unpremul_p16_to_u_128bpp (src_row, t, alpha);
        t [1] = (t [1] & 0xffffffff00000000ULL) | alpha;
        *(dest_row++) = t [1] >> 32;
        *(dest_row++) = t [0];
        *(dest_row++) = t [0] >> 32;
        src_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL16,      LINEAR,
                     321,   24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint64_t t [2];
        uint8_t alpha = src_row [1] >> 8;
        unpremul_p16_to_u_128bpp (src_row, t, alpha);
        to_srgb_pixel_xxxa_128bpp (t, t);
        t [1] = (t [1] & 0xffffffff00000000ULL) | alpha;
        *(dest_row++) = t [1] >> 32;
        *(dest_row++) = t [0];
        *(dest_row++) = t [0] >> 32;
        src_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

#define DEF_REPACK_FROM_1234_128BPP_TO_32BPP(a, b, c, d) \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL8,       COMPRESSED, \
                         a##b##c##d,  32, 32, PREMUL8,       COMPRESSED) { \
        while (dest_row != dest_row_max) \
        { \
            *(dest_row++) = PACK_FROM_1234_128BPP (src_row, a, b, c, d); \
            src_row += 2; \
        } \
    } SMOL_REPACK_ROW_DEF_END \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL8,       LINEAR, \
                         a##b##c##d,  32, 32, PREMUL8,       COMPRESSED) { \
        while (dest_row != dest_row_max) \
        { \
            uint64_t t [2]; \
            uint8_t alpha = get_alpha_from_linear_xxxa_128bpp (src_row); \
            to_srgb_pixel_xxxa_128bpp (src_row, t); \
            t [1] = (t [1] & 0xffffffff00000000ULL) | alpha; \
            *(dest_row++) = PACK_FROM_1234_128BPP (t, a, b, c, d); \
            src_row += 2; \
        } \
    } SMOL_REPACK_ROW_DEF_END \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL8,       COMPRESSED, \
                         a##b##c##d,  32, 32, UNASSOCIATED,  COMPRESSED) { \
        while (dest_row != dest_row_max) \
        { \
            uint64_t t [2]; \
            uint8_t alpha = src_row [1]; \
            unpremul_p8_to_u_128bpp (src_row, t, alpha); \
            t [1] = (t [1] & 0xffffffff00000000ULL) | alpha; \
            *(dest_row++) = PACK_FROM_1234_128BPP (t, a, b, c, d); \
            src_row += 2; \
        } \
    } SMOL_REPACK_ROW_DEF_END \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL8,       LINEAR, \
                         a##b##c##d,  32, 32, UNASSOCIATED,  COMPRESSED) { \
        while (dest_row != dest_row_max) \
        { \
            uint64_t t [2]; \
            uint8_t alpha = get_alpha_from_linear_xxxa_128bpp (src_row); \
            unpremul_p8l_to_ul_128bpp (src_row, t, alpha); \
            to_srgb_pixel_xxxa_128bpp (t, t); \
            t [1] = (t [1] & 0xffffffff00000000ULL) | alpha; \
            *(dest_row++) = PACK_FROM_1234_128BPP (t, a, b, c, d); \
            src_row += 2; \
        } \
    } SMOL_REPACK_ROW_DEF_END \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL16,      COMPRESSED, \
                         a##b##c##d,  32, 32, UNASSOCIATED,  COMPRESSED) { \
        while (dest_row != dest_row_max) \
        { \
            uint64_t t [2]; \
            uint8_t alpha = src_row [1] >> 8; \
            unpremul_p16_to_u_128bpp (src_row, t, alpha); \
            t [1] = (t [1] & 0xffffffff00000000ULL) | alpha; \
            *(dest_row++) = PACK_FROM_1234_128BPP (t, a, b, c, d); \
            src_row += 2; \
        } \
    } SMOL_REPACK_ROW_DEF_END \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL16,      LINEAR, \
                         a##b##c##d,  32, 32, UNASSOCIATED,  COMPRESSED) { \
        while (dest_row != dest_row_max) \
        { \
            uint64_t t [2]; \
            uint8_t alpha = src_row [1] >> 8; \
            unpremul_p16l_to_ul_128bpp (src_row, t, alpha); \
            to_srgb_pixel_xxxa_128bpp (t, t); \
            t [1] = (t [1] & 0xffffffff00000000ULL) | alpha; \
            *(dest_row++) = PACK_FROM_1234_128BPP (t, a, b, c, d); \
            src_row += 2; \
        } \
    } SMOL_REPACK_ROW_DEF_END

DEF_REPACK_FROM_1234_128BPP_TO_32BPP (1, 2, 3, 4)
DEF_REPACK_FROM_1234_128BPP_TO_32BPP (3, 2, 1, 4)
DEF_REPACK_FROM_1234_128BPP_TO_32BPP (4, 1, 2, 3)
DEF_REPACK_FROM_1234_128BPP_TO_32BPP (4, 3, 2, 1)

/* -------------- *
 * Filter helpers *
 * -------------- */

static SMOL_INLINE const char *
src_row_ofs_to_pointer (const SmolScaleCtx *scale_ctx,
                        uint32_t src_row_ofs)
{
    return scale_ctx->src_pixels + scale_ctx->src_rowstride * src_row_ofs;
}

static SMOL_INLINE uint64_t
weight_pixel_64bpp (uint64_t p,
                    uint16_t w)
{
    return ((p * w) >> 8) & 0x00ff00ff00ff00ffULL;
}

/* p and out may be the same address */
static SMOL_INLINE void
weight_pixel_128bpp (const uint64_t *p,
                     uint64_t *out,
                     uint16_t w)
{
    out [0] = ((p [0] * w) >> 8) & 0x00ffffff00ffffffULL;
    out [1] = ((p [1] * w) >> 8) & 0x00ffffff00ffffffULL;
}

static SMOL_INLINE void
sum_parts_64bpp (const uint64_t ** SMOL_RESTRICT parts_in,
                 uint64_t * SMOL_RESTRICT accum,
                 uint32_t n)
{
    const uint64_t * SMOL_RESTRICT pp = *parts_in;
    const uint64_t *pp_end;

    SMOL_ASSUME_ALIGNED_TO (pp, const uint64_t *, sizeof (uint64_t));

    for (pp_end = pp + n; pp < pp_end; pp++)
    {
        *accum += *pp;
    }

    *parts_in = pp;
}

static SMOL_INLINE void
sum_parts_128bpp (const uint64_t ** SMOL_RESTRICT parts_in,
                  uint64_t * SMOL_RESTRICT accum,
                  uint32_t n)
{
    const uint64_t * SMOL_RESTRICT pp = *parts_in;
    const uint64_t *pp_end;

    SMOL_ASSUME_ALIGNED_TO (pp, const uint64_t *, sizeof (uint64_t) * 2);

    for (pp_end = pp + n * 2; pp < pp_end; )
    {
        accum [0] += *(pp++);
        accum [1] += *(pp++);
    }

    *parts_in = pp;
}

static SMOL_INLINE uint64_t
scale_64bpp (uint64_t accum,
             uint64_t multiplier)
{
    uint64_t a, b;

    a = ((accum & 0x0000ffff0000ffffULL) * multiplier
         + (SMOL_BOXES_MULTIPLIER / 2) + ((SMOL_BOXES_MULTIPLIER / 2) << 32)) / SMOL_BOXES_MULTIPLIER;
    b = (((accum & 0xffff0000ffff0000ULL) >> 16) * multiplier
         + (SMOL_BOXES_MULTIPLIER / 2) + ((SMOL_BOXES_MULTIPLIER / 2) << 32)) / SMOL_BOXES_MULTIPLIER;

    return (a & 0x000000ff000000ffULL) | ((b & 0x000000ff000000ffULL) << 16);
}

static SMOL_INLINE uint64_t
scale_128bpp_half (uint64_t accum,
                   uint64_t multiplier)
{
    uint64_t a, b;

    a = accum & 0x00000000ffffffffULL;
    a = (a * multiplier + SMOL_BOXES_MULTIPLIER / 2) / SMOL_BOXES_MULTIPLIER;

    b = (accum & 0xffffffff00000000ULL) >> 32;
    b = (b * multiplier + SMOL_BOXES_MULTIPLIER / 2) / SMOL_BOXES_MULTIPLIER;

    return a | (b << 32);
}

static SMOL_INLINE void
scale_and_store_128bpp (const uint64_t * SMOL_RESTRICT accum,
                        uint64_t multiplier,
                        uint64_t ** SMOL_RESTRICT dest_row_parts)
{
    *(*dest_row_parts)++ = scale_128bpp_half (accum [0], multiplier);
    *(*dest_row_parts)++ = scale_128bpp_half (accum [1], multiplier);
}

static void
add_parts (const uint64_t * SMOL_RESTRICT parts_in,
           uint64_t * SMOL_RESTRICT parts_acc_out,
           uint32_t n)
{
    const uint64_t *parts_in_max = parts_in + n;

    SMOL_ASSUME_ALIGNED (parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (parts_acc_out, uint64_t *);

    while (parts_in < parts_in_max)
        *(parts_acc_out++) += *(parts_in++);
}

static void
copy_weighted_parts_64bpp (const uint64_t * SMOL_RESTRICT parts_in,
                           uint64_t * SMOL_RESTRICT parts_acc_out,
                           uint32_t n,
                           uint16_t w)
{
    const uint64_t *parts_in_max = parts_in + n;

    SMOL_ASSUME_ALIGNED (parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (parts_acc_out, uint64_t *);

    while (parts_in < parts_in_max)
    {
        *(parts_acc_out++) = weight_pixel_64bpp (*(parts_in++), w);
    }
}

static void
copy_weighted_parts_128bpp (const uint64_t * SMOL_RESTRICT parts_in,
                            uint64_t * SMOL_RESTRICT parts_acc_out,
                            uint32_t n,
                            uint16_t w)
{
    const uint64_t *parts_in_max = parts_in + n * 2;

    SMOL_ASSUME_ALIGNED (parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (parts_acc_out, uint64_t *);

    while (parts_in < parts_in_max)
    {
        weight_pixel_128bpp (parts_in, parts_acc_out, w);
        parts_in += 2;
        parts_acc_out += 2;
    }
}

static void
add_weighted_parts_64bpp (const uint64_t * SMOL_RESTRICT parts_in,
                          uint64_t * SMOL_RESTRICT parts_acc_out,
                          uint32_t n,
                          uint16_t w)
{
    const uint64_t *parts_in_max = parts_in + n;

    SMOL_ASSUME_ALIGNED (parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (parts_acc_out, uint64_t *);

    while (parts_in < parts_in_max)
    {
        *(parts_acc_out++) += weight_pixel_64bpp (*(parts_in++), w);
    }
}

static void
add_weighted_parts_128bpp (const uint64_t * SMOL_RESTRICT parts_in,
                           uint64_t * SMOL_RESTRICT parts_acc_out,
                           uint32_t n,
                           uint16_t w)
{
    const uint64_t *parts_in_max = parts_in + n * 2;

    SMOL_ASSUME_ALIGNED (parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (parts_acc_out, uint64_t *);

    while (parts_in < parts_in_max)
    {
        uint64_t t [2];

        weight_pixel_128bpp (parts_in, t, w);
        parts_acc_out [0] += t [0];
        parts_acc_out [1] += t [1];
        parts_in += 2;
        parts_acc_out += 2;
    }
}

static SMOL_INLINE void
apply_subpixel_opacity_64bpp (uint64_t * SMOL_RESTRICT u64_inout, uint16_t opacity)
{
    *u64_inout = ((*u64_inout * opacity) >> SMOL_SUBPIXEL_SHIFT) & 0x00ff00ff00ff00ffULL;
}

static SMOL_INLINE void
apply_subpixel_opacity_128bpp_half (uint64_t * SMOL_RESTRICT u64_inout, uint16_t opacity)
{
    *u64_inout = ((*u64_inout * opacity) >> SMOL_SUBPIXEL_SHIFT) & 0x00ffffff00ffffffULL;
}

static SMOL_INLINE void
apply_subpixel_opacity_128bpp (uint64_t *u64_inout, uint16_t opacity)
{
    apply_subpixel_opacity_128bpp_half (u64_inout, opacity);
    apply_subpixel_opacity_128bpp_half (u64_inout + 1, opacity);
}

static void
apply_subpixel_opacity_row_copy_64bpp (uint64_t * SMOL_RESTRICT u64_in,
                                       uint64_t * SMOL_RESTRICT u64_out,
                                       int n_pixels,
                                       uint16_t opacity)
{
    uint64_t *u64_out_max = u64_out + n_pixels;

    while (u64_out != u64_out_max)
    {
        *u64_out = *u64_in++;
        apply_subpixel_opacity_64bpp (u64_out, opacity);
        u64_out++;
    }
}

static void
apply_subpixel_opacity_row_copy_128bpp (uint64_t * SMOL_RESTRICT u64_in,
                                        uint64_t * SMOL_RESTRICT u64_out,
                                        int n_pixels,
                                        uint16_t opacity)
{
    uint64_t *u64_out_max = u64_out + (n_pixels * 2);

    while (u64_out != u64_out_max)
    {
        u64_out [0] = u64_in [0];
        u64_out [1] = u64_in [1];
        apply_subpixel_opacity_128bpp_half (u64_out, opacity);
        apply_subpixel_opacity_128bpp_half (u64_out + 1, opacity);
        u64_in += 2;
        u64_out += 2;
    }
}

static void
apply_horiz_edge_opacity (const SmolScaleCtx *scale_ctx,
                          uint64_t *row_parts)
{
    if (scale_ctx->storage_type == SMOL_STORAGE_64BPP)
    {
        apply_subpixel_opacity_64bpp (&row_parts [0], scale_ctx->hdim.first_opacity);
        apply_subpixel_opacity_64bpp (&row_parts [scale_ctx->hdim.placement_size_px - 1], scale_ctx->hdim.last_opacity);
    }
    else
    {
        apply_subpixel_opacity_128bpp (&row_parts [0], scale_ctx->hdim.first_opacity);
        apply_subpixel_opacity_128bpp (&row_parts [(scale_ctx->hdim.placement_size_px - 1) * 2], scale_ctx->hdim.last_opacity);
    }
}

/* ------------------ *
 * Horizontal scaling *
 * ------------------ */

#define DEF_INTERP_HORIZONTAL_BILINEAR(n_halvings) \
static void \
interp_horizontal_bilinear_##n_halvings##h_64bpp (const SmolScaleCtx *scale_ctx, \
                                                  const uint64_t * SMOL_RESTRICT src_row_parts, \
                                                  uint64_t * SMOL_RESTRICT dest_row_parts) \
{ \
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc; \
    uint64_t *dest_row_parts_max = dest_row_parts + scale_ctx->hdim.placement_size_px; \
    uint64_t p, q; \
    uint64_t F; \
    int i; \
\
    SMOL_ASSUME_ALIGNED (src_row_parts, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (dest_row_parts, uint64_t *); \
\
    do \
    { \
        uint64_t accum = 0; \
\
        for (i = 0; i < (1 << (n_halvings)); i++) \
        { \
            uint64_t pixel_ofs = *(precalc_x++); \
            F = *(precalc_x++); \
\
            p = src_row_parts [pixel_ofs]; \
            q = src_row_parts [pixel_ofs + 1]; \
\
            accum += ((((p - q) * F) >> 8) + q) & 0x00ff00ff00ff00ffULL; \
            } \
        *(dest_row_parts++) = ((accum) >> (n_halvings)) & 0x00ff00ff00ff00ffULL; \
    } \
    while (dest_row_parts != dest_row_parts_max); \
} \
\
static void \
interp_horizontal_bilinear_##n_halvings##h_128bpp (const SmolScaleCtx *scale_ctx, \
                                                   const uint64_t * SMOL_RESTRICT src_row_parts, \
                                                   uint64_t * SMOL_RESTRICT dest_row_parts) \
{ \
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc; \
    uint64_t *dest_row_parts_max = dest_row_parts + scale_ctx->hdim.placement_size_px * 2; \
    uint64_t p, q; \
    uint64_t F; \
    int i; \
\
    SMOL_ASSUME_ALIGNED (src_row_parts, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (dest_row_parts, uint64_t *); \
\
    do \
    { \
        uint64_t accum [2] = { 0 }; \
         \
        for (i = 0; i < (1 << (n_halvings)); i++) \
        { \
            uint32_t pixel_ofs = *(precalc_x++) * 2; \
            F = *(precalc_x++); \
\
            p = src_row_parts [pixel_ofs]; \
            q = src_row_parts [pixel_ofs + 2]; \
\
            accum [0] += ((((p - q) * F) >> 8) + q) & 0x00ffffff00ffffffULL; \
\
            p = src_row_parts [pixel_ofs + 1]; \
            q = src_row_parts [pixel_ofs + 3]; \
\
            accum [1] += ((((p - q) * F) >> 8) + q) & 0x00ffffff00ffffffULL; \
        } \
        *(dest_row_parts++) = ((accum [0]) >> (n_halvings)) & 0x00ffffff00ffffffULL; \
        *(dest_row_parts++) = ((accum [1]) >> (n_halvings)) & 0x00ffffff00ffffffULL; \
    } \
    while (dest_row_parts != dest_row_parts_max); \
}

static void
interp_horizontal_bilinear_0h_64bpp (const SmolScaleCtx *scale_ctx,
                                     const uint64_t * SMOL_RESTRICT src_row_parts,
                                     uint64_t * SMOL_RESTRICT dest_row_parts)
{
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc;
    uint64_t * SMOL_RESTRICT dest_row_parts_max = dest_row_parts + scale_ctx->hdim.placement_size_px;
    uint64_t p, q;
    uint64_t F;

    SMOL_ASSUME_ALIGNED (src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_row_parts, uint64_t *);

    do
    {
        uint32_t pixel_ofs = *(precalc_x++);
        F = *(precalc_x++);

        p = src_row_parts [pixel_ofs];
        q = src_row_parts [pixel_ofs + 1];

        *(dest_row_parts++) = ((((p - q) * F) >> 8) + q) & 0x00ff00ff00ff00ffULL;
    }
    while (dest_row_parts != dest_row_parts_max);
}

static void
interp_horizontal_bilinear_0h_128bpp (const SmolScaleCtx *scale_ctx,
                                      const uint64_t * SMOL_RESTRICT src_row_parts,
                                      uint64_t * SMOL_RESTRICT dest_row_parts)
{
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc;
    uint64_t * SMOL_RESTRICT dest_row_parts_max = dest_row_parts + scale_ctx->hdim.placement_size_px * 2;
    uint64_t p, q;
    uint64_t F;

    SMOL_ASSUME_ALIGNED (src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_row_parts, uint64_t *);

    do
    {
        uint32_t pixel_ofs = *(precalc_x++) * 2;
        F = *(precalc_x++);

        p = src_row_parts [pixel_ofs];
        q = src_row_parts [pixel_ofs + 2];

        *(dest_row_parts++) = ((((p - q) * F) >> 8) + q) & 0x00ffffff00ffffffULL;

        p = src_row_parts [pixel_ofs + 1];
        q = src_row_parts [pixel_ofs + 3];

        *(dest_row_parts++) = ((((p - q) * F) >> 8) + q) & 0x00ffffff00ffffffULL;
    }
    while (dest_row_parts != dest_row_parts_max);
}

DEF_INTERP_HORIZONTAL_BILINEAR(1)
DEF_INTERP_HORIZONTAL_BILINEAR(2)
DEF_INTERP_HORIZONTAL_BILINEAR(3)
DEF_INTERP_HORIZONTAL_BILINEAR(4)
DEF_INTERP_HORIZONTAL_BILINEAR(5)
DEF_INTERP_HORIZONTAL_BILINEAR(6)

static SMOL_INLINE void
unpack_box_precalc (const uint32_t precalc,
                    uint32_t step,
                    uint32_t *ofs0,
                    uint32_t *ofs1,
                    uint32_t *f0,
                    uint32_t *f1,
                    uint32_t *n)
{
    *ofs0 = precalc;
    *ofs1 = *ofs0 + step;
    *f0 = 256 - (*ofs0 % SMOL_SUBPIXEL_MUL);
    *f1 = *ofs1 % SMOL_SUBPIXEL_MUL;
    *ofs0 /= SMOL_SUBPIXEL_MUL;
    *ofs1 /= SMOL_SUBPIXEL_MUL;
    *n = *ofs1 - *ofs0 - 1;
}

static void
interp_horizontal_boxes_64bpp (const SmolScaleCtx *scale_ctx,
                               const uint64_t *src_row_parts,
                               uint64_t * SMOL_RESTRICT dest_row_parts)
{
    const uint64_t * SMOL_RESTRICT pp;
    const uint32_t *precalc_x = scale_ctx->hdim.precalc;
    uint64_t *dest_row_parts_max = dest_row_parts + scale_ctx->hdim.placement_size_px;
    uint64_t accum;

    SMOL_ASSUME_ALIGNED (src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_row_parts, uint64_t *);

    while (dest_row_parts < dest_row_parts_max)
    {
        uint32_t ofs0, ofs1;
        uint32_t f0, f1;
        uint32_t n;

        unpack_box_precalc (*(precalc_x++),
                            scale_ctx->hdim.span_step,
                            &ofs0,
                            &ofs1,
                            &f0,
                            &f1,
                            &n);

        pp = src_row_parts + ofs0;

        accum = weight_pixel_64bpp (*(pp++), f0);
        sum_parts_64bpp ((const uint64_t ** SMOL_RESTRICT) &pp, &accum, n);
        accum += weight_pixel_64bpp (*pp, f1);

        *(dest_row_parts++) = scale_64bpp (accum, scale_ctx->hdim.span_mul);
    }
}

static void
interp_horizontal_boxes_128bpp (const SmolScaleCtx *scale_ctx,
                                const uint64_t *src_row_parts,
                                uint64_t * SMOL_RESTRICT dest_row_parts)
{
    const uint64_t * SMOL_RESTRICT pp;
    const uint32_t *precalc_x = scale_ctx->hdim.precalc;
    uint64_t *dest_row_parts_max = dest_row_parts + scale_ctx->hdim.placement_size_px * 2;
    uint64_t accum [2];

    SMOL_ASSUME_ALIGNED (src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_row_parts, uint64_t *);

    while (dest_row_parts < dest_row_parts_max)
    {
        uint32_t ofs0, ofs1;
        uint32_t f0, f1;
        uint32_t n;
        uint64_t t [2];

        unpack_box_precalc (*(precalc_x++),
                            scale_ctx->hdim.span_step,
                            &ofs0,
                            &ofs1,
                            &f0,
                            &f1,
                            &n);

        pp = src_row_parts + (ofs0 * 2);

        weight_pixel_128bpp (pp, accum, f0);
        pp += 2;

        sum_parts_128bpp ((const uint64_t ** SMOL_RESTRICT) &pp, accum, n);

        weight_pixel_128bpp (pp, t, f1);
        accum [0] += t [0];
        accum [1] += t [1];

        scale_and_store_128bpp (accum,
                                scale_ctx->hdim.span_mul,
                                (uint64_t ** SMOL_RESTRICT) &dest_row_parts);
    }
}

static void
interp_horizontal_one_64bpp (const SmolScaleCtx *scale_ctx,
                             const uint64_t * SMOL_RESTRICT src_row_parts,
                             uint64_t * SMOL_RESTRICT dest_row_parts)
{
    uint64_t *dest_row_parts_max = dest_row_parts + scale_ctx->hdim.placement_size_px;
    uint64_t part;

    SMOL_ASSUME_ALIGNED (src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_row_parts, uint64_t *);

    part = *src_row_parts;
    while (dest_row_parts != dest_row_parts_max)
        *(dest_row_parts++) = part;
}

static void
interp_horizontal_one_128bpp (const SmolScaleCtx *scale_ctx,
                              const uint64_t * SMOL_RESTRICT src_row_parts,
                              uint64_t * SMOL_RESTRICT dest_row_parts)
{
    uint64_t *dest_row_parts_max = dest_row_parts + scale_ctx->hdim.placement_size_px * 2;

    SMOL_ASSUME_ALIGNED (src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_row_parts, uint64_t *);

    while (dest_row_parts != dest_row_parts_max)
    {
        *(dest_row_parts++) = src_row_parts [0];
        *(dest_row_parts++) = src_row_parts [1];
    }
}

static void
interp_horizontal_copy_64bpp (const SmolScaleCtx *scale_ctx,
                              const uint64_t * SMOL_RESTRICT src_row_parts,
                              uint64_t * SMOL_RESTRICT dest_row_parts)
{
    SMOL_ASSUME_ALIGNED (src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_row_parts, uint64_t *);

    memcpy (dest_row_parts, src_row_parts, scale_ctx->hdim.placement_size_px * sizeof (uint64_t));
}

static void
interp_horizontal_copy_128bpp (const SmolScaleCtx *scale_ctx,
                               const uint64_t * SMOL_RESTRICT src_row_parts,
                               uint64_t * SMOL_RESTRICT dest_row_parts)
{
    SMOL_ASSUME_ALIGNED (src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_row_parts, uint64_t *);

    memcpy (dest_row_parts, src_row_parts, scale_ctx->hdim.placement_size_px * 2 * sizeof (uint64_t));
}

static void
scale_horizontal (const SmolScaleCtx *scale_ctx,
                  SmolLocalCtx *local_ctx,
                  const char *src_row,
                  uint64_t *dest_row_parts)
{
    uint64_t * SMOL_RESTRICT src_row_unpacked;

    src_row_unpacked = local_ctx->parts_row [3];

    /* 32-bit unpackers need 32-bit alignment */
    if ((((uintptr_t) src_row) & 3)
        && scale_ctx->src_pixel_type != SMOL_PIXEL_RGB8
        && scale_ctx->src_pixel_type != SMOL_PIXEL_BGR8)
    {
        if (!local_ctx->src_aligned)
            local_ctx->src_aligned =
                smol_alloc_aligned (scale_ctx->hdim.src_size_px * sizeof (uint32_t),
                                    &local_ctx->src_aligned_storage);
        memcpy (local_ctx->src_aligned, src_row, scale_ctx->hdim.src_size_px * sizeof (uint32_t));
        src_row = (const char *) local_ctx->src_aligned;
    }

    scale_ctx->src_unpack_row_func (src_row,
                                    src_row_unpacked,
                                    scale_ctx->hdim.src_size_px);
    scale_ctx->hfilter_func (scale_ctx,
                             src_row_unpacked,
                             dest_row_parts);

    apply_horiz_edge_opacity (scale_ctx, dest_row_parts);
}

/* ---------------- *
 * Vertical scaling *
 * ---------------- */

static void
update_local_ctx_bilinear (const SmolScaleCtx *scale_ctx,
                           SmolLocalCtx *local_ctx,
                           uint32_t dest_row_index)
{
    uint16_t *precalc_y = scale_ctx->vdim.precalc;
    uint32_t new_src_ofs = precalc_y [dest_row_index * 2];

    if (new_src_ofs == local_ctx->src_ofs)
        return;

    if (new_src_ofs == local_ctx->src_ofs + 1)
    {
        uint64_t *t = local_ctx->parts_row [0];
        local_ctx->parts_row [0] = local_ctx->parts_row [1];
        local_ctx->parts_row [1] = t;

        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, new_src_ofs + 1),
                          local_ctx->parts_row [1]);
    }
    else
    {
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, new_src_ofs),
                          local_ctx->parts_row [0]);
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, new_src_ofs + 1),
                          local_ctx->parts_row [1]);
    }

    local_ctx->src_ofs = new_src_ofs;
}

static void
interp_vertical_bilinear_store_64bpp (uint64_t F,
                                      const uint64_t * SMOL_RESTRICT top_src_row_parts,
                                      const uint64_t * SMOL_RESTRICT bottom_src_row_parts,
                                      uint64_t * SMOL_RESTRICT dest_parts,
                                      uint32_t width)
{
    uint64_t *parts_dest_last = dest_parts + width;

    SMOL_ASSUME_ALIGNED (top_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (bottom_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_parts, uint64_t *);

    do
    {
        uint64_t p, q;

        p = *(top_src_row_parts++);
        q = *(bottom_src_row_parts++);

        *(dest_parts++) = ((((p - q) * F) >> 8) + q) & 0x00ff00ff00ff00ffULL;
    }
    while (dest_parts != parts_dest_last);
}

static void
interp_vertical_bilinear_store_with_opacity_64bpp (uint64_t F,
                                                   const uint64_t * SMOL_RESTRICT top_src_row_parts,
                                                   const uint64_t * SMOL_RESTRICT bottom_src_row_parts,
                                                   uint64_t * SMOL_RESTRICT dest_parts,
                                                   uint32_t width,
                                                   uint16_t opacity)
{
    uint64_t *parts_dest_last = dest_parts + width;

    SMOL_ASSUME_ALIGNED (top_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (bottom_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_parts, uint64_t *);

    do
    {
        uint64_t p, q;

        p = *(top_src_row_parts++);
        q = *(bottom_src_row_parts++);

        *dest_parts = ((((p - q) * F) >> 8) + q) & 0x00ff00ff00ff00ffULL;
        apply_subpixel_opacity_64bpp (dest_parts, opacity);
        dest_parts++;
    }
    while (dest_parts != parts_dest_last);
}

static void
interp_vertical_bilinear_add_64bpp (uint64_t F,
                                    const uint64_t * SMOL_RESTRICT top_src_row_parts,
                                    const uint64_t * SMOL_RESTRICT bottom_src_row_parts,
                                    uint64_t * SMOL_RESTRICT accum_out,
                                    uint32_t width)
{
    uint64_t *accum_dest_last = accum_out + width;

    SMOL_ASSUME_ALIGNED (top_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (bottom_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (accum_out, uint64_t *);

    do
    {
        uint64_t p, q;

        p = *(top_src_row_parts++);
        q = *(bottom_src_row_parts++);

        *(accum_out++) += ((((p - q) * F) >> 8) + q) & 0x00ff00ff00ff00ffULL;
    }
    while (accum_out != accum_dest_last);
}

static void
interp_vertical_bilinear_store_128bpp (uint64_t F,
                                       const uint64_t * SMOL_RESTRICT top_src_row_parts,
                                       const uint64_t * SMOL_RESTRICT bottom_src_row_parts,
                                       uint64_t * SMOL_RESTRICT dest_parts,
                                       uint32_t width)
{
    uint64_t *parts_dest_last = dest_parts + width;

    SMOL_ASSUME_ALIGNED (top_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (bottom_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_parts, uint64_t *);

    do
    {
        uint64_t p, q;

        p = *(top_src_row_parts++);
        q = *(bottom_src_row_parts++);

        *(dest_parts++) = ((((p - q) * F) >> 8) + q) & 0x00ffffff00ffffffULL;
    }
    while (dest_parts != parts_dest_last);
}

static void
interp_vertical_bilinear_store_with_opacity_128bpp (uint64_t F,
                                                    const uint64_t * SMOL_RESTRICT top_src_row_parts,
                                                    const uint64_t * SMOL_RESTRICT bottom_src_row_parts,
                                                    uint64_t * SMOL_RESTRICT dest_parts,
                                                    uint32_t width,
                                                    uint16_t opacity)
{
    uint64_t *parts_dest_last = dest_parts + width;

    SMOL_ASSUME_ALIGNED (top_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (bottom_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_parts, uint64_t *);

    do
    {
        uint64_t p, q;

        p = *(top_src_row_parts++);
        q = *(bottom_src_row_parts++);

        *dest_parts = ((((p - q) * F) >> 8) + q) & 0x00ffffff00ffffffULL;
        apply_subpixel_opacity_128bpp_half (dest_parts, opacity);
        dest_parts++;
    }
    while (dest_parts != parts_dest_last);
}

static void
interp_vertical_bilinear_add_128bpp (uint64_t F,
                                     const uint64_t * SMOL_RESTRICT top_src_row_parts,
                                     const uint64_t * SMOL_RESTRICT bottom_src_row_parts,
                                     uint64_t * SMOL_RESTRICT accum_out,
                                     uint32_t width)
{
    uint64_t *accum_dest_last = accum_out + width;

    SMOL_ASSUME_ALIGNED (top_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (bottom_src_row_parts, const uint64_t *);
    SMOL_ASSUME_ALIGNED (accum_out, uint64_t *);

    do
    {
        uint64_t p, q;

        p = *(top_src_row_parts++);
        q = *(bottom_src_row_parts++);

        *(accum_out++) += ((((p - q) * F) >> 8) + q) & 0x00ffffff00ffffffULL;
    }
    while (accum_out != accum_dest_last);
}

#define DEF_INTERP_VERTICAL_BILINEAR_FINAL(n_halvings) \
static void \
interp_vertical_bilinear_final_##n_halvings##h_64bpp (uint64_t F, \
                                                      const uint64_t * SMOL_RESTRICT top_src_row_parts, \
                                                      const uint64_t * SMOL_RESTRICT bottom_src_row_parts, \
                                                      uint64_t * SMOL_RESTRICT accum_inout, \
                                                      uint32_t width) \
{ \
    uint64_t *accum_inout_last = accum_inout + width; \
\
    SMOL_ASSUME_ALIGNED (top_src_row_parts, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (bottom_src_row_parts, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (accum_inout, uint64_t *); \
\
    do \
    { \
        uint64_t p, q; \
\
        p = *(top_src_row_parts++); \
        q = *(bottom_src_row_parts++); \
\
        p = ((((p - q) * F) >> 8) + q) & 0x00ff00ff00ff00ffULL; \
        p = ((p + *accum_inout) >> n_halvings) & 0x00ff00ff00ff00ffULL; \
\
        *(accum_inout++) = p; \
    } \
    while (accum_inout != accum_inout_last); \
} \
\
static void \
interp_vertical_bilinear_final_##n_halvings##h_with_opacity_64bpp (uint64_t F, \
                                                                   const uint64_t * SMOL_RESTRICT top_src_row_parts, \
                                                                   const uint64_t * SMOL_RESTRICT bottom_src_row_parts, \
                                                                   uint64_t * SMOL_RESTRICT accum_inout, \
                                                                   uint32_t width, \
                                                                   uint16_t opacity) \
{ \
    uint64_t *accum_inout_last = accum_inout + width; \
\
    SMOL_ASSUME_ALIGNED (top_src_row_parts, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (bottom_src_row_parts, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (accum_inout, uint64_t *); \
\
    do \
    { \
        uint64_t p, q; \
\
        p = *(top_src_row_parts++); \
        q = *(bottom_src_row_parts++); \
\
        p = ((((p - q) * F) >> 8) + q) & 0x00ff00ff00ff00ffULL; \
        p = ((p + *accum_inout) >> n_halvings) & 0x00ff00ff00ff00ffULL; \
\
        apply_subpixel_opacity_64bpp (&p, opacity); \
        *(accum_inout++) = p; \
    } \
    while (accum_inout != accum_inout_last); \
} \
\
static void \
interp_vertical_bilinear_final_##n_halvings##h_128bpp (uint64_t F, \
                                                       const uint64_t * SMOL_RESTRICT top_src_row_parts, \
                                                       const uint64_t * SMOL_RESTRICT bottom_src_row_parts, \
                                                       uint64_t * SMOL_RESTRICT accum_inout, \
                                                       uint32_t width) \
{ \
    uint64_t *accum_inout_last = accum_inout + width; \
\
    SMOL_ASSUME_ALIGNED (top_src_row_parts, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (bottom_src_row_parts, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (accum_inout, uint64_t *); \
\
    do \
    { \
        uint64_t p, q; \
\
        p = *(top_src_row_parts++); \
        q = *(bottom_src_row_parts++); \
\
        p = ((((p - q) * F) >> 8) + q) & 0x00ffffff00ffffffULL; \
        p = ((p + *accum_inout) >> n_halvings) & 0x00ffffff00ffffffULL; \
\
        *(accum_inout++) = p; \
    } \
    while (accum_inout != accum_inout_last); \
} \
\
static void \
interp_vertical_bilinear_final_##n_halvings##h_with_opacity_128bpp (uint64_t F, \
                                                                    const uint64_t * SMOL_RESTRICT top_src_row_parts, \
                                                                    const uint64_t * SMOL_RESTRICT bottom_src_row_parts, \
                                                                    uint64_t * SMOL_RESTRICT accum_inout, \
                                                                    uint32_t width, \
                                                                    uint16_t opacity) \
{ \
    uint64_t *accum_inout_last = accum_inout + width; \
\
    SMOL_ASSUME_ALIGNED (top_src_row_parts, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (bottom_src_row_parts, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (accum_inout, uint64_t *); \
\
    do \
    { \
        uint64_t p, q; \
\
        p = *(top_src_row_parts++); \
        q = *(bottom_src_row_parts++); \
\
        p = ((((p - q) * F) >> 8) + q) & 0x00ffffff00ffffffULL; \
        p = ((p + *accum_inout) >> n_halvings) & 0x00ffffff00ffffffULL; \
\
        apply_subpixel_opacity_128bpp_half (&p, opacity); \
        *(accum_inout++) = p; \
    } \
    while (accum_inout != accum_inout_last); \
}

#define DEF_SCALE_DEST_ROW_BILINEAR(n_halvings) \
static int \
scale_dest_row_bilinear_##n_halvings##h_64bpp (const SmolScaleCtx *scale_ctx, \
                                               SmolLocalCtx *local_ctx, \
                                               uint32_t dest_row_index) \
{ \
    uint16_t *precalc_y = scale_ctx->vdim.precalc; \
    uint32_t bilin_index = dest_row_index << (n_halvings); \
    unsigned int i; \
\
    update_local_ctx_bilinear (scale_ctx, local_ctx, bilin_index); \
    interp_vertical_bilinear_store_64bpp (precalc_y [bilin_index * 2 + 1], \
                                          local_ctx->parts_row [0], \
                                          local_ctx->parts_row [1], \
                                          local_ctx->parts_row [2], \
                                          scale_ctx->hdim.placement_size_px); \
    bilin_index++; \
\
    for (i = 0; i < (1 << (n_halvings)) - 2; i++) \
    { \
        update_local_ctx_bilinear (scale_ctx, local_ctx, bilin_index); \
        interp_vertical_bilinear_add_64bpp (precalc_y [bilin_index * 2 + 1], \
                                            local_ctx->parts_row [0], \
                                            local_ctx->parts_row [1], \
                                            local_ctx->parts_row [2], \
                                            scale_ctx->hdim.placement_size_px); \
        bilin_index++; \
    } \
\
    update_local_ctx_bilinear (scale_ctx, local_ctx, bilin_index); \
\
    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < 256) \
        interp_vertical_bilinear_final_##n_halvings##h_with_opacity_64bpp (precalc_y [bilin_index * 2 + 1], \
                                                                           local_ctx->parts_row [0], \
                                                                           local_ctx->parts_row [1], \
                                                                           local_ctx->parts_row [2], \
                                                                           scale_ctx->hdim.placement_size_px, \
                                                                           scale_ctx->vdim.first_opacity); \
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < 256) \
        interp_vertical_bilinear_final_##n_halvings##h_with_opacity_64bpp (precalc_y [bilin_index * 2 + 1], \
                                                                           local_ctx->parts_row [0], \
                                                                           local_ctx->parts_row [1], \
                                                                           local_ctx->parts_row [2], \
                                                                           scale_ctx->hdim.placement_size_px, \
                                                                           scale_ctx->vdim.last_opacity); \
    else \
        interp_vertical_bilinear_final_##n_halvings##h_64bpp (precalc_y [bilin_index * 2 + 1], \
                                                              local_ctx->parts_row [0], \
                                                              local_ctx->parts_row [1], \
                                                              local_ctx->parts_row [2], \
                                                              scale_ctx->hdim.placement_size_px); \
\
    return 2; \
} \
\
static int \
scale_dest_row_bilinear_##n_halvings##h_128bpp (const SmolScaleCtx *scale_ctx, \
                                                SmolLocalCtx *local_ctx, \
                                                uint32_t dest_row_index) \
{ \
    uint16_t *precalc_y = scale_ctx->vdim.precalc; \
    uint32_t bilin_index = dest_row_index << (n_halvings); \
    unsigned int i; \
\
    update_local_ctx_bilinear (scale_ctx, local_ctx, bilin_index); \
    interp_vertical_bilinear_store_128bpp (precalc_y [bilin_index * 2 + 1], \
                                           local_ctx->parts_row [0], \
                                           local_ctx->parts_row [1], \
                                           local_ctx->parts_row [2], \
                                           scale_ctx->hdim.placement_size_px * 2); \
    bilin_index++; \
\
    for (i = 0; i < (1 << (n_halvings)) - 2; i++) \
    { \
        update_local_ctx_bilinear (scale_ctx, local_ctx, bilin_index); \
        interp_vertical_bilinear_add_128bpp (precalc_y [bilin_index * 2 + 1], \
                                             local_ctx->parts_row [0], \
                                             local_ctx->parts_row [1], \
                                             local_ctx->parts_row [2], \
                                             scale_ctx->hdim.placement_size_px * 2); \
        bilin_index++; \
    } \
\
    update_local_ctx_bilinear (scale_ctx, local_ctx, bilin_index); \
\
    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < 256) \
        interp_vertical_bilinear_final_##n_halvings##h_with_opacity_128bpp (precalc_y [bilin_index * 2 + 1], \
                                                                            local_ctx->parts_row [0], \
                                                                            local_ctx->parts_row [1], \
                                                                            local_ctx->parts_row [2], \
                                                                            scale_ctx->hdim.placement_size_px * 2, \
                                                                            scale_ctx->vdim.first_opacity); \
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < 256) \
        interp_vertical_bilinear_final_##n_halvings##h_with_opacity_128bpp (precalc_y [bilin_index * 2 + 1], \
                                                                            local_ctx->parts_row [0], \
                                                                            local_ctx->parts_row [1], \
                                                                            local_ctx->parts_row [2], \
                                                                            scale_ctx->hdim.placement_size_px * 2, \
                                                                            scale_ctx->vdim.last_opacity); \
    else \
        interp_vertical_bilinear_final_##n_halvings##h_128bpp (precalc_y [bilin_index * 2 + 1], \
                                                               local_ctx->parts_row [0], \
                                                               local_ctx->parts_row [1], \
                                                               local_ctx->parts_row [2], \
                                                               scale_ctx->hdim.placement_size_px * 2); \
\
    return 2; \
}

static int
scale_dest_row_bilinear_0h_64bpp (const SmolScaleCtx *scale_ctx,
                                  SmolLocalCtx *local_ctx,
                                  uint32_t dest_row_index)
{
    uint16_t *precalc_y = scale_ctx->vdim.precalc;

    update_local_ctx_bilinear (scale_ctx, local_ctx, dest_row_index);

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < 256)
        interp_vertical_bilinear_store_with_opacity_64bpp (precalc_y [dest_row_index * 2 + 1],
                                                           local_ctx->parts_row [0],
                                                           local_ctx->parts_row [1],
                                                           local_ctx->parts_row [2],
                                                           scale_ctx->hdim.placement_size_px,
                                                           scale_ctx->vdim.first_opacity);
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < 256)
        interp_vertical_bilinear_store_with_opacity_64bpp (precalc_y [dest_row_index * 2 + 1],
                                                           local_ctx->parts_row [0],
                                                           local_ctx->parts_row [1],
                                                           local_ctx->parts_row [2],
                                                           scale_ctx->hdim.placement_size_px,
                                                           scale_ctx->vdim.last_opacity);
    else
        interp_vertical_bilinear_store_64bpp (precalc_y [dest_row_index * 2 + 1],
                                              local_ctx->parts_row [0],
                                              local_ctx->parts_row [1],
                                              local_ctx->parts_row [2],
                                              scale_ctx->hdim.placement_size_px);

    return 2;
}

static int
scale_dest_row_bilinear_0h_128bpp (const SmolScaleCtx *scale_ctx,
                                   SmolLocalCtx *local_ctx,
                                   uint32_t dest_row_index)
{
    uint16_t *precalc_y = scale_ctx->vdim.precalc;

    update_local_ctx_bilinear (scale_ctx, local_ctx, dest_row_index);

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < 256)
        interp_vertical_bilinear_store_with_opacity_128bpp (precalc_y [dest_row_index * 2 + 1],
                                                            local_ctx->parts_row [0],
                                                            local_ctx->parts_row [1],
                                                            local_ctx->parts_row [2],
                                                            scale_ctx->hdim.placement_size_px * 2,
                                                            scale_ctx->vdim.first_opacity);
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < 256)
        interp_vertical_bilinear_store_with_opacity_128bpp (precalc_y [dest_row_index * 2 + 1],
                                                            local_ctx->parts_row [0],
                                                            local_ctx->parts_row [1],
                                                            local_ctx->parts_row [2],
                                                            scale_ctx->hdim.placement_size_px * 2,
                                                            scale_ctx->vdim.last_opacity);
    else
        interp_vertical_bilinear_store_128bpp (precalc_y [dest_row_index * 2 + 1],
                                               local_ctx->parts_row [0],
                                               local_ctx->parts_row [1],
                                               local_ctx->parts_row [2],
                                               scale_ctx->hdim.placement_size_px * 2);

    return 2;
}

DEF_INTERP_VERTICAL_BILINEAR_FINAL(1)

static int
scale_dest_row_bilinear_1h_64bpp (const SmolScaleCtx *scale_ctx,
                                  SmolLocalCtx *local_ctx,
                                  uint32_t dest_row_index)
{
    uint16_t *precalc_y = scale_ctx->vdim.precalc;
    uint32_t bilin_index = dest_row_index << 1;

    update_local_ctx_bilinear (scale_ctx, local_ctx, bilin_index);
    interp_vertical_bilinear_store_64bpp (precalc_y [bilin_index * 2 + 1],
                                          local_ctx->parts_row [0],
                                          local_ctx->parts_row [1],
                                          local_ctx->parts_row [2],
                                          scale_ctx->hdim.placement_size_px);
    bilin_index++;
    update_local_ctx_bilinear (scale_ctx, local_ctx, bilin_index);

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < 256)
        interp_vertical_bilinear_final_1h_with_opacity_64bpp (precalc_y [bilin_index * 2 + 1],
                                                              local_ctx->parts_row [0],
                                                              local_ctx->parts_row [1],
                                                              local_ctx->parts_row [2],
                                                              scale_ctx->hdim.placement_size_px,
                                                              scale_ctx->vdim.first_opacity);
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < 256)
        interp_vertical_bilinear_final_1h_with_opacity_64bpp (precalc_y [bilin_index * 2 + 1],
                                                              local_ctx->parts_row [0],
                                                              local_ctx->parts_row [1],
                                                              local_ctx->parts_row [2],
                                                              scale_ctx->hdim.placement_size_px,
                                                              scale_ctx->vdim.last_opacity);
    else
        interp_vertical_bilinear_final_1h_64bpp (precalc_y [bilin_index * 2 + 1],
                                                 local_ctx->parts_row [0],
                                                 local_ctx->parts_row [1],
                                                 local_ctx->parts_row [2],
                                                 scale_ctx->hdim.placement_size_px);

    return 2;
}

static int
scale_dest_row_bilinear_1h_128bpp (const SmolScaleCtx *scale_ctx,
                                   SmolLocalCtx *local_ctx,
                                   uint32_t dest_row_index)
{
    uint16_t *precalc_y = scale_ctx->vdim.precalc;
    uint32_t bilin_index = dest_row_index << 1;

    update_local_ctx_bilinear (scale_ctx, local_ctx, bilin_index);
    interp_vertical_bilinear_store_128bpp (precalc_y [bilin_index * 2 + 1],
                                           local_ctx->parts_row [0],
                                           local_ctx->parts_row [1],
                                           local_ctx->parts_row [2],
                                           scale_ctx->hdim.placement_size_px * 2);
    bilin_index++;
    update_local_ctx_bilinear (scale_ctx, local_ctx, bilin_index);

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < 256)
        interp_vertical_bilinear_final_1h_with_opacity_128bpp (precalc_y [bilin_index * 2 + 1],
                                                               local_ctx->parts_row [0],
                                                               local_ctx->parts_row [1],
                                                               local_ctx->parts_row [2],
                                                               scale_ctx->hdim.placement_size_px * 2,
                                                               scale_ctx->vdim.first_opacity);
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < 256)
        interp_vertical_bilinear_final_1h_with_opacity_128bpp (precalc_y [bilin_index * 2 + 1],
                                                               local_ctx->parts_row [0],
                                                               local_ctx->parts_row [1],
                                                               local_ctx->parts_row [2],
                                                               scale_ctx->hdim.placement_size_px * 2,
                                                               scale_ctx->vdim.last_opacity);
    else
        interp_vertical_bilinear_final_1h_128bpp (precalc_y [bilin_index * 2 + 1],
                                                  local_ctx->parts_row [0],
                                                  local_ctx->parts_row [1],
                                                  local_ctx->parts_row [2],
                                                  scale_ctx->hdim.placement_size_px * 2);

    return 2;
}

DEF_INTERP_VERTICAL_BILINEAR_FINAL(2)
DEF_SCALE_DEST_ROW_BILINEAR(2)
DEF_INTERP_VERTICAL_BILINEAR_FINAL(3)
DEF_SCALE_DEST_ROW_BILINEAR(3)
DEF_INTERP_VERTICAL_BILINEAR_FINAL(4)
DEF_SCALE_DEST_ROW_BILINEAR(4)
DEF_INTERP_VERTICAL_BILINEAR_FINAL(5)
DEF_SCALE_DEST_ROW_BILINEAR(5)
DEF_INTERP_VERTICAL_BILINEAR_FINAL(6)
DEF_SCALE_DEST_ROW_BILINEAR(6)

static void
finalize_vertical_64bpp (const uint64_t * SMOL_RESTRICT accums,
                         uint64_t multiplier,
                         uint64_t * SMOL_RESTRICT dest_parts,
                         uint32_t n)
{
    uint64_t *parts_dest_max = dest_parts + n;

    SMOL_ASSUME_ALIGNED (accums, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_parts, uint64_t *);

    while (dest_parts != parts_dest_max)
    {
        *(dest_parts++) = scale_64bpp (*(accums++), multiplier);
    }
}

static void
finalize_vertical_with_opacity_64bpp (const uint64_t * SMOL_RESTRICT accums,
                                      uint64_t multiplier,
                                      uint64_t * SMOL_RESTRICT dest_parts,
                                      uint32_t n,
                                      uint16_t opacity)
{
    uint64_t *parts_dest_max = dest_parts + n;

    SMOL_ASSUME_ALIGNED (accums, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_parts, uint64_t *);

    while (dest_parts != parts_dest_max)
    {
        *dest_parts = scale_64bpp (*(accums++), multiplier);
        apply_subpixel_opacity_64bpp (dest_parts, opacity);
        dest_parts++;
    }
}

static int
scale_dest_row_box_64bpp (const SmolScaleCtx *scale_ctx,
                          SmolLocalCtx *local_ctx,
                          uint32_t dest_row_index)
{
    uint32_t *precalc_y = scale_ctx->vdim.precalc;
    uint32_t ofs_y, ofs_y_max;
    uint32_t w1, w2;
    uint32_t n, i;

    unpack_box_precalc (precalc_y [dest_row_index],
                        scale_ctx->vdim.span_step,
                        &ofs_y,
                        &ofs_y_max,
                        &w1,
                        &w2,
                        &n);

    /* First input row */

    scale_horizontal (scale_ctx,
                      local_ctx,
                      src_row_ofs_to_pointer (scale_ctx, ofs_y),
                      local_ctx->parts_row [0]);
    copy_weighted_parts_64bpp (local_ctx->parts_row [0],
                               local_ctx->parts_row [1],
                               scale_ctx->hdim.placement_size_px,
                               w1);
    ofs_y++;

    /* Add up whole input rows */

    for (i = 0; i < n; i++)
    {
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, ofs_y),
                          local_ctx->parts_row [0]);
        add_parts (local_ctx->parts_row [0],
                   local_ctx->parts_row [1],
                   scale_ctx->hdim.placement_size_px);

        ofs_y++;
    }

    /* Last input row */

    if (ofs_y < scale_ctx->vdim.src_size_px)
    {
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, ofs_y),
                          local_ctx->parts_row [0]);
        add_weighted_parts_64bpp (local_ctx->parts_row [0],
                                  local_ctx->parts_row [1],
                                  scale_ctx->hdim.placement_size_px,
                                  w2);
    }

    /* Finalize */

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < 256)
    {
        finalize_vertical_with_opacity_64bpp (local_ctx->parts_row [1],
                                              scale_ctx->vdim.span_mul,
                                              local_ctx->parts_row [0],
                                              scale_ctx->hdim.placement_size_px,
                                              scale_ctx->vdim.first_opacity);
    }
    else if (dest_row_index == scale_ctx->vdim.placement_size_px - 1 && scale_ctx->vdim.last_opacity < 256)
    {
        finalize_vertical_with_opacity_64bpp (local_ctx->parts_row [1],
                                              scale_ctx->vdim.span_mul,
                                              local_ctx->parts_row [0],
                                              scale_ctx->hdim.placement_size_px,
                                              scale_ctx->vdim.last_opacity);
    }
    else
    {
        finalize_vertical_64bpp (local_ctx->parts_row [1],
                                 scale_ctx->vdim.span_mul,
                                 local_ctx->parts_row [0],
                                 scale_ctx->hdim.placement_size_px);
    }

    return 0;
}

static void
finalize_vertical_128bpp (const uint64_t * SMOL_RESTRICT accums,
                          uint64_t multiplier,
                          uint64_t * SMOL_RESTRICT dest_parts,
                          uint32_t n)
{
    uint64_t *parts_dest_max = dest_parts + n * 2;

    SMOL_ASSUME_ALIGNED (accums, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_parts, uint64_t *);

    while (dest_parts != parts_dest_max)
    {
        *(dest_parts++) = scale_128bpp_half (*(accums++), multiplier);
        *(dest_parts++) = scale_128bpp_half (*(accums++), multiplier);
    }
}

static void
finalize_vertical_with_opacity_128bpp (const uint64_t * SMOL_RESTRICT accums,
                                       uint64_t multiplier,
                                       uint64_t * SMOL_RESTRICT dest_parts,
                                       uint32_t n,
                                       uint16_t opacity)
{
    uint64_t *parts_dest_max = dest_parts + n * 2;

    SMOL_ASSUME_ALIGNED (accums, const uint64_t *);
    SMOL_ASSUME_ALIGNED (dest_parts, uint64_t *);

    while (dest_parts != parts_dest_max)
    {
        dest_parts [0] = scale_128bpp_half (*(accums++), multiplier);
        dest_parts [1] = scale_128bpp_half (*(accums++), multiplier);
        apply_subpixel_opacity_128bpp (dest_parts, opacity);
        dest_parts += 2;
    }
}

static int
scale_dest_row_box_128bpp (const SmolScaleCtx *scale_ctx,
                           SmolLocalCtx *local_ctx,
                           uint32_t dest_row_index)
{
    uint32_t *precalc_y = scale_ctx->vdim.precalc;
    uint32_t ofs_y, ofs_y_max;
    uint32_t w1, w2;
    uint32_t n, i;

    unpack_box_precalc (precalc_y [dest_row_index],
                        scale_ctx->vdim.span_step,
                        &ofs_y,
                        &ofs_y_max,
                        &w1,
                        &w2,
                        &n);

    /* First input row */

    scale_horizontal (scale_ctx,
                      local_ctx,
                      src_row_ofs_to_pointer (scale_ctx, ofs_y),
                      local_ctx->parts_row [0]);
    copy_weighted_parts_128bpp (local_ctx->parts_row [0],
                                local_ctx->parts_row [1],
                                scale_ctx->hdim.placement_size_px,
                                w1);
    ofs_y++;

    /* Add up whole input rows */

    for (i = 0; i < n; i++)
    {
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, ofs_y),
                          local_ctx->parts_row [0]);
        add_parts (local_ctx->parts_row [0],
                   local_ctx->parts_row [1],
                   scale_ctx->hdim.placement_size_px * 2);

        ofs_y++;
    }

    /* Last input row */

    if (ofs_y < scale_ctx->vdim.src_size_px)
    {
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, ofs_y),
                          local_ctx->parts_row [0]);
        add_weighted_parts_128bpp (local_ctx->parts_row [0],
                                   local_ctx->parts_row [1],
                                   scale_ctx->hdim.placement_size_px,
                                   w2);
    }

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < 256)
    {
        finalize_vertical_with_opacity_128bpp (local_ctx->parts_row [1],
                                               scale_ctx->vdim.span_mul,
                                               local_ctx->parts_row [0],
                                               scale_ctx->hdim.placement_size_px,
                                               scale_ctx->vdim.first_opacity);
    }
    else if (dest_row_index == scale_ctx->vdim.placement_size_px - 1 && scale_ctx->vdim.last_opacity < 256)
    {
        finalize_vertical_with_opacity_128bpp (local_ctx->parts_row [1],
                                               scale_ctx->vdim.span_mul,
                                               local_ctx->parts_row [0],
                                               scale_ctx->hdim.placement_size_px,
                                               scale_ctx->vdim.last_opacity);
    }
    else
    {
        finalize_vertical_128bpp (local_ctx->parts_row [1],
                                  scale_ctx->vdim.span_mul,
                                  local_ctx->parts_row [0],
                                  scale_ctx->hdim.placement_size_px);
    }

    return 0;
}

static int
scale_dest_row_one_64bpp (const SmolScaleCtx *scale_ctx,
                          SmolLocalCtx *local_ctx,
                          uint32_t row_index)
{
    /* Scale the row and store it */

    if (local_ctx->src_ofs != 0)
    {
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, 0),
                          local_ctx->parts_row [0]);
        local_ctx->src_ofs = 0;
    }

    if (row_index == 0 && scale_ctx->vdim.first_opacity < 256)
    {
        apply_subpixel_opacity_row_copy_64bpp (local_ctx->parts_row [0],
                                               local_ctx->parts_row [1],
                                               scale_ctx->hdim.placement_size_px,
                                               scale_ctx->vdim.first_opacity);
    }
    else if (row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < 256)
    {
        apply_subpixel_opacity_row_copy_64bpp (local_ctx->parts_row [0],
                                               local_ctx->parts_row [1],
                                               scale_ctx->hdim.placement_size_px,
                                               scale_ctx->vdim.last_opacity);
    }
    else
    {
        memcpy (local_ctx->parts_row [1],
                local_ctx->parts_row [0],
                scale_ctx->hdim.placement_size_px * sizeof (uint64_t));
    }

    return 1;
}

static int
scale_dest_row_one_128bpp (const SmolScaleCtx *scale_ctx,
                           SmolLocalCtx *local_ctx,
                           uint32_t row_index)
{
    /* Scale the row and store it */

    if (local_ctx->src_ofs != 0)
    {
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, 0),
                          local_ctx->parts_row [0]);
        local_ctx->src_ofs = 0;
    }

    if (row_index == 0 && scale_ctx->vdim.first_opacity < 256)
    {
        apply_subpixel_opacity_row_copy_128bpp (local_ctx->parts_row [0],
                                                local_ctx->parts_row [1],
                                                scale_ctx->hdim.placement_size_px,
                                                scale_ctx->vdim.first_opacity);
    }
    else if (row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < 256)
    {
        apply_subpixel_opacity_row_copy_128bpp (local_ctx->parts_row [0],
                                                local_ctx->parts_row [1],
                                                scale_ctx->hdim.placement_size_px,
                                                scale_ctx->vdim.last_opacity);
    }
    else
    {
        memcpy (local_ctx->parts_row [1],
                local_ctx->parts_row [0],
                scale_ctx->hdim.placement_size_px * sizeof (uint64_t) * 2);
    }

    return 1;
}

static int
scale_dest_row_copy (const SmolScaleCtx *scale_ctx,
                     SmolLocalCtx *local_ctx,
                     uint32_t row_index)
{
    scale_horizontal (scale_ctx,
                      local_ctx,
                      src_row_ofs_to_pointer (scale_ctx, row_index),
                      local_ctx->parts_row [0]);

    return 0;
}

/* ----------- *
 * Compositing *
 * ----------- */

static void
composite_over_color_64bpp (uint64_t * SMOL_RESTRICT srcdest_row,
                            const uint64_t * SMOL_RESTRICT color_pixel,
                            uint32_t n_pixels)
{
    uint32_t i;

    SMOL_ASSUME_ALIGNED_TO (srcdest_row, uint64_t *, sizeof (uint64_t));
    SMOL_ASSUME_ALIGNED_TO (color_pixel, const uint64_t *, sizeof (uint64_t));

    for (i = 0; i < n_pixels; i++)
    {
        uint64_t a = srcdest_row [i] & 0xff;

        srcdest_row [i] += (((*color_pixel) * (0xff - a)) >> 8) & 0x00ff00ff00ff00ff;
    }
}

static void
composite_over_color_128bpp (uint64_t * SMOL_RESTRICT srcdest_row,
                             const uint64_t * SMOL_RESTRICT color_pixel,
                             uint32_t n_pixels)
{
    uint32_t i;

    SMOL_ASSUME_ALIGNED_TO (srcdest_row, uint64_t *, sizeof (uint64_t) * 2);
    SMOL_ASSUME_ALIGNED_TO (color_pixel, const uint64_t *, sizeof (uint64_t));

    for (i = 0; i < n_pixels * 2; i += 2)
    {
        uint64_t a = (srcdest_row [i + 1] >> 4) & 0xfff;

        srcdest_row [i] += ((color_pixel [0] * (0xfff - a)) >> 12) & 0x000fffff000fffff;
        srcdest_row [i + 1] += ((color_pixel [1] * (0xfff - a)) >> 12) & 0x000fffff000fffff;
    }
}

static void
composite_over_dest_64bpp (const uint64_t * SMOL_RESTRICT src_row,
                           uint64_t * SMOL_RESTRICT dest_row,
                           uint32_t n_pixels)
{
    uint32_t i;

    SMOL_ASSUME_ALIGNED_TO (src_row, const uint64_t *, sizeof (uint64_t));
    SMOL_ASSUME_ALIGNED_TO (dest_row, uint64_t *, sizeof (uint64_t));

    for (i = 0; i < n_pixels; i++)
    {
        dest_row [i] = ((src_row [i] + dest_row [i]) >> 1) & 0x7fff7fff7fff7fff;
    }
}

static void
composite_over_dest_128bpp (const uint64_t * SMOL_RESTRICT src_row,
                            uint64_t * SMOL_RESTRICT dest_row,
                            uint32_t n_pixels)
{
    uint32_t i;

    SMOL_ASSUME_ALIGNED_TO (src_row, const uint64_t *, sizeof (uint64_t) * 2);
    SMOL_ASSUME_ALIGNED_TO (dest_row, uint64_t *, sizeof (uint64_t) * 2);

    for (i = 0; i < n_pixels * 2; i += 2)
    {
        dest_row [i] = ((src_row [i] + dest_row [i]) >> 1) & 0x7fffffff7fffffff;
        dest_row [i + 1] = ((src_row [i + 1] + dest_row [i + 1]) >> 1) & 0x7fffffff7fffffff;
    }
}

/* -------- *
 * Clearing *
 * -------- */

static void
clear_24bpp (const void *src_pixel_batch,
             void *dest_row,
             uint32_t n_pixels)
{
    const uint8_t *src_pixel_batch_u8 = src_pixel_batch;
    const uint32_t *src_pixel_batch_u32 = src_pixel_batch;
    uint8_t *dest_row_u8 = dest_row;
    uint32_t *dest_row_u32 = dest_row;
    uint32_t i;

    SMOL_ASSUME_ALIGNED_TO (src_pixel_batch_u32, const uint32_t *, sizeof (uint32_t));

    for (i = 0; n_pixels - i >= 4; i += 4)
    {
        *(dest_row_u32++) = src_pixel_batch_u32 [0];
        *(dest_row_u32++) = src_pixel_batch_u32 [1];
        *(dest_row_u32++) = src_pixel_batch_u32 [2];
    }

    for ( ; i < n_pixels; i++)
    {
        dest_row_u8 [i * 3] = src_pixel_batch_u8 [0];
        dest_row_u8 [i * 3 + 1] = src_pixel_batch_u8 [1];
        dest_row_u8 [i * 3 + 2] = src_pixel_batch_u8 [2];
    }
}

static void
clear_32bpp (const void *src_pixel_batch,
             void *dest_row,
             uint32_t n_pixels)
{
    const uint32_t *src_pixel_batch_u32 = src_pixel_batch;
    uint32_t *dest_row_u32 = dest_row;
    uint32_t i;

    SMOL_ASSUME_ALIGNED_TO (src_pixel_batch_u32, const uint32_t *, sizeof (uint32_t));

    for (i = 0; i < n_pixels; i++)
        dest_row_u32 [i] = src_pixel_batch_u32 [0];
}

/* --------------- *
 * Function tables *
 * --------------- */

#define R SMOL_REPACK_META

static const SmolRepackMeta repack_meta [] =
{
    R (123,   24, PREMUL8,      COMPRESSED, 1324,  64, PREMUL8,       COMPRESSED),

    R (123,   24, PREMUL8,      COMPRESSED, 1234, 128, PREMUL8,       COMPRESSED),
    R (123,   24, PREMUL8,      COMPRESSED, 1234, 128, PREMUL8,       LINEAR),

    R (1234,  32, PREMUL8,      COMPRESSED, 1324,  64, PREMUL8,       COMPRESSED),
    R (1234,  32, PREMUL8,      COMPRESSED, 2431,  64, PREMUL8,       COMPRESSED),
    R (1234,  32, PREMUL8,      COMPRESSED, 3241,  64, PREMUL8,       COMPRESSED),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 1324,  64, PREMUL8,       COMPRESSED),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 2431,  64, PREMUL8,       COMPRESSED),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 3241,  64, PREMUL8,       COMPRESSED),

    R (1234,  32, PREMUL8,      COMPRESSED, 1234, 128, PREMUL8,       COMPRESSED),
    R (1234,  32, PREMUL8,      COMPRESSED, 2341, 128, PREMUL8,       COMPRESSED),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 1234, 128, PREMUL8,       COMPRESSED),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 2341, 128, PREMUL8,       COMPRESSED),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 1234, 128, PREMUL16,      COMPRESSED),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 2341, 128, PREMUL16,      COMPRESSED),
    R (1234,  32, PREMUL8,      COMPRESSED, 1234, 128, PREMUL8,       LINEAR),
    R (1234,  32, PREMUL8,      COMPRESSED, 2341, 128, PREMUL8,       LINEAR),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 1234, 128, PREMUL8,       LINEAR),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 2341, 128, PREMUL8,       LINEAR),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 1234, 128, PREMUL16,      LINEAR),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 2341, 128, PREMUL16,      LINEAR),

    R (1234,  64, PREMUL8,      COMPRESSED, 132,   24, PREMUL8,       COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 231,   24, PREMUL8,       COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 324,   24, PREMUL8,       COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 423,   24, PREMUL8,       COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 132,   24, UNASSOCIATED,  COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 231,   24, UNASSOCIATED,  COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 324,   24, UNASSOCIATED,  COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 423,   24, UNASSOCIATED,  COMPRESSED),

    R (1234,  64, PREMUL8,      COMPRESSED, 1324,  32, PREMUL8,       COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 1423,  32, PREMUL8,       COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 2314,  32, PREMUL8,       COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 4132,  32, PREMUL8,       COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 4231,  32, PREMUL8,       COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 1324,  32, UNASSOCIATED,  COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 1423,  32, UNASSOCIATED,  COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 2314,  32, UNASSOCIATED,  COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 4132,  32, UNASSOCIATED,  COMPRESSED),
    R (1234,  64, PREMUL8,      COMPRESSED, 4231,  32, UNASSOCIATED,  COMPRESSED),

    R (1234, 128, PREMUL8,      COMPRESSED, 123,   24, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      COMPRESSED, 321,   24, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      COMPRESSED, 123,   24, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      COMPRESSED, 321,   24, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     COMPRESSED, 123,   24, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     COMPRESSED, 321,   24, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     123,   24, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     321,   24, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     123,   24, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     321,   24, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     LINEAR,     123,   24, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     LINEAR,     321,   24, UNASSOCIATED,  COMPRESSED),

    R (1234, 128, PREMUL8,      COMPRESSED, 1234,  32, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      COMPRESSED, 3214,  32, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      COMPRESSED, 4123,  32, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      COMPRESSED, 4321,  32, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      COMPRESSED, 1234,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      COMPRESSED, 3214,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      COMPRESSED, 4123,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      COMPRESSED, 4321,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     COMPRESSED, 1234,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     COMPRESSED, 3214,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     COMPRESSED, 4123,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     COMPRESSED, 4321,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     1234,  32, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     3214,  32, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     4123,  32, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     4321,  32, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     1234,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     3214,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     4123,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     4321,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     LINEAR,     1234,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     LINEAR,     3214,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     LINEAR,     4123,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     LINEAR,     4321,  32, UNASSOCIATED,  COMPRESSED),

    SMOL_REPACK_META_LAST
};

#undef R

static const SmolImplementation implementation =
{
    /* Horizontal init */
    init_horizontal,

    /* Vertical init */
    init_vertical,

    {
        /* Horizontal filters */
        {
            /* 24bpp */
        },
        {
            /* 32bpp */
        },
        {
            /* 64bpp */
            interp_horizontal_copy_64bpp,
            interp_horizontal_one_64bpp,
            interp_horizontal_bilinear_0h_64bpp,
            interp_horizontal_bilinear_1h_64bpp,
            interp_horizontal_bilinear_2h_64bpp,
            interp_horizontal_bilinear_3h_64bpp,
            interp_horizontal_bilinear_4h_64bpp,
            interp_horizontal_bilinear_5h_64bpp,
            interp_horizontal_bilinear_6h_64bpp,
            interp_horizontal_boxes_64bpp
        },
        {
            /* 128bpp */
            interp_horizontal_copy_128bpp,
            interp_horizontal_one_128bpp,
            interp_horizontal_bilinear_0h_128bpp,
            interp_horizontal_bilinear_1h_128bpp,
            interp_horizontal_bilinear_2h_128bpp,
            interp_horizontal_bilinear_3h_128bpp,
            interp_horizontal_bilinear_4h_128bpp,
            interp_horizontal_bilinear_5h_128bpp,
            interp_horizontal_bilinear_6h_128bpp,
            interp_horizontal_boxes_128bpp
        }
    },
    {
        /* Vertical filters */
        {
            /* 24bpp */
        },
        {
            /* 32bpp */
        },
        {
            /* 64bpp */
            scale_dest_row_copy,
            scale_dest_row_one_64bpp,
            scale_dest_row_bilinear_0h_64bpp,
            scale_dest_row_bilinear_1h_64bpp,
            scale_dest_row_bilinear_2h_64bpp,
            scale_dest_row_bilinear_3h_64bpp,
            scale_dest_row_bilinear_4h_64bpp,
            scale_dest_row_bilinear_5h_64bpp,
            scale_dest_row_bilinear_6h_64bpp,
            scale_dest_row_box_64bpp
        },
        {
            /* 128bpp */
            scale_dest_row_copy,
            scale_dest_row_one_128bpp,
            scale_dest_row_bilinear_0h_128bpp,
            scale_dest_row_bilinear_1h_128bpp,
            scale_dest_row_bilinear_2h_128bpp,
            scale_dest_row_bilinear_3h_128bpp,
            scale_dest_row_bilinear_4h_128bpp,
            scale_dest_row_bilinear_5h_128bpp,
            scale_dest_row_bilinear_6h_128bpp,
            scale_dest_row_box_128bpp
        }
    },
    {
        /* Composite over color */
        NULL,
        NULL,
        composite_over_color_64bpp,
        composite_over_color_128bpp
    },
    {
        /* Composite over dest */
        NULL,
        NULL,
        composite_over_dest_64bpp,
        composite_over_dest_128bpp
    },
    {
        /* Clear dest */
        clear_24bpp,
        clear_32bpp,
        NULL,
        NULL
    },
    repack_meta
};

const SmolImplementation *
_smol_get_generic_implementation (void)
{
    return &implementation;
}
