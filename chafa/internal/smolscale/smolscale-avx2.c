/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright © 2019-2025 Hans Petter Jansson. See COPYING for details. */

#include <assert.h> /* assert */
#include <stdlib.h> /* malloc, free, alloca */
#include <stddef.h> /* ptrdiff_t */
#include <string.h> /* memset */
#include <limits.h>
#include <immintrin.h>
#include "smolscale-private.h"

/* ---------------------- *
 * Context initialization *
 * ---------------------- */

/* Number of horizontal pixels to process in a single batch. The define exists for
 * clarity and cannot be changed without significant changes to the code elsewhere. */
#define BILIN_HORIZ_BATCH_PIXELS 16

/* Batched precalc array layout:
 *
 * 16 offsets followed by 16 factors, repeating until epilogue. The epilogue
 * has offsets and factors alternating one by one, and will always have fewer
 * than 16 o/f pairs:
 *
 * ooooooooooooooooffffffffffffffffooooooooooooooooffffffffffffffffofofofofof...
 *
 * 16 offsets layout: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
 * 16 factors layout: 0 2 4 6 8 10 12 14 1 3 5 7 9 11 13 15
 */

static uint32_t
array_offset_offset (uint32_t elem_i, int max_index, int do_batches)
{
    if (do_batches
        && (max_index - ((elem_i / BILIN_HORIZ_BATCH_PIXELS) * BILIN_HORIZ_BATCH_PIXELS)
            >= BILIN_HORIZ_BATCH_PIXELS))
    {
        return (elem_i / (BILIN_HORIZ_BATCH_PIXELS)) * (BILIN_HORIZ_BATCH_PIXELS * 2)
            + (elem_i % BILIN_HORIZ_BATCH_PIXELS);
    }
    else
    {
        return elem_i * 2;
    }
}

static uint32_t
array_offset_factor (uint32_t elem_i, int max_index, int do_batches)
{
    const uint8_t o [BILIN_HORIZ_BATCH_PIXELS] = { 0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15 };

    if (do_batches
        && (max_index - ((elem_i / BILIN_HORIZ_BATCH_PIXELS) * BILIN_HORIZ_BATCH_PIXELS)
            >= BILIN_HORIZ_BATCH_PIXELS))
    {
        return (elem_i / (BILIN_HORIZ_BATCH_PIXELS)) * (BILIN_HORIZ_BATCH_PIXELS * 2)
            + BILIN_HORIZ_BATCH_PIXELS + o [elem_i % BILIN_HORIZ_BATCH_PIXELS];
    }
    else
    {
        return elem_i * 2 + 1;
    }
}

static void
precalc_linear_range (uint16_t *array_out,
                      int first_index,
                      int last_index,
                      int max_index,
                      uint64_t first_sample_ofs,
                      uint64_t sample_step,
                      int sample_ofs_px_max,
                      int32_t dest_clip_before_px,
                      int do_batches,
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
                array_out [array_offset_offset ((*array_i_inout), max_index, do_batches)] = sample_ofs_px_max - 2;
                array_out [array_offset_factor ((*array_i_inout), max_index, do_batches)] = 0;
                (*array_i_inout)++;
            }
            continue;
        }

        if (i >= dest_clip_before_px)
        {
            array_out [array_offset_offset ((*array_i_inout), max_index, do_batches)] = sample_ofs_px;
            array_out [array_offset_factor ((*array_i_inout), max_index, do_batches)] = SMOL_SMALL_MUL
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
                        int32_t dest_clip_before_px,
                        int32_t dest_clip_after_px,
                        unsigned int do_batches)
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
                          dest_dim_prehalving_px - dest_clip_after_px,
                          first_sample_ofs [0],
                          sample_step,
                          src_dim_px,
                          dest_clip_before_px,
                          do_batches,
                          &i);

    /* Check to prevent overruns when the output size is exactly 1 */
    if (dest_dim_prehalving_px > (1U << n_halvings))
    {
        /* Main range */
        precalc_linear_range (array,
                              1 << n_halvings,
                              dest_dim_prehalving_px - (1 << n_halvings),
                              dest_dim_prehalving_px - dest_clip_after_px,
                              first_sample_ofs [1],
                              sample_step,
                              src_dim_px,
                              dest_clip_before_px,
                              do_batches,
                              &i);

        /* Right fringe */
        precalc_linear_range (array,
                              dest_dim_prehalving_px - (1 << n_halvings),
                              dest_dim_prehalving_px,
                              dest_dim_prehalving_px - dest_clip_after_px,
                              first_sample_ofs [2],
                              sample_step,
                              src_dim_px,
                              dest_clip_before_px,
                              do_batches,
                              &i);
    }
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
init_dim (SmolDim *dim, int do_batches)
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
                                dim->clip_before_px,
                                dim->clip_after_px,
                                do_batches);
    }
}

static void
init_horizontal (SmolScaleCtx *scale_ctx)
{
    init_dim (&scale_ctx->hdim,
              scale_ctx->storage_type == SMOL_STORAGE_64BPP ? TRUE : FALSE);
}

static void
init_vertical (SmolScaleCtx *scale_ctx)
{
    init_dim (&scale_ctx->vdim, FALSE);
}

/* ----------------- *
 * Premultiplication *
 * ----------------- */

static SMOL_INLINE void
premul_u_to_p8_128bpp (uint64_t * SMOL_RESTRICT inout,
                       uint8_t alpha)
{
    inout [0] = (((inout [0] + 0x0000000100000001) * ((uint16_t) alpha + 1) - 0x0000000100000001)
                 >> 8) & 0x000000ff000000ff;
    inout [1] = (((inout [1] + 0x0000000100000001) * ((uint16_t) alpha + 1) - 0x0000000100000001)
                 >> 8) & 0x000000ff000000ff;
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
                      uint8_t alpha)
{
    return (((in + 0x0001000100010001) * ((uint16_t) alpha + 1) - 0x0001000100010001)
            >> 8) & 0x00ff00ff00ff00ff;
}

static SMOL_INLINE uint64_t
unpremul_p8_to_u_64bpp (const uint64_t in,
                        uint8_t alpha)
{
    uint64_t in_128bpp [2];
    uint64_t out_128bpp [2];

    in_128bpp [0] = (in & 0x000000ff000000ff);
    in_128bpp [1] = (in & 0x00ff000000ff0000) >> 16;

    unpremul_p8_to_u_128bpp (in_128bpp, out_128bpp, alpha);

    return out_128bpp [0] | (out_128bpp [1] << 16);
}

static SMOL_INLINE void
premul_u_to_p16_128bpp (uint64_t *inout,
                        uint8_t alpha)
{
    inout [0] = inout [0] * alpha;
    inout [1] = inout [1] * alpha;
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

/* --------- *
 * Repacking *
 * --------- */

/* PACK_SHUF_MM256_EPI8_32_TO_128() 
 *
 * Generates a shuffling register for packing 8bpc pixel channels in the
 * provided order. The order (1, 2, 3, 4) is neutral and corresponds to
 *
 * _mm256_set_epi8 (13,12,15,14, 9,8,11,10, 5,4,7,6, 1,0,3,2,
 *                  13,12,15,14, 9,8,11,10, 5,4,7,6, 1,0,3,2);
 */
#define SHUF_ORDER_32_TO_128 0x01000302U
#define SHUF_CH_32_TO_128(n) ((char) (SHUF_ORDER_32_TO_128 >> ((4 - (n)) * 8)))
#define SHUF_QUAD_CH_32_TO_128(q, n) (4 * (q) + SHUF_CH_32_TO_128 (n))
#define SHUF_QUAD_32_TO_128(q, a, b, c, d) \
    SHUF_QUAD_CH_32_TO_128 ((q), (a)), \
    SHUF_QUAD_CH_32_TO_128 ((q), (b)), \
    SHUF_QUAD_CH_32_TO_128 ((q), (c)), \
    SHUF_QUAD_CH_32_TO_128 ((q), (d))
#define PACK_SHUF_EPI8_LANE_32_TO_128(a, b, c, d) \
    SHUF_QUAD_32_TO_128 (3, (a), (b), (c), (d)), \
    SHUF_QUAD_32_TO_128 (2, (a), (b), (c), (d)), \
    SHUF_QUAD_32_TO_128 (1, (a), (b), (c), (d)), \
    SHUF_QUAD_32_TO_128 (0, (a), (b), (c), (d))
#define PACK_SHUF_MM256_EPI8_32_TO_128(a, b, c, d) _mm256_set_epi8 ( \
    PACK_SHUF_EPI8_LANE_32_TO_128 ((a), (b), (c), (d)), \
    PACK_SHUF_EPI8_LANE_32_TO_128 ((a), (b), (c), (d)))

/* PACK_SHUF_MM256_EPI8_32_TO_64()
 *
 * 64bpp version. Packs only once, so fewer contortions required. */
#define SHUF_CH_32_TO_64(n) ((char) (4 - (n)))
#define SHUF_QUAD_CH_32_TO_64(q, n) (4 * (q) + SHUF_CH_32_TO_64 (n))
#define SHUF_QUAD_32_TO_64(q, a, b, c, d) \
    SHUF_QUAD_CH_32_TO_64 ((q), (a)), \
    SHUF_QUAD_CH_32_TO_64 ((q), (b)), \
    SHUF_QUAD_CH_32_TO_64 ((q), (c)), \
    SHUF_QUAD_CH_32_TO_64 ((q), (d))
#define PACK_SHUF_EPI8_LANE_32_TO_64(a, b, c, d) \
    SHUF_QUAD_32_TO_64 (3, (a), (b), (c), (d)), \
    SHUF_QUAD_32_TO_64 (2, (a), (b), (c), (d)), \
    SHUF_QUAD_32_TO_64 (1, (a), (b), (c), (d)), \
    SHUF_QUAD_32_TO_64 (0, (a), (b), (c), (d))
#define PACK_SHUF_MM256_EPI8_32_TO_64(a, b, c, d) _mm256_set_epi8 ( \
    PACK_SHUF_EPI8_LANE_32_TO_64 ((a), (b), (c), (d)), \
    PACK_SHUF_EPI8_LANE_32_TO_64 ((a), (b), (c), (d)))

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

static void
unpack_8x_1234_p8_to_xxxx_p8_64bpp (const uint32_t * SMOL_RESTRICT *in,
                                    uint64_t * SMOL_RESTRICT *out,
                                    uint64_t *out_max,
                                    const __m256i channel_shuf)
{
    const __m256i zero = _mm256_setzero_si256 ();
    const __m256i * SMOL_RESTRICT my_in = (const __m256i * SMOL_RESTRICT) *in;
    __m256i * SMOL_RESTRICT my_out = (__m256i * SMOL_RESTRICT) *out;
    __m256i m0, m1, m2;

    SMOL_ASSUME_ALIGNED (my_out, __m256i * SMOL_RESTRICT);

    while ((ptrdiff_t) (my_out + 2) <= (ptrdiff_t) out_max)
    {
        m0 = _mm256_loadu_si256 (my_in);
        my_in++;

        m0 = _mm256_shuffle_epi8 (m0, channel_shuf);
        m0 = _mm256_permute4x64_epi64 (m0, SMOL_4X2BIT (3, 1, 2, 0));

        m1 = _mm256_unpacklo_epi8 (m0, zero);
        m2 = _mm256_unpackhi_epi8 (m0, zero);

        _mm256_store_si256 (my_out, m1);
        my_out++;
        _mm256_store_si256 (my_out, m2);
        my_out++;
    }

    *out = (uint64_t * SMOL_RESTRICT) my_out;
    *in = (const uint32_t * SMOL_RESTRICT) my_in;
}

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
    const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_64 (1, 3, 2, 4);
    unpack_8x_1234_p8_to_xxxx_p8_64bpp (&src_row, &dest_row, dest_row_max,
                                        channel_shuf);

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
    const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_64 (3, 2, 4, 1);
    unpack_8x_1234_p8_to_xxxx_p8_64bpp (&src_row, &dest_row, dest_row_max,
                                        channel_shuf);

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
    const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_64 (2, 4, 3, 1);
    unpack_8x_1234_p8_to_xxxx_p8_64bpp (&src_row, &dest_row, dest_row_max,
                                        channel_shuf);

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

    return premul_u_to_p8_64bpp (p64, alpha) | alpha;
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

    return premul_u_to_p8_64bpp (p64, alpha) | alpha;
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

    return premul_u_to_p8_64bpp (p64, alpha) | alpha;
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

static void
unpack_8x_xxxx_u_to_123a_p16_128bpp (const uint32_t * SMOL_RESTRICT *in,
                                     uint64_t * SMOL_RESTRICT *out,
                                     uint64_t *out_max,
                                     const __m256i channel_shuf)
{
    const __m256i zero = _mm256_setzero_si256 ();
    const __m256i factor_shuf = _mm256_set_epi8 (
        -1, 12, -1, -1, -1, 12, -1, 12,  -1, 4, -1, -1, -1, 4, -1, 4,
        -1, 12, -1, -1, -1, 12, -1, 12,  -1, 4, -1, -1, -1, 4, -1, 4);
    const __m256i alpha_mul = _mm256_set_epi16 (
        0, 0x100, 0, 0,  0, 0x100, 0, 0,
        0, 0x100, 0, 0,  0, 0x100, 0, 0);
    const __m256i alpha_add = _mm256_set_epi16 (
        0, 0x80, 0, 0,  0, 0x80, 0, 0,
        0, 0x80, 0, 0,  0, 0x80, 0, 0);
    const __m256i * SMOL_RESTRICT my_in = (const __m256i * SMOL_RESTRICT) *in;
    __m256i * SMOL_RESTRICT my_out = (__m256i * SMOL_RESTRICT) *out;
    __m256i m0, m1, m2, m3, m4, m5, m6;
    __m256i fact1, fact2;

    SMOL_ASSUME_ALIGNED (my_out, __m256i * SMOL_RESTRICT);

    while ((ptrdiff_t) (my_out + 4) <= (ptrdiff_t) out_max)
    {
        m0 = _mm256_loadu_si256 (my_in);
        my_in++;

        m0 = _mm256_shuffle_epi8 (m0, channel_shuf);
        m0 = _mm256_permute4x64_epi64 (m0, SMOL_4X2BIT (3, 1, 2, 0));

        m1 = _mm256_unpacklo_epi8 (m0, zero);
        m2 = _mm256_unpackhi_epi8 (m0, zero);

        fact1 = _mm256_shuffle_epi8 (m1, factor_shuf);
        fact2 = _mm256_shuffle_epi8 (m2, factor_shuf);

        fact1 = _mm256_or_si256 (fact1, alpha_mul);
        fact2 = _mm256_or_si256 (fact2, alpha_mul);

        m1 = _mm256_mullo_epi16 (m1, fact1);
        m2 = _mm256_mullo_epi16 (m2, fact2);

        m1 = _mm256_add_epi16 (m1, alpha_add);
        m2 = _mm256_add_epi16 (m2, alpha_add);

        m1 = _mm256_permute4x64_epi64 (m1, SMOL_4X2BIT (3, 1, 2, 0));
        m2 = _mm256_permute4x64_epi64 (m2, SMOL_4X2BIT (3, 1, 2, 0));

        m3 = _mm256_unpacklo_epi16 (m1, zero);
        m4 = _mm256_unpackhi_epi16 (m1, zero);
        m5 = _mm256_unpacklo_epi16 (m2, zero);
        m6 = _mm256_unpackhi_epi16 (m2, zero);

        _mm256_store_si256 (my_out, m3);
        my_out++;
        _mm256_store_si256 (my_out, m4);
        my_out++;
        _mm256_store_si256 (my_out, m5);
        my_out++;
        _mm256_store_si256 (my_out, m6);
        my_out++;
    }

    *out = (uint64_t * SMOL_RESTRICT) my_out;
    *in = (const uint32_t * SMOL_RESTRICT) my_in;
}

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

static SMOL_INLINE void
unpack_pixel_a234_u_to_234a_p8_128bpp (uint32_t p,
                                       uint64_t *out)
{
    uint64_t p64 = (((uint64_t) p & 0x00ff00ff) << 32) | (((uint64_t) p & 0x0000ff00) << 8);
    uint8_t alpha = p >> 24;

    p64 = premul_u_to_p8_64bpp (p64, alpha) | alpha;
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
unpack_pixel_a234_u_to_234a_p16_128bpp (uint32_t p,
                                        uint64_t *out)
{
    uint64_t p64 = p;
    uint64_t alpha = p >> 24;

    out [0] = (((((p64 & 0x00ff0000) << 16) | ((p64 & 0x0000ff00) >> 8)) * alpha));
    out [1] = (((((p64 & 0x000000ff) << 32) * alpha))) | (alpha << 8) | 0x80;
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     2341, 128, 64, PREMUL16,     COMPRESSED) {
    const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_128 (2, 3, 4, 1);
    unpack_8x_xxxx_u_to_123a_p16_128bpp (&src_row, &dest_row, dest_row_max,
                                         channel_shuf);

    while (dest_row != dest_row_max)
    {
        unpack_pixel_a234_u_to_234a_p16_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_123a_u_to_123a_p8_128bpp (uint32_t p,
                                       uint64_t *out)
{
    uint64_t p64 = (((uint64_t) p & 0xff00ff00) << 24) | (p & 0x00ff0000);
    uint8_t alpha = p & 0xff;

    p64 = premul_u_to_p8_64bpp (p64, alpha) | ((uint64_t) alpha);
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
unpack_pixel_123a_u_to_123a_p16_128bpp (uint32_t p,
                                        uint64_t *out)
{
    uint64_t p64 = p;
    uint64_t alpha = p & 0xff;

    out [0] = (((((p64 & 0xff000000) << 8) | ((p64 & 0x00ff0000) >> 16)) * alpha));
    out [1] = (((((p64 & 0x0000ff00) << 24) * alpha))) | (alpha << 8) | 0x80;
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     1234, 128, 64, PREMUL16,     COMPRESSED) {
    const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_128 (1, 2, 3, 4);
    unpack_8x_xxxx_u_to_123a_p16_128bpp (&src_row, &dest_row, dest_row_max,
                                         channel_shuf);

    while (dest_row != dest_row_max)
    {
        unpack_pixel_123a_u_to_123a_p16_128bpp (*(src_row++), dest_row);
        dest_row += 2;
    }
} SMOL_REPACK_ROW_DEF_END

/* ---------------------- *
 * Repacking: 64 -> 24/32 *
 * ---------------------- */

static void
pack_8x_1234_p8_to_xxxx_p8_64bpp (const uint64_t * SMOL_RESTRICT *in,
                                  uint32_t * SMOL_RESTRICT *out,
                                  uint32_t * out_max,
                                  const __m256i channel_shuf)
{
    const __m256i * SMOL_RESTRICT my_in = (const __m256i * SMOL_RESTRICT) *in;
    __m256i * SMOL_RESTRICT my_out = (__m256i * SMOL_RESTRICT) *out;
    __m256i m0, m1;

    SMOL_ASSUME_ALIGNED (my_in, __m256i * SMOL_RESTRICT);

    while ((ptrdiff_t) (my_out + 1) <= (ptrdiff_t) out_max)
    {
        /* Load inputs */

        m0 = _mm256_stream_load_si256 (my_in);
        my_in++;
        m1 = _mm256_stream_load_si256 (my_in);
        my_in++;

        /* Pack and store */

        m0 = _mm256_packus_epi16 (m0, m1);
        m0 = _mm256_shuffle_epi8 (m0, channel_shuf);
        m0 = _mm256_permute4x64_epi64 (m0, SMOL_4X2BIT (3, 1, 2, 0));

        _mm256_storeu_si256 (my_out, m0);
        my_out++;
    }

    *out = (uint32_t * SMOL_RESTRICT) my_out;
    *in = (const uint64_t * SMOL_RESTRICT) my_in;
}

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
    const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_64 (1, 3, 2, 4);
    pack_8x_1234_p8_to_xxxx_p8_64bpp (&src_row, &dest_row, dest_row_max,
                                      channel_shuf);
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
        const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_64 ((a), (b), (c), (d)); \
        pack_8x_1234_p8_to_xxxx_p8_64bpp (&src_row, &dest_row, dest_row_max, \
                                          channel_shuf); \
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

static void
pack_8x_123a_p16_to_xxxx_u_128bpp (const uint64_t * SMOL_RESTRICT *in,
                                   uint32_t * SMOL_RESTRICT *out,
                                   uint32_t * out_max,
                                   const __m256i channel_shuf)
{
#define ALPHA_MUL (1 << (INVERTED_DIV_SHIFT_P16 - 8))
#define ALPHA_MASK SMOL_8X1BIT (0, 1, 0, 0, 0, 1, 0, 0)

    const __m256i ones = _mm256_set_epi32 (
        ALPHA_MUL, ALPHA_MUL, ALPHA_MUL, ALPHA_MUL,
        ALPHA_MUL, ALPHA_MUL, ALPHA_MUL, ALPHA_MUL);
    const __m256i alpha_clean_mask = _mm256_set_epi32 (
        0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
        0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff);
    const __m256i * SMOL_RESTRICT my_in = (const __m256i * SMOL_RESTRICT) *in;
    __m256i * SMOL_RESTRICT my_out = (__m256i * SMOL_RESTRICT) *out;
    __m256i m0, m1, m2, m3, m4, m5, m6, m7, m8;

    SMOL_ASSUME_ALIGNED (my_in, __m256i * SMOL_RESTRICT);

    while ((ptrdiff_t) (my_out + 1) <= (ptrdiff_t) out_max)
    {
        /* Load inputs */

        m0 = _mm256_stream_load_si256 (my_in);
        my_in++;
        m1 = _mm256_stream_load_si256 (my_in);
        my_in++;
        m2 = _mm256_stream_load_si256 (my_in);
        my_in++;
        m3 = _mm256_stream_load_si256 (my_in);
        my_in++;

        /* Load alpha factors */

        m4 = _mm256_slli_si256 (m0, 4);
        m6 = _mm256_srli_si256 (m3, 4);
        m5 = _mm256_blend_epi32 (m4, m1, ALPHA_MASK);
        m7 = _mm256_blend_epi32 (m6, m2, ALPHA_MASK);
        m7 = _mm256_srli_si256 (m7, 4);

        m4 = _mm256_blend_epi32 (m5, m7, SMOL_8X1BIT (0, 0, 1, 1, 0, 0, 1, 1));
        m4 = _mm256_srli_epi32 (m4, 8);
        m4 = _mm256_and_si256 (m4, alpha_clean_mask);
        m4 = _mm256_i32gather_epi32 ((const void *) _smol_inv_div_p16_lut, m4, 4);

        /* 2 pixels times 4 */

        m5 = _mm256_shuffle_epi32 (m4, SMOL_4X2BIT (3, 3, 3, 3));
        m6 = _mm256_shuffle_epi32 (m4, SMOL_4X2BIT (2, 2, 2, 2));
        m7 = _mm256_shuffle_epi32 (m4, SMOL_4X2BIT (1, 1, 1, 1));
        m8 = _mm256_shuffle_epi32 (m4, SMOL_4X2BIT (0, 0, 0, 0));

        m5 = _mm256_blend_epi32 (m5, ones, ALPHA_MASK);
        m6 = _mm256_blend_epi32 (m6, ones, ALPHA_MASK);
        m7 = _mm256_blend_epi32 (m7, ones, ALPHA_MASK);
        m8 = _mm256_blend_epi32 (m8, ones, ALPHA_MASK);

        m5 = _mm256_mullo_epi32 (m5, m0);
        m6 = _mm256_mullo_epi32 (m6, m1);
        m7 = _mm256_mullo_epi32 (m7, m2);
        m8 = _mm256_mullo_epi32 (m8, m3);

        m5 = _mm256_srli_epi32 (m5, INVERTED_DIV_SHIFT_P16);
        m6 = _mm256_srli_epi32 (m6, INVERTED_DIV_SHIFT_P16);
        m7 = _mm256_srli_epi32 (m7, INVERTED_DIV_SHIFT_P16);
        m8 = _mm256_srli_epi32 (m8, INVERTED_DIV_SHIFT_P16);

        /* Pack and store */

        m0 = _mm256_packus_epi32 (m5, m6);
        m1 = _mm256_packus_epi32 (m7, m8);
        m0 = _mm256_packus_epi16 (m0, m1);

        m0 = _mm256_shuffle_epi8 (m0, channel_shuf);
        m0 = _mm256_permute4x64_epi64 (m0, SMOL_4X2BIT (3, 1, 2, 0));
        m0 = _mm256_shuffle_epi32 (m0, SMOL_4X2BIT (3, 1, 2, 0));

        _mm256_storeu_si256 (my_out, m0);
        my_out += 1;
    }

    *out = (uint32_t * SMOL_RESTRICT) my_out;
    *in = (const uint64_t * SMOL_RESTRICT) my_in;

#undef ALPHA_MUL
#undef ALPHA_MASK
}

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL8,       COMPRESSED,
                     123,   24,  8, PREMUL8,       COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = *src_row >> 32;
        *(dest_row++) = *(src_row++);
        *(dest_row++) = *(src_row++) >> 32;
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

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL16,      COMPRESSED,
                     123,   24,  8, UNASSOCIATED,  COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        uint64_t t [2];
        uint8_t alpha = src_row [1];
        unpremul_p16_to_u_128bpp (src_row, t, alpha);
        t [1] = (t [1] & 0xffffffff00000000ULL) | alpha;
        *(dest_row++) = t [0] >> 32;
        *(dest_row++) = t [0];
        *(dest_row++) = t [1] >> 32;
        src_row += 2;
    }
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

#define DEF_REPACK_FROM_1234_128BPP_TO_32BPP(a, b, c, d) \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL8,       COMPRESSED, \
                         a##b##c##d,  32, 32, PREMUL8,       COMPRESSED) { \
        while (dest_row != dest_row_max) \
        { \
            *(dest_row++) = PACK_FROM_1234_128BPP (src_row, a, b, c, d); \
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
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL16,      COMPRESSED, \
                         a##b##c##d,  32, 32, UNASSOCIATED,  COMPRESSED) { \
        const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_128 ((a), (b), (c), (d)); \
        pack_8x_123a_p16_to_xxxx_u_128bpp (&src_row, &dest_row, dest_row_max, \
                                           channel_shuf);               \
        while (dest_row != dest_row_max) \
        { \
            uint64_t t [2]; \
            uint8_t alpha = src_row [1] >> 8; \
            unpremul_p16_to_u_128bpp (src_row, t, alpha); \
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

#define LERP_SIMD256_EPI32(a, b, f) \
    _mm256_add_epi32 ( \
        _mm256_srli_epi32 ( \
            _mm256_mullo_epi32 ( \
                _mm256_sub_epi32 ((a), (b)), (f)), 8), (b))

#define LERP_SIMD128_EPI32(a, b, f) \
    _mm_add_epi32 ( \
        _mm_srli_epi32 ( \
            _mm_mullo_epi32 ( \
                _mm_sub_epi32 ((a), (b)), (f)), 8), (b))

#define LERP_SIMD256_EPI32_AND_MASK(a, b, f, mask) \
    _mm256_and_si256 (LERP_SIMD256_EPI32 ((a), (b), (f)), (mask))

#define LERP_SIMD128_EPI32_AND_MASK(a, b, f, mask) \
    _mm_and_si128 (LERP_SIMD128_EPI32 ((a), (b), (f)), (mask))

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
    const uint64_t *pp_end;
    const uint64_t * SMOL_RESTRICT pp = *parts_in;

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
    const uint64_t *pp_end;
    const uint64_t * SMOL_RESTRICT pp = *parts_in;

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

    /* Average the inputs */
    a = ((accum & 0x0000ffff0000ffffULL) * multiplier
         + (SMOL_BOXES_MULTIPLIER / 2) + ((SMOL_BOXES_MULTIPLIER / 2) << 32)) / SMOL_BOXES_MULTIPLIER;
    b = (((accum & 0xffff0000ffff0000ULL) >> 16) * multiplier
         + (SMOL_BOXES_MULTIPLIER / 2) + ((SMOL_BOXES_MULTIPLIER / 2) << 32)) / SMOL_BOXES_MULTIPLIER;

    /* Return pixel */
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
                        uint64_t ** SMOL_RESTRICT row_parts_out)
{
    *(*row_parts_out)++ = scale_128bpp_half (accum [0], multiplier);
    *(*row_parts_out)++ = scale_128bpp_half (accum [1], multiplier);
}

static void
add_parts (const uint64_t * SMOL_RESTRICT parts_in,
           uint64_t * SMOL_RESTRICT parts_acc_out,
           uint32_t n)
{
    const uint64_t *parts_in_max = parts_in + n;

    SMOL_ASSUME_ALIGNED (parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (parts_acc_out, uint64_t *);

    while (parts_in + 4 <= parts_in_max)
    {
        __m256i m0, m1;

        m0 = _mm256_stream_load_si256 ((const __m256i *) parts_in);
        parts_in += 4;
        m1 = _mm256_load_si256 ((__m256i *) parts_acc_out);

        m0 = _mm256_add_epi32 (m0, m1);
        _mm256_store_si256 ((__m256i *) parts_acc_out, m0);
        parts_acc_out += 4;
    }

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

#define CONTROL_4X2BIT_1_0_3_2 (SMOL_4X2BIT (1, 0, 3, 2))
#define CONTROL_4X2BIT_3_1_2_0 (SMOL_4X2BIT (3, 1, 2, 0))
#define CONTROL_8X1BIT_1_1_0_0_1_1_0_0 (SMOL_8X1BIT (1, 1, 0, 0, 1, 1, 0, 0))

static SMOL_INLINE void
hadd_pixels_16x_to_8x_64bpp (__m256i i0, __m256i i1, __m256i i2, __m256i i3,
                             __m256i * SMOL_RESTRICT o0, __m256i * SMOL_RESTRICT o1)
{
    __m256i t0, t1, t2, t3;

    t0 = _mm256_shuffle_epi32 (i0, CONTROL_4X2BIT_1_0_3_2);
    t1 = _mm256_shuffle_epi32 (i1, CONTROL_4X2BIT_1_0_3_2);
    t2 = _mm256_shuffle_epi32 (i2, CONTROL_4X2BIT_1_0_3_2);
    t3 = _mm256_shuffle_epi32 (i3, CONTROL_4X2BIT_1_0_3_2);

    t0 = _mm256_add_epi16 (t0, i0);
    t1 = _mm256_add_epi16 (t1, i1);
    t2 = _mm256_add_epi16 (t2, i2);
    t3 = _mm256_add_epi16 (t3, i3);

    t0 = _mm256_blend_epi32 (t0, t1, CONTROL_8X1BIT_1_1_0_0_1_1_0_0);
    t1 = _mm256_blend_epi32 (t2, t3, CONTROL_8X1BIT_1_1_0_0_1_1_0_0);

    t0 = _mm256_permute4x64_epi64 (t0, CONTROL_4X2BIT_3_1_2_0);
    t1 = _mm256_permute4x64_epi64 (t1, CONTROL_4X2BIT_3_1_2_0);

    *o0 = t0;
    *o1 = t1;
}

static SMOL_INLINE void
hadd_pixels_8x_to_4x_64bpp (__m256i i0, __m256i i1, __m256i * SMOL_RESTRICT o0)
{
    __m256i t0, t1;

    t0 = _mm256_shuffle_epi32 (i0, CONTROL_4X2BIT_1_0_3_2);
    t1 = _mm256_shuffle_epi32 (i1, CONTROL_4X2BIT_1_0_3_2);

    t0 = _mm256_add_epi16 (t0, i0);
    t1 = _mm256_add_epi16 (t1, i1);

    t0 = _mm256_blend_epi32 (t0, t1, CONTROL_8X1BIT_1_1_0_0_1_1_0_0);

    t0 = _mm256_permute4x64_epi64 (t0, CONTROL_4X2BIT_3_1_2_0);

    *o0 = t0;
}

static SMOL_INLINE void
interp_horizontal_bilinear_batch_64bpp (const uint64_t * SMOL_RESTRICT row_parts_in,
                                        const uint16_t * SMOL_RESTRICT precalc_x,
                                        __m256i * SMOL_RESTRICT o0,
                                        __m256i * SMOL_RESTRICT o1,
                                        __m256i * SMOL_RESTRICT o2,
                                        __m256i * SMOL_RESTRICT o3)
{
    const __m256i mask = _mm256_set_epi16 (0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff,
                                           0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff);
    const __m256i shuf_0 = _mm256_set_epi8 (3, 2, 3, 2, 3, 2, 3, 2, 1, 0, 1, 0, 1, 0, 1, 0,
                                            3, 2, 3, 2, 3, 2, 3, 2, 1, 0, 1, 0, 1, 0, 1, 0);
    const __m256i shuf_1 = _mm256_set_epi8 (7, 6, 7, 6, 7, 6, 7, 6, 5, 4, 5, 4, 5, 4, 5, 4,
                                            7, 6, 7, 6, 7, 6, 7, 6, 5, 4, 5, 4, 5, 4, 5, 4);
    const __m256i shuf_2 = _mm256_set_epi8 (11, 10, 11, 10, 11, 10, 11, 10, 9, 8, 9, 8, 9, 8, 9, 8,
                                            11, 10, 11, 10, 11, 10, 11, 10, 9, 8, 9, 8, 9, 8, 9, 8);
    const __m256i shuf_3 = _mm256_set_epi8 (15, 14, 15, 14, 15, 14, 15, 14, 13, 12, 13, 12, 13, 12, 13, 12,
                                            15, 14, 15, 14, 15, 14, 15, 14, 13, 12, 13, 12, 13, 12, 13, 12);
    __m256i m0, m1, m2, m3;
    __m256i f0, f1, f2, f3;
    __m256i q00, q10, q20, q30, q40, q50, q60, q70;
    __m256i q01, q11, q21, q31, q41, q51, q61, q71;
    __m256i p00, p01, p10, p11, p20, p21, p30, p31;
    __m256i f;

    /* Fetch pixel pairs to interpolate between, two pairs per ymm register.
     * This looks clumsy, but it's a lot faster than using _mm256_i32gather_epi64(),
     * as benchmarked on both Haswell and Tiger Lake. */

    q00 = _mm256_inserti128_si256 (_mm256_castsi128_si256 (
                                       _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [0]))),
                                   _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [1])), 1);
    q10 = _mm256_inserti128_si256 (_mm256_castsi128_si256 (
                                       _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [2]))),
                                   _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [3])), 1);
    q20 = _mm256_inserti128_si256 (_mm256_castsi128_si256 (
                                       _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [4]))),
                                   _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [5])), 1);
    q30 = _mm256_inserti128_si256 (_mm256_castsi128_si256 (
                                       _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [6]))),
                                   _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [7])), 1);

    q40 = _mm256_inserti128_si256 (_mm256_castsi128_si256 (
                                       _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [8]))),
                                   _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [9])), 1);
    q50 = _mm256_inserti128_si256 (_mm256_castsi128_si256 (
                                       _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [10]))),
                                   _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [11])), 1);
    q60 = _mm256_inserti128_si256 (_mm256_castsi128_si256 (
                                       _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [12]))),
                                   _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [13])), 1);
    q70 = _mm256_inserti128_si256 (_mm256_castsi128_si256 (
                                       _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [14]))),
                                   _mm_loadu_si128 ((const __m128i *) (row_parts_in + precalc_x [15])), 1);

    f = _mm256_load_si256 ((const __m256i *) (precalc_x + 16));  /* Factors */

    /* 0123 -> 0x2x, 1x3x. 4567 -> x4x6, x5x7. Etc. */

    q01 = _mm256_shuffle_epi32 (q00, CONTROL_4X2BIT_1_0_3_2);
    q11 = _mm256_shuffle_epi32 (q10, CONTROL_4X2BIT_1_0_3_2);
    q21 = _mm256_shuffle_epi32 (q20, CONTROL_4X2BIT_1_0_3_2);
    q31 = _mm256_shuffle_epi32 (q30, CONTROL_4X2BIT_1_0_3_2);
    q41 = _mm256_shuffle_epi32 (q40, CONTROL_4X2BIT_1_0_3_2);
    q51 = _mm256_shuffle_epi32 (q50, CONTROL_4X2BIT_1_0_3_2);
    q61 = _mm256_shuffle_epi32 (q60, CONTROL_4X2BIT_1_0_3_2);
    q71 = _mm256_shuffle_epi32 (q70, CONTROL_4X2BIT_1_0_3_2);

    /* 0x2x, x4x6 -> 0426. 1x3x, x5x7 -> 1537. Etc. */

    p00 = _mm256_blend_epi32 (q00, q11, CONTROL_8X1BIT_1_1_0_0_1_1_0_0);
    p10 = _mm256_blend_epi32 (q20, q31, CONTROL_8X1BIT_1_1_0_0_1_1_0_0);
    p20 = _mm256_blend_epi32 (q40, q51, CONTROL_8X1BIT_1_1_0_0_1_1_0_0);
    p30 = _mm256_blend_epi32 (q60, q71, CONTROL_8X1BIT_1_1_0_0_1_1_0_0);

    p01 = _mm256_blend_epi32 (q01, q10, CONTROL_8X1BIT_1_1_0_0_1_1_0_0);
    p11 = _mm256_blend_epi32 (q21, q30, CONTROL_8X1BIT_1_1_0_0_1_1_0_0);
    p21 = _mm256_blend_epi32 (q41, q50, CONTROL_8X1BIT_1_1_0_0_1_1_0_0);
    p31 = _mm256_blend_epi32 (q61, q70, CONTROL_8X1BIT_1_1_0_0_1_1_0_0);

    /* Interpolation. 0426 vs 1537. Etc. */

    m0 = _mm256_sub_epi16 (p00, p01);
    m1 = _mm256_sub_epi16 (p10, p11);
    m2 = _mm256_sub_epi16 (p20, p21);
    m3 = _mm256_sub_epi16 (p30, p31);

    f0 = _mm256_shuffle_epi8 (f, shuf_0);
    f1 = _mm256_shuffle_epi8 (f, shuf_1);
    f2 = _mm256_shuffle_epi8 (f, shuf_2);
    f3 = _mm256_shuffle_epi8 (f, shuf_3);

    m0 = _mm256_mullo_epi16 (m0, f0);
    m1 = _mm256_mullo_epi16 (m1, f1);
    m2 = _mm256_mullo_epi16 (m2, f2);
    m3 = _mm256_mullo_epi16 (m3, f3);

    m0 = _mm256_srli_epi16 (m0, 8);
    m1 = _mm256_srli_epi16 (m1, 8);
    m2 = _mm256_srli_epi16 (m2, 8);
    m3 = _mm256_srli_epi16 (m3, 8);

    m0 = _mm256_add_epi16 (m0, p01);
    m1 = _mm256_add_epi16 (m1, p11);
    m2 = _mm256_add_epi16 (m2, p21);
    m3 = _mm256_add_epi16 (m3, p31);

    m0 = _mm256_and_si256 (m0, mask);
    m1 = _mm256_and_si256 (m1, mask);
    m2 = _mm256_and_si256 (m2, mask);
    m3 = _mm256_and_si256 (m3, mask);

    /* [0426/1537] -> [0246/1357]. Etc. */

    *o0 = _mm256_permute4x64_epi64 (m0, CONTROL_4X2BIT_3_1_2_0);
    *o1 = _mm256_permute4x64_epi64 (m1, CONTROL_4X2BIT_3_1_2_0);
    *o2 = _mm256_permute4x64_epi64 (m2, CONTROL_4X2BIT_3_1_2_0);
    *o3 = _mm256_permute4x64_epi64 (m3, CONTROL_4X2BIT_3_1_2_0);
}

static void
interp_horizontal_bilinear_batch_to_4x_64bpp (const uint64_t * SMOL_RESTRICT row_parts_in,
                                              const uint16_t * SMOL_RESTRICT precalc_x,
                                              __m256i * SMOL_RESTRICT o0)
{
    __m256i m0, m1, m2, m3, s0, s1;

    interp_horizontal_bilinear_batch_64bpp (row_parts_in, precalc_x, &m0, &m1, &m2, &m3);
    hadd_pixels_16x_to_8x_64bpp (m0, m1, m2, m3, &s0, &s1);
    hadd_pixels_8x_to_4x_64bpp (s0, s1, o0);
}

static void
interp_horizontal_bilinear_4x_batch_to_4x_64bpp (const uint64_t * SMOL_RESTRICT row_parts_in,
                                                 const uint16_t * SMOL_RESTRICT precalc_x,
                                                 __m256i * SMOL_RESTRICT o0)
{
    __m256i t0, t1, t2, t3;

    interp_horizontal_bilinear_batch_to_4x_64bpp (row_parts_in, precalc_x, &t0);
    interp_horizontal_bilinear_batch_to_4x_64bpp (row_parts_in, precalc_x + 32, &t1);
    interp_horizontal_bilinear_batch_to_4x_64bpp (row_parts_in, precalc_x + 64, &t2);
    interp_horizontal_bilinear_batch_to_4x_64bpp (row_parts_in, precalc_x + 96, &t3);

    hadd_pixels_16x_to_8x_64bpp (t0, t1, t2, t3, &t0, &t1);
    hadd_pixels_8x_to_4x_64bpp (t0, t1, o0);
}

/* Note that precalc_x must point to offsets and factors interleaved one by one, i.e.
 * offset - factor - offset - factor, and not 16x as with the batch function. */
static SMOL_INLINE void
interp_horizontal_bilinear_epilogue_64bpp (const uint64_t * SMOL_RESTRICT row_parts_in,
                                           uint64_t * SMOL_RESTRICT row_parts_out,
                                           uint64_t * SMOL_RESTRICT row_parts_out_max,
                                           const uint16_t * SMOL_RESTRICT precalc_x,
                                           int n_halvings)
{
    while (row_parts_out != row_parts_out_max)
    {
        uint64_t accum = 0;
        int i;

        for (i = 0; i < (1 << (n_halvings)); i++)
        {
            uint64_t p, q;
            uint64_t F;

            p = *(row_parts_in + (*precalc_x));
            q = *(row_parts_in + (*precalc_x) + 1);
            precalc_x++;
            F = *(precalc_x++);

            accum += ((((p - q) * F) >> 8) + q) & 0x00ff00ff00ff00ffULL;
        }

        *(row_parts_out++) = ((accum) >> (n_halvings)) & 0x00ff00ff00ff00ffULL;
    }
}

static void
interp_horizontal_bilinear_0h_64bpp (const SmolScaleCtx *scale_ctx,
                                     const uint64_t * SMOL_RESTRICT row_parts_in,
                                     uint64_t * SMOL_RESTRICT row_parts_out)
{
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc;
    uint64_t * SMOL_RESTRICT row_parts_out_max = row_parts_out + scale_ctx->hdim.placement_size_px;

    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (precalc_x, const uint16_t * SMOL_RESTRICT);

    while (row_parts_out + 16 <= row_parts_out_max)
    {
        __m256i m0, m1, m2, m3;

        interp_horizontal_bilinear_batch_64bpp (row_parts_in, precalc_x, &m0, &m1, &m2, &m3);

        _mm256_store_si256 ((__m256i *) row_parts_out + 0, m0);
        _mm256_store_si256 ((__m256i *) row_parts_out + 1, m1);
        _mm256_store_si256 ((__m256i *) row_parts_out + 2, m2);
        _mm256_store_si256 ((__m256i *) row_parts_out + 3, m3);

        row_parts_out += 16;
        precalc_x += 32;
    }

    interp_horizontal_bilinear_epilogue_64bpp (row_parts_in, row_parts_out, row_parts_out_max, precalc_x, 0);
}

static void
interp_horizontal_bilinear_1h_64bpp (const SmolScaleCtx *scale_ctx,
                                     const uint64_t * SMOL_RESTRICT row_parts_in,
                                     uint64_t * SMOL_RESTRICT row_parts_out)
{
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc;
    uint64_t * SMOL_RESTRICT row_parts_out_max = row_parts_out + scale_ctx->hdim.placement_size_px;

    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (precalc_x, const uint16_t * SMOL_RESTRICT);

    while (row_parts_out + 8 <= row_parts_out_max)
    {
        __m256i m0, m1, m2, m3, s0, s1;

        interp_horizontal_bilinear_batch_64bpp (row_parts_in, precalc_x, &m0, &m1, &m2, &m3);
        hadd_pixels_16x_to_8x_64bpp (m0, m1, m2, m3, &s0, &s1);

        s0 = _mm256_srli_epi16 (s0, 1);
        s1 = _mm256_srli_epi16 (s1, 1);

        _mm256_store_si256 ((__m256i *) row_parts_out, s0);
        _mm256_store_si256 ((__m256i *) row_parts_out + 1, s1);

        row_parts_out += 8;
        precalc_x += 32;
    }

    interp_horizontal_bilinear_epilogue_64bpp (row_parts_in, row_parts_out, row_parts_out_max, precalc_x, 1);
}

static void
interp_horizontal_bilinear_2h_64bpp (const SmolScaleCtx *scale_ctx,
                                     const uint64_t * SMOL_RESTRICT row_parts_in,
                                     uint64_t * SMOL_RESTRICT row_parts_out)
{
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc;
    uint64_t * SMOL_RESTRICT row_parts_out_max = row_parts_out + scale_ctx->hdim.placement_size_px;

    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (precalc_x, const uint16_t * SMOL_RESTRICT);

    while (row_parts_out + 4 <= row_parts_out_max)
    {
        __m256i t;

        interp_horizontal_bilinear_batch_to_4x_64bpp (row_parts_in, precalc_x, &t);
        t = _mm256_srli_epi16 (t, 2);
        _mm256_store_si256 ((__m256i *) row_parts_out, t);

        row_parts_out += 4;
        precalc_x += 32;
    }

    interp_horizontal_bilinear_epilogue_64bpp (row_parts_in, row_parts_out, row_parts_out_max, precalc_x, 2);
}

static void
interp_horizontal_bilinear_3h_64bpp (const SmolScaleCtx *scale_ctx,
                                     const uint64_t * SMOL_RESTRICT row_parts_in,
                                     uint64_t * SMOL_RESTRICT row_parts_out)
{
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc;
    uint64_t * SMOL_RESTRICT row_parts_out_max = row_parts_out + scale_ctx->hdim.placement_size_px;

    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (precalc_x, const uint16_t * SMOL_RESTRICT);

    while (row_parts_out + 4 <= row_parts_out_max)
    {
        __m256i s0, s1;

        interp_horizontal_bilinear_batch_to_4x_64bpp (row_parts_in, precalc_x, &s0);
        interp_horizontal_bilinear_batch_to_4x_64bpp (row_parts_in, precalc_x + 32, &s1);

        hadd_pixels_8x_to_4x_64bpp (s0, s1, &s0);
        s0 = _mm256_srli_epi16 (s0, 3);
        _mm256_store_si256 ((__m256i *) row_parts_out, s0);

        row_parts_out += 4;
        precalc_x += 64;
    }

    interp_horizontal_bilinear_epilogue_64bpp (row_parts_in, row_parts_out, row_parts_out_max, precalc_x, 3);
}

static void
interp_horizontal_bilinear_4h_64bpp (const SmolScaleCtx *scale_ctx,
                                     const uint64_t * SMOL_RESTRICT row_parts_in,
                                     uint64_t * SMOL_RESTRICT row_parts_out)
{
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc;
    uint64_t * SMOL_RESTRICT row_parts_out_max = row_parts_out + scale_ctx->hdim.placement_size_px;

    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (precalc_x, const uint16_t * SMOL_RESTRICT);

    while (row_parts_out + 4 <= row_parts_out_max)
    {
        __m256i t0;

        interp_horizontal_bilinear_4x_batch_to_4x_64bpp (row_parts_in, precalc_x, &t0);
        t0 = _mm256_srli_epi16 (t0, 4);
        _mm256_store_si256 ((__m256i *) row_parts_out, t0);

        row_parts_out += 4;
        precalc_x += 128;
    }

    interp_horizontal_bilinear_epilogue_64bpp (row_parts_in, row_parts_out, row_parts_out_max, precalc_x, 4);
}

static void
interp_horizontal_bilinear_5h_64bpp (const SmolScaleCtx *scale_ctx,
                                     const uint64_t * SMOL_RESTRICT row_parts_in,
                                     uint64_t * SMOL_RESTRICT row_parts_out)
{
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc;
    uint64_t * SMOL_RESTRICT row_parts_out_max = row_parts_out + scale_ctx->hdim.placement_size_px;

    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (precalc_x, const uint16_t * SMOL_RESTRICT);

    while (row_parts_out + 4 <= row_parts_out_max)
    {
        __m256i t0, t1;

        interp_horizontal_bilinear_4x_batch_to_4x_64bpp (row_parts_in, precalc_x, &t0);
        interp_horizontal_bilinear_4x_batch_to_4x_64bpp (row_parts_in, precalc_x + 128, &t1);

        hadd_pixels_8x_to_4x_64bpp (t0, t1, &t0);
        t0 = _mm256_srli_epi16 (t0, 5);
        _mm256_store_si256 ((__m256i *) row_parts_out, t0);

        row_parts_out += 4;
        precalc_x += 256;
    }

    interp_horizontal_bilinear_epilogue_64bpp (row_parts_in, row_parts_out, row_parts_out_max, precalc_x, 5);
}

static void
interp_horizontal_bilinear_6h_64bpp (const SmolScaleCtx *scale_ctx,
                                     const uint64_t * SMOL_RESTRICT row_parts_in,
                                     uint64_t * SMOL_RESTRICT row_parts_out)
{
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc;
    uint64_t * SMOL_RESTRICT row_parts_out_max = row_parts_out + scale_ctx->hdim.placement_size_px;

    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t * SMOL_RESTRICT);
    SMOL_ASSUME_ALIGNED (precalc_x, const uint16_t * SMOL_RESTRICT);

    while (row_parts_out + 4 <= row_parts_out_max)
    {
        __m256i t0, t1, t2, t3;

        interp_horizontal_bilinear_4x_batch_to_4x_64bpp (row_parts_in, precalc_x, &t0);
        interp_horizontal_bilinear_4x_batch_to_4x_64bpp (row_parts_in, precalc_x + 128, &t1);
        interp_horizontal_bilinear_4x_batch_to_4x_64bpp (row_parts_in, precalc_x + 256, &t2);
        interp_horizontal_bilinear_4x_batch_to_4x_64bpp (row_parts_in, precalc_x + 384, &t3);

        hadd_pixels_16x_to_8x_64bpp (t0, t1, t2, t3, &t0, &t1);
        hadd_pixels_8x_to_4x_64bpp (t0, t1, &t0);

        t0 = _mm256_srli_epi16 (t0, 6);
        _mm256_store_si256 ((__m256i *) row_parts_out, t0);

        row_parts_out += 4;
        precalc_x += 512;
    }

    interp_horizontal_bilinear_epilogue_64bpp (row_parts_in, row_parts_out, row_parts_out_max, precalc_x, 6);
}

static void
interp_horizontal_bilinear_0h_128bpp (const SmolScaleCtx *scale_ctx,
                                      const uint64_t * SMOL_RESTRICT row_parts_in,
                                      uint64_t * SMOL_RESTRICT row_parts_out)
{
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc;
    uint64_t * SMOL_RESTRICT row_parts_out_max = row_parts_out + scale_ctx->hdim.placement_size_px * 2;
    const __m256i mask256 = _mm256_set_epi32 (
        0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
        0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff);
    const __m128i mask128 = _mm_set_epi32 (
        0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff);
    const __m256i zero = _mm256_setzero_si256 ();

    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t *);

    while (row_parts_out + 4 <= row_parts_out_max)
    {
        __m256i factors;
        __m256i m0, m1;
        __m128i n0, n1, n2, n3, n4, n5;
        const uint64_t * SMOL_RESTRICT p0;

        p0 = row_parts_in + *(precalc_x++) * 2;
        n4 = _mm_set1_epi16 (*(precalc_x++));
        n0 = _mm_load_si128 ((__m128i *) p0);
        n1 = _mm_load_si128 ((__m128i *) p0 + 1);

        p0 = row_parts_in + *(precalc_x++) * 2;
        n5 = _mm_set1_epi16 (*(precalc_x++));
        n2 = _mm_load_si128 ((__m128i *) p0);
        n3 = _mm_load_si128 ((__m128i *) p0 + 1);

        m0 = _mm256_set_m128i (n2, n0);
        m1 = _mm256_set_m128i (n3, n1);
        factors = _mm256_set_m128i (n5, n4);
        factors = _mm256_blend_epi16 (factors, zero, 0xaa);

        m0 = LERP_SIMD256_EPI32_AND_MASK (m0, m1, factors, mask256);
        _mm256_store_si256 ((__m256i *) row_parts_out, m0);
        row_parts_out += 4;
    }

    /* No need for a loop here; let compiler know we're doing it at most once */
    if (row_parts_out != row_parts_out_max)
    {
        __m128i factors;
        __m128i m0, m1;
        uint32_t f;
        const uint64_t * SMOL_RESTRICT p0;

        p0 = row_parts_in + *(precalc_x++) * 2;
        f = *(precalc_x++);

        factors = _mm_set1_epi32 ((uint32_t) f);
        m0 = _mm_stream_load_si128 ((__m128i *) p0);
        m1 = _mm_stream_load_si128 ((__m128i *) p0 + 1);

        m0 = LERP_SIMD128_EPI32_AND_MASK (m0, m1, factors, mask128);
        _mm_store_si128 ((__m128i *) row_parts_out, m0);
        row_parts_out += 2;
    }
}

#define DEF_INTERP_HORIZONTAL_BILINEAR_128BPP(n_halvings) \
static void \
interp_horizontal_bilinear_##n_halvings##h_128bpp (const SmolScaleCtx *scale_ctx, \
                                                   const uint64_t * SMOL_RESTRICT row_parts_in, \
                                                   uint64_t * SMOL_RESTRICT row_parts_out) \
{ \
    const uint16_t * SMOL_RESTRICT precalc_x = scale_ctx->hdim.precalc; \
    uint64_t *row_parts_out_max = row_parts_out + scale_ctx->hdim.placement_size_px * 2; \
    const __m256i mask256 = _mm256_set_epi32 ( \
        0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, \
        0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff); \
    const __m128i mask128 = _mm_set_epi32 ( \
        0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff); \
    const __m256i zero256 = _mm256_setzero_si256 (); \
    int i; \
     \
    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t *); \
\
    while (row_parts_out != row_parts_out_max) \
    { \
        __m256i a0 = _mm256_setzero_si256 (); \
        __m128i a1; \
\
        for (i = 0; i < (1 << ((n_halvings) - 1)); i++) \
        { \
            __m256i m0, m1; \
            __m256i factors; \
            __m128i n0, n1, n2, n3, n4, n5; \
            const uint64_t * SMOL_RESTRICT p0; \
\
            p0 = row_parts_in + *(precalc_x++) * 2; \
            n4 = _mm_set1_epi16 (*(precalc_x++)); \
            n0 = _mm_load_si128 ((__m128i *) p0); \
            n1 = _mm_load_si128 ((__m128i *) p0 + 1); \
\
            p0 = row_parts_in + *(precalc_x++) * 2; \
            n5 = _mm_set1_epi16 (*(precalc_x++)); \
            n2 = _mm_load_si128 ((__m128i *) p0); \
            n3 = _mm_load_si128 ((__m128i *) p0 + 1); \
\
            m0 = _mm256_set_m128i (n2, n0); \
            m1 = _mm256_set_m128i (n3, n1); \
            factors = _mm256_set_m128i (n5, n4); \
            factors = _mm256_blend_epi16 (factors, zero256, 0xaa); \
\
            m0 = LERP_SIMD256_EPI32_AND_MASK (m0, m1, factors, mask256); \
            a0 = _mm256_add_epi32 (a0, m0); \
        } \
\
        a1 = _mm_add_epi32 (_mm256_extracti128_si256 (a0, 0), \
                            _mm256_extracti128_si256 (a0, 1)); \
        a1 = _mm_srli_epi32 (a1, (n_halvings)); \
        a1 = _mm_and_si128 (a1, mask128); \
        _mm_store_si128 ((__m128i *) row_parts_out, a1); \
        row_parts_out += 2; \
    } \
}

DEF_INTERP_HORIZONTAL_BILINEAR_128BPP(1)
DEF_INTERP_HORIZONTAL_BILINEAR_128BPP(2)
DEF_INTERP_HORIZONTAL_BILINEAR_128BPP(3)
DEF_INTERP_HORIZONTAL_BILINEAR_128BPP(4)
DEF_INTERP_HORIZONTAL_BILINEAR_128BPP(5)
DEF_INTERP_HORIZONTAL_BILINEAR_128BPP(6)

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
                             const uint64_t * SMOL_RESTRICT row_parts_in,
                             uint64_t * SMOL_RESTRICT row_parts_out)
{
    uint64_t *row_parts_out_max = row_parts_out + scale_ctx->hdim.placement_size_px;
    uint64_t part;

    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t *);

    part = *row_parts_in;
    while (row_parts_out != row_parts_out_max)
        *(row_parts_out++) = part;
}

static void
interp_horizontal_one_128bpp (const SmolScaleCtx *scale_ctx,
                              const uint64_t * SMOL_RESTRICT row_parts_in,
                              uint64_t * SMOL_RESTRICT row_parts_out)
{
    uint64_t *row_parts_out_max = row_parts_out + scale_ctx->hdim.placement_size_px * 2;

    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t *);

    while (row_parts_out != row_parts_out_max)
    {
        *(row_parts_out++) = row_parts_in [0];
        *(row_parts_out++) = row_parts_in [1];
    }
}

static void
interp_horizontal_copy_64bpp (const SmolScaleCtx *scale_ctx,
                              const uint64_t * SMOL_RESTRICT row_parts_in,
                              uint64_t * SMOL_RESTRICT row_parts_out)
{
    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t *);

    memcpy (row_parts_out, row_parts_in, scale_ctx->hdim.placement_size_px * sizeof (uint64_t));
}

static void
interp_horizontal_copy_128bpp (const SmolScaleCtx *scale_ctx,
                               const uint64_t * SMOL_RESTRICT row_parts_in,
                               uint64_t * SMOL_RESTRICT row_parts_out)
{
    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t *);

    memcpy (row_parts_out, row_parts_in, scale_ctx->hdim.placement_size_px * 2 * sizeof (uint64_t));
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
                                      const uint64_t * SMOL_RESTRICT top_row_parts_in,
                                      const uint64_t * SMOL_RESTRICT bottom_row_parts_in,
                                      uint64_t * SMOL_RESTRICT parts_out,
                                      uint32_t width)
{
    const __m256i mask = _mm256_set_epi16 (0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff,
                                           0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff);
    uint64_t *parts_out_last = parts_out + width;
    __m256i F256;

    SMOL_ASSUME_ALIGNED (top_row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (bottom_row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (parts_out, uint64_t *);

    F256 = _mm256_set1_epi16 ((uint16_t) F);

    while (parts_out + 4 <= parts_out_last)
    {
        __m256i m0, m1;

        m0 = _mm256_load_si256 ((const __m256i *) top_row_parts_in);
        top_row_parts_in += 4;
        m1 = _mm256_load_si256 ((const __m256i *) bottom_row_parts_in);
        bottom_row_parts_in += 4;

        m0 = _mm256_sub_epi16 (m0, m1);
        m0 = _mm256_mullo_epi16 (m0, F256);
        m0 = _mm256_srli_epi16 (m0, 8);
        m0 = _mm256_add_epi16 (m0, m1);
        m0 = _mm256_and_si256 (m0, mask);

        _mm256_store_si256 ((__m256i *) parts_out, m0);
        parts_out += 4;
    }

    while (parts_out != parts_out_last)
    {
        uint64_t p, q;

        p = *(top_row_parts_in++);
        q = *(bottom_row_parts_in++);

        *(parts_out++) = ((((p - q) * F) >> 8) + q) & 0x00ff00ff00ff00ffULL;
    }
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
interp_vertical_bilinear_add_64bpp (uint16_t F,
                                    const uint64_t *top_row_parts_in,
                                    const uint64_t *bottom_row_parts_in,
                                    uint64_t *accum_out,
                                    uint32_t width)
{
    const __m256i mask = _mm256_set_epi16 (0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff,
                                           0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff);
    uint64_t *accum_out_last = accum_out + width;
    __m256i F256;

    SMOL_ASSUME_ALIGNED (top_row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (bottom_row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (accum_out, uint64_t *);

    F256 = _mm256_set1_epi16 ((uint16_t) F);

    while (accum_out + 4 <= accum_out_last)
    {
        __m256i m0, m1, o0;

        m0 = _mm256_load_si256 ((const __m256i *) top_row_parts_in);
        top_row_parts_in += 4;
        m1 = _mm256_load_si256 ((const __m256i *) bottom_row_parts_in);
        bottom_row_parts_in += 4;
        o0 = _mm256_load_si256 ((const __m256i *) accum_out);

        m0 = _mm256_sub_epi16 (m0, m1);
        m0 = _mm256_mullo_epi16 (m0, F256);
        m0 = _mm256_srli_epi16 (m0, 8);
        m0 = _mm256_add_epi16 (m0, m1);
        m0 = _mm256_and_si256 (m0, mask);

        o0 = _mm256_add_epi16 (o0, m0);
        _mm256_store_si256 ((__m256i *) accum_out, o0);
        accum_out += 4;
    }

    while (accum_out != accum_out_last)
    {
        uint64_t p, q;

        p = *(top_row_parts_in++);
        q = *(bottom_row_parts_in++);

        *(accum_out++) += ((((p - q) * F) >> 8) + q) & 0x00ff00ff00ff00ffULL;
    }
}

static void
interp_vertical_bilinear_store_128bpp (uint64_t F,
                                       const uint64_t * SMOL_RESTRICT top_row_parts_in,
                                       const uint64_t * SMOL_RESTRICT bottom_row_parts_in,
                                       uint64_t * SMOL_RESTRICT parts_out,
                                       uint32_t width)
{
    uint64_t *parts_out_last = parts_out + width;
    const __m256i mask = _mm256_set_epi32 (
        0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 
        0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff);
    __m256i F256;

    SMOL_ASSUME_ALIGNED (top_row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (bottom_row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (parts_out, uint64_t *);

    F256 = _mm256_set1_epi32 ((uint32_t) F);

    while (parts_out + 8 <= parts_out_last)
    {
        __m256i m0, m1, m2, m3;

        m0 = _mm256_load_si256 ((const __m256i *) top_row_parts_in);
        top_row_parts_in += 4;
        m2 = _mm256_load_si256 ((const __m256i *) top_row_parts_in);
        top_row_parts_in += 4;
        m1 = _mm256_load_si256 ((const __m256i *) bottom_row_parts_in);
        bottom_row_parts_in += 4;
        m3 = _mm256_load_si256 ((const __m256i *) bottom_row_parts_in);
        bottom_row_parts_in += 4;

        m0 = _mm256_sub_epi32 (m0, m1);
        m2 = _mm256_sub_epi32 (m2, m3);
        m0 = _mm256_mullo_epi32 (m0, F256);
        m2 = _mm256_mullo_epi32 (m2, F256);
        m0 = _mm256_srli_epi32 (m0, 8);
        m2 = _mm256_srli_epi32 (m2, 8);
        m0 = _mm256_add_epi32 (m0, m1);
        m2 = _mm256_add_epi32 (m2, m3);
        m0 = _mm256_and_si256 (m0, mask);
        m2 = _mm256_and_si256 (m2, mask);

        _mm256_store_si256 ((__m256i *) parts_out, m0);
        parts_out += 4;
        _mm256_store_si256 ((__m256i *) parts_out, m2);
        parts_out += 4;
    }

    while (parts_out != parts_out_last)
    {
        uint64_t p, q;

        p = *(top_row_parts_in++);
        q = *(bottom_row_parts_in++);

        *(parts_out++) = ((((p - q) * F) >> 8) + q) & 0x00ffffff00ffffffULL;
    }
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
                                     const uint64_t * SMOL_RESTRICT top_row_parts_in,
                                     const uint64_t * SMOL_RESTRICT bottom_row_parts_in,
                                     uint64_t * SMOL_RESTRICT accum_out,
                                     uint32_t width)
{
    uint64_t *accum_out_last = accum_out + width;
    const __m256i mask = _mm256_set_epi32 (
        0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 
        0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff);
    __m256i F256;

    SMOL_ASSUME_ALIGNED (top_row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (bottom_row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (accum_out, uint64_t *);

    F256 = _mm256_set1_epi32 ((uint32_t) F);

    while (accum_out + 8 <= accum_out_last)
    {
        __m256i m0, m1, m2, m3, o0, o1;

        m0 = _mm256_load_si256 ((const __m256i *) top_row_parts_in);
        top_row_parts_in += 4;
        m2 = _mm256_load_si256 ((const __m256i *) top_row_parts_in);
        top_row_parts_in += 4;
        m1 = _mm256_load_si256 ((const __m256i *) bottom_row_parts_in);
        bottom_row_parts_in += 4;
        m3 = _mm256_load_si256 ((const __m256i *) bottom_row_parts_in);
        bottom_row_parts_in += 4;
        o0 = _mm256_load_si256 ((const __m256i *) accum_out);
        o1 = _mm256_load_si256 ((const __m256i *) (accum_out + 4));

        m0 = _mm256_sub_epi32 (m0, m1);
        m2 = _mm256_sub_epi32 (m2, m3);
        m0 = _mm256_mullo_epi32 (m0, F256);
        m2 = _mm256_mullo_epi32 (m2, F256);
        m0 = _mm256_srli_epi32 (m0, 8);
        m2 = _mm256_srli_epi32 (m2, 8);
        m0 = _mm256_add_epi32 (m0, m1);
        m2 = _mm256_add_epi32 (m2, m3);
        m0 = _mm256_and_si256 (m0, mask);
        m2 = _mm256_and_si256 (m2, mask);

        o0 = _mm256_add_epi32 (o0, m0);
        o1 = _mm256_add_epi32 (o1, m2);
        _mm256_store_si256 ((__m256i *) accum_out, o0);
        accum_out += 4;
        _mm256_store_si256 ((__m256i *) accum_out, o1);
        accum_out += 4;
    }

    while (accum_out != accum_out_last)
    {
        uint64_t p, q;

        p = *(top_row_parts_in++);
        q = *(bottom_row_parts_in++);

        *(accum_out++) += ((((p - q) * F) >> 8) + q) & 0x00ffffff00ffffffULL;
    }
}

#define DEF_INTERP_VERTICAL_BILINEAR_FINAL(n_halvings) \
static void \
interp_vertical_bilinear_final_##n_halvings##h_64bpp (uint64_t F, \
                                                      const uint64_t * SMOL_RESTRICT top_row_parts_in, \
                                                      const uint64_t * SMOL_RESTRICT bottom_row_parts_in, \
                                                      uint64_t * SMOL_RESTRICT accum_inout, \
                                                      uint32_t width) \
{ \
    const __m256i mask = _mm256_set_epi16 (0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, \
                                           0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff, 0x00ff); \
    uint64_t *accum_inout_last = accum_inout + width; \
    __m256i F256; \
\
    SMOL_ASSUME_ALIGNED (top_row_parts_in, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (bottom_row_parts_in, const uint64_t *); \
    SMOL_ASSUME_ALIGNED (accum_inout, uint64_t *); \
\
    F256 = _mm256_set1_epi16 ((uint16_t) F); \
\
    while (accum_inout + 4 <= accum_inout_last) \
    { \
        __m256i m0, m1, o0; \
\
        m0 = _mm256_load_si256 ((const __m256i *) top_row_parts_in); \
        top_row_parts_in += 4; \
        m1 = _mm256_load_si256 ((const __m256i *) bottom_row_parts_in); \
        bottom_row_parts_in += 4; \
        o0 = _mm256_load_si256 ((const __m256i *) accum_inout); \
\
        m0 = _mm256_sub_epi16 (m0, m1); \
        m0 = _mm256_mullo_epi16 (m0, F256); \
        m0 = _mm256_srli_epi16 (m0, 8); \
        m0 = _mm256_add_epi16 (m0, m1); \
        m0 = _mm256_and_si256 (m0, mask); \
\
        o0 = _mm256_add_epi16 (o0, m0); \
        o0 = _mm256_srli_epi16 (o0, n_halvings); \
\
        _mm256_store_si256 ((__m256i *) accum_inout, o0); \
        accum_inout += 4; \
    } \
\
    while (accum_inout != accum_inout_last) \
    { \
        uint64_t p, q; \
\
        p = *(top_row_parts_in++); \
        q = *(bottom_row_parts_in++); \
\
        p = ((((p - q) * F) >> 8) + q) & 0x00ff00ff00ff00ffULL; \
        p = ((p + *accum_inout) >> n_halvings) & 0x00ff00ff00ff00ffULL; \
\
        *(accum_inout++) = p; \
    } \
} \
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
                         uint64_t * SMOL_RESTRICT parts_out,
                         uint32_t n)
{
    uint64_t *parts_out_max = parts_out + n;

    SMOL_ASSUME_ALIGNED (accums, const uint64_t *);
    SMOL_ASSUME_ALIGNED (parts_out, uint64_t *);

    while (parts_out != parts_out_max)
    {
        *(parts_out++) = scale_64bpp (*(accums++), multiplier);
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

/* --------------- *
 * Function tables *
 * --------------- */

#define R SMOL_REPACK_META

static const SmolRepackMeta repack_meta [] =
{
    R (123,   24, PREMUL8,      COMPRESSED, 1324,  64, PREMUL8,       COMPRESSED),

    R (123,   24, PREMUL8,      COMPRESSED, 1234, 128, PREMUL8,       COMPRESSED),

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
            NULL
        },
        {
            /* 32bpp */
            NULL
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
            NULL
        },
        {
            /* 32bpp */
            NULL
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

        { { NULL, NULL, NULL }, { NULL, NULL, NULL } },  /* 24bpp - unused */
        { { NULL, NULL, NULL }, { NULL, NULL, NULL } },  /* 32bpp - unused */
        { { NULL, NULL, NULL }, { NULL, NULL, NULL } },  /* 64bpp */
        { { NULL, NULL, NULL }, { NULL, NULL, NULL } },  /* 128bpp */
    },
    {
        /* Composite over dest */
        NULL,
        NULL,
        NULL,
        NULL
    },
    {
        /* Clear dest */
        NULL,
        NULL,
        NULL,
        NULL
    },
    repack_meta
};

const SmolImplementation *
_smol_get_avx2_implementation (void)
{
    return &implementation;
}
