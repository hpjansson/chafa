/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright © 2019-2025 Hans Petter Jansson. See COPYING for details. */

#include <assert.h> /* assert */
#include <stddef.h> /* ptrdiff_t */
#include <string.h> /* memcpy */
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
 * has offsets and factors alternating one by one:
 *
 * ooooooooooooooooffffffffffffffffooooooooooooooooffffffffffffffffofofofofof...
 *
 * 16 offsets layout: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
 * 16 factors layout: 0 2 4 6 8 10 12 14 1 3 5 7 9 11 13 15
 *
 * n_batch_samples is the number of samples the filter's bulk loop consumes
 * batch-wise before switching to the epilogue; it must be a multiple of the
 * batch size. Everything from there on is stored in epilogue layout. The
 * bulk loops consume whole output pixels, so with halvings the epilogue can
 * hold 16 or more o/f pairs. */

/* Sample layouts per halving depth, applied within a perm window of one
 * batch (16 samples) or, for 3H, two batches (32). The batch
 * interpolator emits stored slots 0..3, 4..7, 8..11, 12..15 as its four
 * output registers, so interleaving the samples at precalc time makes
 * each halving a plain vertical add with the sums emerging in output
 * order. 0H stores results directly and needs the identity. 3H spreads
 * each output pixel's eight samples across two consecutive batches, four
 * per batch, so each batch folds to per-pixel partial sums and one
 * cross-batch add finishes the job. */
static const uint8_t batch_sample_perm_linear [BILIN_HORIZ_BATCH_PIXELS] =
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
static const uint8_t batch_sample_perm_1h [BILIN_HORIZ_BATCH_PIXELS] =
    { 0, 4, 1, 5, 2, 6, 3, 7, 8, 12, 9, 13, 10, 14, 11, 15 };
static const uint8_t batch_sample_perm_2h [BILIN_HORIZ_BATCH_PIXELS] =
    { 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15 };
static const uint8_t batch_sample_perm_3h [BILIN_HORIZ_BATCH_PIXELS * 2] =
    { 0, 4, 8, 12, 16, 20, 24, 28, 1, 5, 9, 13, 17, 21, 25, 29,
      2, 6, 10, 14, 18, 22, 26, 30, 3, 7, 11, 15, 19, 23, 27, 31 };

static uint32_t
array_offset_offset (uint32_t elem_i, int n_batch_samples, int do_batches,
                     const uint8_t *batch_perm, uint32_t perm_window)
{
    if (do_batches && (int) elem_i < n_batch_samples)
    {
        uint32_t slot = batch_perm [elem_i % perm_window];

        return (elem_i / perm_window) * (perm_window * 2)
            + (slot / BILIN_HORIZ_BATCH_PIXELS) * (BILIN_HORIZ_BATCH_PIXELS * 2)
            + (slot % BILIN_HORIZ_BATCH_PIXELS);
    }
    else
    {
        return elem_i * 2;
    }
}

static uint32_t
array_offset_factor (uint32_t elem_i, int n_batch_samples, int do_batches,
                     const uint8_t *batch_perm, uint32_t perm_window)
{
    const uint8_t o [BILIN_HORIZ_BATCH_PIXELS] = { 0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15 };

    if (do_batches && (int) elem_i < n_batch_samples)
    {
        uint32_t slot = batch_perm [elem_i % perm_window];

        return (elem_i / perm_window) * (perm_window * 2)
            + (slot / BILIN_HORIZ_BATCH_PIXELS) * (BILIN_HORIZ_BATCH_PIXELS * 2)
            + BILIN_HORIZ_BATCH_PIXELS + o [slot % BILIN_HORIZ_BATCH_PIXELS];
    }
    else
    {
        return elem_i * 2 + 1;
    }
}

static void
precalc_linear_range (uint16_t *array_out,
                      int64_t first_index,
                      int64_t last_index,
                      int n_batch_samples,
                      uint64_t first_sample_ofs,
                      uint64_t sample_step,
                      int sample_ofs_px_max,
                      int64_t clip_first_index,
                      int64_t clip_last_index,
                      int do_batches,
                      const uint8_t *batch_perm,
                      uint32_t perm_window,
                      int *array_i_inout)
{
    uint64_t sample_ofs;
    int64_t i;

    /* Precalc only the samples inside the clip window. Jumping straight to
     * the window keeps setup cost proportional to the visible size even
     * when most of a huge virtual placement is off-destination. */

    if (first_index < clip_first_index)
    {
        first_sample_ofs += sample_step * (uint64_t) (clip_first_index - first_index);
        first_index = clip_first_index;
    }

    if (last_index > clip_last_index)
        last_index = clip_last_index;

    sample_ofs = first_sample_ofs;

    for (i = first_index; i < last_index; i++)
    {
        uint64_t sample_ofs_px = sample_ofs / SMOL_BILIN_MULTIPLIER;

        if (sample_ofs_px >= (uint64_t) sample_ofs_px_max - 1)
        {
            array_out [array_offset_offset ((*array_i_inout), n_batch_samples, do_batches, batch_perm, perm_window)] = sample_ofs_px_max - 2;
            array_out [array_offset_factor ((*array_i_inout), n_batch_samples, do_batches, batch_perm, perm_window)] = 0;
            (*array_i_inout)++;
            continue;
        }

        array_out [array_offset_offset ((*array_i_inout), n_batch_samples, do_batches, batch_perm, perm_window)] = sample_ofs_px;
        array_out [array_offset_factor ((*array_i_inout), n_batch_samples, do_batches, batch_perm, perm_window)] = SMOL_SMALL_MUL
            - ((sample_ofs / (SMOL_BILIN_MULTIPLIER / SMOL_SMALL_MUL)) % SMOL_SMALL_MUL);
        (*array_i_inout)++;

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
                        int32_t dest_visible_px,
                        int n_batch_samples,
                        unsigned int do_batches)
{
    uint32_t src_dim_px = SMOL_SPX_TO_PX (src_dim_spx);
    uint64_t first_sample_ofs [3];
    uint64_t sample_step;
    /* The consumers index samples relative to the visible window, in
     * pre-halving (sample) units: visible output pixel p covers samples
     * [p << n_halvings, (p + 1) << n_halvings). */
    int64_t clip_first = (int64_t) dest_clip_before_px << n_halvings;
    int64_t clip_last = clip_first + ((int64_t) dest_visible_px << n_halvings);
    const uint8_t *batch_perm =
        n_halvings == 1 ? batch_sample_perm_1h :
        n_halvings == 2 ? batch_sample_perm_2h :
        n_halvings == 3 ? batch_sample_perm_3h : batch_sample_perm_linear;
    uint32_t perm_window = n_halvings == 3 ? BILIN_HORIZ_BATCH_PIXELS * 2
                                           : BILIN_HORIZ_BATCH_PIXELS;
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
        first_sample_ofs [1] = (sample_step * (SMOL_SUBPIXEL_MUL - dest_ofs_spx)
                                * (1 << n_halvings)) / SMOL_SUBPIXEL_MUL;
    }

    first_sample_ofs [2] = ((((uint64_t) src_dim_spx * SMOL_BILIN_MULTIPLIER * 2) / SMOL_SUBPIXEL_MUL)
        + sample_step - SMOL_BILIN_MULTIPLIER) / 2
        - sample_step * (1U << n_halvings);

    /* Left fringe */
    precalc_linear_range (array,
                          0,
                          1 << n_halvings,
                          n_batch_samples,
                          first_sample_ofs [0],
                          sample_step,
                          src_dim_px,
                          clip_first,
                          clip_last,
                          do_batches,
                          batch_perm,
                          perm_window,
                          &i);

    /* Check to prevent overruns when the output size is exactly 1 */
    if (dest_dim_prehalving_px > (1U << n_halvings))
    {
        /* Main range */
        precalc_linear_range (array,
                              1 << n_halvings,
                              dest_dim_prehalving_px - (1 << n_halvings),
                              n_batch_samples,
                              first_sample_ofs [1],
                              sample_step,
                              src_dim_px,
                              clip_first,
                              clip_last,
                              do_batches,
                              batch_perm,
                              perm_window,
                              &i);

        /* Right fringe */
        precalc_linear_range (array,
                              dest_dim_prehalving_px - (1 << n_halvings),
                              dest_dim_prehalving_px,
                              n_batch_samples,
                              first_sample_ofs [2],
                              sample_step,
                              src_dim_px,
                              clip_first,
                              clip_last,
                              do_batches,
                              batch_perm,
                              perm_window,
                              &i);
    }
}

static void
precalc_boxes_array (uint32_t *array,
                     uint32_t *span_step,
                     uint32_t *span_mul,
                     uint32_t src_dim_spx,
                     uint32_t dest_dim,
                     uint32_t dest_ofs_spx,
                     uint32_t dest_dim_spx,
                     int32_t dest_clip_before_px,
                     int32_t dest_visible_px)
{
    uint64_t fracF, frac_stepF;
    uint64_t f;
    uint64_t stride;
    uint64_t a, b;
    uint64_t last_ofs;
    int64_t clip_first, clip_last;
    int64_t dest_i, main_first, main_last;
    int i;

    dest_ofs_spx %= SMOL_SUBPIXEL_MUL;

    /* Output sample can't be less than a pixel. Fringe opacity is applied in
     * a separate step. When e.g. a 0.5px output straddles a pixel, both sides
     * will receive the same sampled color, which is technically incorrect, but
     * visually better than the alternatives. */
    if (dest_dim_spx < 256)
        dest_dim_spx = 256;

    frac_stepF = ((uint64_t) src_dim_spx * SMOL_BIG_MUL) / (uint64_t) dest_dim_spx;
    fracF = 0;

    stride = frac_stepF / (uint64_t) SMOL_BIG_MUL;
    f = (frac_stepF / SMOL_SMALL_MUL) % SMOL_SMALL_MUL;

    /* Floor division by (b + 1) guarantees the normalization always
     * undershoots: 255 * span_step <= 256 * (b + 1) - 1, so accum *
     * span_mul stays below vmax * SMOL_BOXES_MULTIPLIER and the
     * + SMOL_BOXES_MULTIPLIER / 2 rounding in scale_64bpp() /
     * scale_128bpp_half() can never push a lane past its canonical
     * maximum. Rounding this division to nearest instead can overshoot
     * for spans beyond ~570 px, spilling the premul16 alpha lane past
     * 0xffff, which the compositor would read back as alpha 0. The
     * undershoot costs under 1 LSB (16-bit) at ratios below 256x. */

    a = (SMOL_BOXES_MULTIPLIER * 255);
    b = ((stride * 255) + ((f * 255) / 256));
    *span_step = frac_stepF / SMOL_SMALL_MUL;
    *span_mul = a / (b + 1);

    /* The last span starts here, ending flush with the source's edge. It
     * doubles as a clamp for the main range: fractional placement sizes
     * can otherwise produce spans extending past the source. */
    last_ofs = ((uint64_t) src_dim_spx * SMOL_SMALL_MUL - frac_stepF) / SMOL_SMALL_MUL;

    /* Precalc only the entries inside the clip window; the consumers index
     * them relative to the visible window's start. */
    clip_first = dest_clip_before_px;
    clip_last = clip_first + dest_visible_px;
    i = 0;

    /* Left fringe */
    if (clip_first == 0 && clip_last > 0)
        array [i++] = 0;

    /* Main range */
    main_first = MAX (clip_first, 1);
    main_last = MIN (clip_last, (int64_t) dest_dim - 1);
    fracF = ((frac_stepF * (SMOL_SUBPIXEL_MUL - dest_ofs_spx)) / SMOL_SUBPIXEL_MUL)
        + (uint64_t) (main_first - 1) * frac_stepF;
    for (dest_i = main_first; dest_i < main_last; dest_i++)
    {
        array [i++] = MIN (fracF / SMOL_SMALL_MUL, last_ofs);
        fracF += frac_stepF;
    }

    /* Right fringe */
    if (dest_dim > 1
        && (int64_t) dest_dim - 1 >= clip_first
        && (int64_t) dest_dim - 1 < clip_last)
        array [i++] = last_ofs;
}

static void
init_dim (SmolDim *dim, int do_batches)
{
    if (dim->filter_type == SMOL_FILTER_ONE || dim->filter_type == SMOL_FILTER_COPY)
    {
    }
    else if (dim->filter_type == SMOL_FILTER_BOX)
    {
        /* Box has no halvings, so the prehalving size is the virtual
         * (unclipped) placement size the span geometry needs */
        precalc_boxes_array (dim->precalc,
                             &dim->span_step,
                             &dim->span_mul,
                             dim->src_size_spx,
                             dim->placement_size_prehalving_px,
                             dim->placement_ofs_spx,
                             dim->placement_size_spx,
                             dim->clip_before_px,
                             dim->placement_size_px);
    }
    else if (dim->filter_type >= SMOL_FILTER_BILINEAR_0H
             && dim->filter_type <= SMOL_FILTER_BILINEAR_3H)
    {
        int n_batch_samples = 0;

        if (do_batches)
        {
            /* The bulk loops of the batched filters consume whole output
             * pixels per iteration, 16 >> n_halvings of them down to a
             * minimum of 4, and leave the rest to the epilogue, which
             * expects interleaved layout. Match that boundary exactly. */
            int n_out_per_iter = MAX ((int) (16 >> dim->n_halvings), 4);

            n_batch_samples = (dim->placement_size_px / n_out_per_iter)
                * (n_out_per_iter << dim->n_halvings);
        }

        precalc_bilinear_array (dim->precalc,
                                dim->src_size_spx,
                                dim->placement_ofs_spx,
                                dim->placement_size_prehalving_spx,
                                dim->placement_size_prehalving_px,
                                dim->n_halvings,
                                dim->clip_before_px,
                                dim->placement_size_px,
                                n_batch_samples,
                                do_batches);
    }
    else
    {
        /* We don't have any AVX2 nearest filters */
        SMOL_ASSERT (FALSE);
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
    /* (alpha + 1) keeps RGB recoverable when alpha=0; matches the LUT in
     * smolscale.c (ceil (2^16 / (alpha + 1))) so the round-trip is exact. */
    inout [0] = inout [0] * ((uint16_t) alpha + 1);
    inout [1] = inout [1] * ((uint16_t) alpha + 1);
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

/* ------------------------- *
 * Batched repack alpha test *
 * ------------------------- */

#undef SMOL_BATCH_IS_OPAQUE_32BPP
#define SMOL_BATCH_IS_OPAQUE_32BPP(src, mask) \
    smol_batch_is_opaque_32bpp_avx2 ((src), (mask))

#undef SMOL_BATCH_ALPHA_CLASS_32BPP
#define SMOL_BATCH_ALPHA_CLASS_32BPP(src, mask) \
    smol_batch_alpha_class_32bpp_avx2 ((src), (mask))

static SMOL_INLINE int
smol_batch_is_opaque_32bpp_avx2 (const uint32_t *src, uint32_t alpha_mask)
{
    __m256i vacc;
    uint32_t i;

    /* Probe the first pixel */
    if ((src [0] & alpha_mask) != alpha_mask)
        return FALSE;

    vacc = _mm256_loadu_si256 ((const __m256i *) src);
    for (i = 8; i < PIXEL_BATCH_SIZE; i += 8)
        vacc = _mm256_and_si256 (
            vacc, _mm256_loadu_si256 ((const __m256i *) (src + i)));

    vacc = _mm256_and_si256 (vacc, _mm256_permute2x128_si256 (vacc, vacc, 0x01));
    vacc = _mm256_and_si256 (vacc, _mm256_shuffle_epi32 (vacc, 0x4e));
    vacc = _mm256_and_si256 (vacc, _mm256_shuffle_epi32 (vacc, 0xb1));

    return (((uint32_t) _mm256_extract_epi32 (vacc, 0)) & alpha_mask) == alpha_mask;
}

static SMOL_INLINE int
smol_batch_alpha_class_32bpp_avx2 (const uint32_t *src, uint32_t alpha_mask)
{
    __m256i vacc;
    uint32_t a;
    uint32_t i;

    /* The first pixel selects the reduction to run */
    a = src [0] & alpha_mask;

    if (a == alpha_mask)
    {
        vacc = _mm256_loadu_si256 ((const __m256i *) src);
        for (i = 8; i < PIXEL_BATCH_SIZE; i += 8)
            vacc = _mm256_and_si256 (
                vacc, _mm256_loadu_si256 ((const __m256i *) (src + i)));

        vacc = _mm256_and_si256 (vacc, _mm256_permute2x128_si256 (vacc, vacc, 0x01));
        vacc = _mm256_and_si256 (vacc, _mm256_shuffle_epi32 (vacc, 0x4e));
        vacc = _mm256_and_si256 (vacc, _mm256_shuffle_epi32 (vacc, 0xb1));

        return ((((uint32_t) _mm256_extract_epi32 (vacc, 0)) & alpha_mask)
                == alpha_mask) ? SMOL_BATCH_OPAQUE : SMOL_BATCH_MIXED;
    }

    if (a == 0)
    {
        vacc = _mm256_loadu_si256 ((const __m256i *) src);
        for (i = 8; i < PIXEL_BATCH_SIZE; i += 8)
            vacc = _mm256_or_si256 (
                vacc, _mm256_loadu_si256 ((const __m256i *) (src + i)));

        vacc = _mm256_or_si256 (vacc, _mm256_permute2x128_si256 (vacc, vacc, 0x01));
        vacc = _mm256_or_si256 (vacc, _mm256_shuffle_epi32 (vacc, 0x4e));
        vacc = _mm256_or_si256 (vacc, _mm256_shuffle_epi32 (vacc, 0xb1));

        return ((((uint32_t) _mm256_extract_epi32 (vacc, 0)) & alpha_mask)
                == 0) ? SMOL_BATCH_TRANSPARENT : SMOL_BATCH_MIXED;
    }

    return SMOL_BATCH_MIXED;
}

/* ---------------------- *
 * Repacking: 24/32 -> 64 *
 * ---------------------- */

#define UNPACK_32BPP_TO_64BPP_BATCHED_AVX2(pixel_func, alpha_ch, shuf) \
    SMOL_REPACK_BATCHED_3WAY (1, 1, \
        SMOL_BATCH_ALPHA_CLASS_32BPP (src_row, \
                                      SMOL_32BPP_ALPHA_MASK (alpha_ch)), \
        n * sizeof (uint64_t), \
        if (n >= 8) \
        { \
            const uint32_t *sp = src_row; \
            uint64_t *dp = dest_row; \
            unpack_8x_1234_p8_to_xxxx_p8_64bpp (&sp, &dp, dest_row + n, shuf); \
            for (i = (uint32_t) (dp - dest_row); i < n; i++) \
                dest_row [i] = pixel_func (src_row [i], TRUE); \
        } \
        else \
        { \
            for (i = 0; i < n; i++) \
                dest_row [i] = pixel_func (src_row [i], TRUE); \
        }, \
        for (i = 0; i < n; i++) \
            dest_row [i] = pixel_func (src_row [i], FALSE))

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
unpack_pixel_a234_u_to_324a_p8_64bpp (uint32_t p, int opaque)
{
    uint64_t p64 = (((uint64_t) p & 0x0000ff00) << 40) | (((uint64_t) p & 0x00ff00ff) << 16);
    uint8_t alpha = p >> 24;

    if (!opaque)
        p64 = premul_u_to_p8_64bpp (p64, alpha);

    return (p64 & 0xffffffffffffff00ULL) | alpha;
}

SMOL_REPACK_ROW_DEF (1234, 32, 32, UNASSOCIATED, COMPRESSED,
                     3241, 64, 64, PREMUL8,      COMPRESSED) {
    const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_64 (3, 2, 4, 1);
    UNPACK_32BPP_TO_64BPP_BATCHED_AVX2 (unpack_pixel_a234_u_to_324a_p8_64bpp,
                                        1, channel_shuf);
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE uint64_t
unpack_pixel_1234_u_to_2431_p8_64bpp (uint32_t p, int opaque)
{
    uint64_t p64 = (((uint64_t) p & 0x00ff00ff) << 32) | (((uint64_t) p & 0x0000ff00) << 8);
    uint8_t alpha = p >> 24;

    if (!opaque)
        p64 = premul_u_to_p8_64bpp (p64, alpha);

    return (p64 & 0xffffffffffffff00ULL) | alpha;
}

SMOL_REPACK_ROW_DEF (1234, 32, 32, UNASSOCIATED, COMPRESSED,
                     2431, 64, 64, PREMUL8,      COMPRESSED) {
    const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_64 (2, 4, 3, 1);
    UNPACK_32BPP_TO_64BPP_BATCHED_AVX2 (unpack_pixel_1234_u_to_2431_p8_64bpp,
                                      1, channel_shuf);
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE uint64_t
unpack_pixel_123a_u_to_132a_p8_64bpp (uint32_t p, int opaque)
{
    uint64_t p64 = (((uint64_t) p & 0xff00ff00) << 24) | (p & 0x00ff0000);
    uint8_t alpha = p & 0xff;

    if (!opaque)
        p64 = premul_u_to_p8_64bpp (p64, alpha);

    return (p64 & 0xffffffffffffff00ULL) | alpha;
}

SMOL_REPACK_ROW_DEF (1234, 32, 32, UNASSOCIATED, COMPRESSED,
                     1324, 64, 64, PREMUL8,      COMPRESSED) {
    const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_64 (1, 3, 2, 4);
    UNPACK_32BPP_TO_64BPP_BATCHED_AVX2 (unpack_pixel_123a_u_to_132a_p8_64bpp,
                                      4, channel_shuf);
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
    /* 1 at every non-alpha slot, 0x100 at every alpha slot. */
    const __m256i alpha_combine = _mm256_set_epi16 (
        1, 0x100, 1, 1,  1, 0x100, 1, 1,
        1, 0x100, 1, 1,  1, 0x100, 1, 1);
    /* 0xff at every alpha slot, 0 elsewhere. OR'd in after the multiply
     * to set the low byte of the alpha encoding. */
    const __m256i alpha_lowbyte = _mm256_set_epi16 (
        0, 0xff, 0, 0,  0, 0xff, 0, 0,
        0, 0xff, 0, 0,  0, 0xff, 0, 0);
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

        fact1 = _mm256_add_epi16 (fact1, alpha_combine);
        fact2 = _mm256_add_epi16 (fact2, alpha_combine);

        m1 = _mm256_mullo_epi16 (m1, fact1);
        m2 = _mm256_mullo_epi16 (m2, fact2);

        m1 = _mm256_or_si256 (m1, alpha_lowbyte);
        m2 = _mm256_or_si256 (m2, alpha_lowbyte);

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
                                       uint64_t *out,
                                       int opaque)
{
    uint64_t p64 = (((uint64_t) p & 0x00ff00ff) << 32) | (((uint64_t) p & 0x0000ff00) << 8);
    uint8_t alpha = p >> 24;

    if (!opaque)
        p64 = premul_u_to_p8_64bpp (p64, alpha);

    p64 = (p64 & 0xffffffffffffff00ULL) | alpha;
    out [0] = (p64 >> 16) & 0x000000ff000000ff;
    out [1] = p64 & 0x000000ff000000ff;
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     2341, 128, 64, PREMUL8,      COMPRESSED) {
    SMOL_UNPACK_32BPP_TO_P8_128BPP_BATCHED (unpack_pixel_a234_u_to_234a_p8_128bpp, 1);
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_a234_u_to_234a_p16_128bpp (uint32_t p,
                                        uint64_t *out)
{
    uint64_t p64 = p;
    uint64_t alpha = p >> 24;
    uint64_t mul = alpha + 1;

    out [0] = (((p64 & 0x00ff0000) << 16) | ((p64 & 0x0000ff00) >> 8)) * mul;
    out [1] = ((p64 & 0x000000ff) << 32) * mul | (alpha << 8) | 0xff;
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
                                       uint64_t *out,
                                       int opaque)
{
    uint64_t p64 = (((uint64_t) p & 0xff00ff00) << 24) | (p & 0x00ff0000);
    uint8_t alpha = p & 0xff;

    if (!opaque)
        p64 = premul_u_to_p8_64bpp (p64, alpha);

    p64 = (p64 & 0xffffffffffffff00ULL) | ((uint64_t) alpha);
    out [0] = (p64 >> 16) & 0x000000ff000000ff;
    out [1] = p64 & 0x000000ff000000ff;
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     1234, 128, 64, PREMUL8,      COMPRESSED) {
    SMOL_UNPACK_32BPP_TO_P8_128BPP_BATCHED (unpack_pixel_123a_u_to_123a_p8_128bpp, 4);
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_pixel_123a_u_to_123a_p16_128bpp (uint32_t p,
                                        uint64_t *out)
{
    uint64_t p64 = p;
    uint64_t alpha = p & 0xff;
    uint64_t mul = alpha + 1;

    out [0] = (((p64 & 0xff000000) << 8) | ((p64 & 0x00ff0000) >> 16)) * mul;
    out [1] = ((p64 & 0x0000ff00) << 24) * mul | (alpha << 8) | 0xff;
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

#define PACK_64BPP_TO_32BPP_BATCHED_AVX2(pixel_func, shuf) \
    SMOL_REPACK_BATCHED_3WAY (1, 1, \
        smol_batch_alpha_class_64bpp (src_row), \
        n * sizeof (uint32_t), \
        if (n >= 8) \
        { \
            const uint64_t *sp = src_row; \
            uint32_t *dp = dest_row; \
            pack_8x_1234_p8_to_xxxx_p8_64bpp (&sp, &dp, dest_row + n, shuf); \
            for (i = (uint32_t) (dp - dest_row); i < n; i++) \
                dest_row [i] = pixel_func (src_row [i], TRUE); \
        } \
        else \
        { \
            for (i = 0; i < n; i++) \
                dest_row [i] = pixel_func (src_row [i], TRUE); \
        }, \
        for (i = 0; i < n; i++) \
            dest_row [i] = pixel_func (src_row [i], FALSE))

static void
pack_8x_1234_p8_to_xxxx_p8_64bpp (const uint64_t * SMOL_RESTRICT *in,
                                  uint32_t * SMOL_RESTRICT *out,
                                  uint32_t * out_max,
                                  const __m256i channel_shuf)
{
    const __m256i * SMOL_RESTRICT my_in = (const __m256i * SMOL_RESTRICT) *in;
    __m256i * SMOL_RESTRICT my_out = (__m256i * SMOL_RESTRICT) *out;
    __m256i m0, m1;

    SMOL_ASSUME_ALIGNED (my_in, const __m256i * SMOL_RESTRICT);

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
                     1324, 32, 32, PREMUL8,       COMPRESSED) {
    const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_64 (1, 3, 2, 4);
    pack_8x_1234_p8_to_xxxx_p8_64bpp (&src_row, &dest_row, dest_row_max,
                                      channel_shuf);
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = pack_pixel_1234_p8_to_1324_p8_64bpp (*(src_row++));
    }
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE uint32_t
pack_pixel_1234_p8_to_1324_u_64bpp (uint64_t in, int opaque)
{
    uint8_t alpha = in;

    if (!opaque)
        in = (unpremul_p8_to_u_64bpp (in, alpha) & 0xffffffffffffff00ULL) | alpha;

    return pack_pixel_1234_p8_to_1324_p8_64bpp (in);
}

SMOL_REPACK_ROW_DEF (1234, 64, 64, PREMUL8,       COMPRESSED,
                     1324, 32, 32, UNASSOCIATED,  COMPRESSED) {
    const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_64 (1, 3, 2, 4);
    PACK_64BPP_TO_32BPP_BATCHED_AVX2 (pack_pixel_1234_p8_to_1324_u_64bpp,
                                      channel_shuf);
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
    static SMOL_INLINE uint32_t \
    pack_pixel_1234_p8_to_##a##b##c##d##_u_64bpp (uint64_t in, int opaque) \
    { \
        uint8_t alpha = in; \
        if (!opaque) \
            in = (unpremul_p8_to_u_64bpp (in, alpha) & 0xffffffffffffff00ULL) | alpha; \
        return PACK_FROM_1234_64BPP (in, a, b, c, d); \
    } \
    SMOL_REPACK_ROW_DEF (1234,       64, 64, PREMUL8,       COMPRESSED, \
                         a##b##c##d, 32, 32, UNASSOCIATED,  COMPRESSED) { \
        const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_64 ((a), (b), (c), (d)); \
        PACK_64BPP_TO_32BPP_BATCHED_AVX2 (pack_pixel_1234_p8_to_##a##b##c##d##_u_64bpp, \
                                          channel_shuf); \
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
                                   const __m256i channel_shuf,
                                   int opaque)
{
#define ALPHA_MUL (1 << (INVERTED_DIV_SHIFT_P16 - 8))
#define ALPHA_MASK SMOL_8X1BIT (0, 1, 0, 0, 0, 1, 0, 0)
#define OPAQUE_SHIFT (INVERTED_DIV_SHIFT_P16 - 8)

#define PACK_AND_STORE \
    do { \
        m0 = _mm256_packus_epi32 (m5, m6); \
        m1 = _mm256_packus_epi32 (m7, m8); \
        m0 = _mm256_packus_epi16 (m0, m1); \
\
        m0 = _mm256_shuffle_epi8 (m0, channel_shuf); \
        m0 = _mm256_permute4x64_epi64 (m0, SMOL_4X2BIT (3, 1, 2, 0)); \
        m0 = _mm256_shuffle_epi32 (m0, SMOL_4X2BIT (3, 1, 2, 0)); \
\
        _mm256_storeu_si256 (my_out, m0); \
        my_out += 1; \
    } while (0)

#define LOAD_4 \
    do { \
        m0 = _mm256_stream_load_si256 (my_in); my_in++; \
        m1 = _mm256_stream_load_si256 (my_in); my_in++; \
        m2 = _mm256_stream_load_si256 (my_in); my_in++; \
        m3 = _mm256_stream_load_si256 (my_in); my_in++; \
    } while (0)

    const __m256i ones = _mm256_set_epi32 (
        ALPHA_MUL, ALPHA_MUL, ALPHA_MUL, ALPHA_MUL,
        ALPHA_MUL, ALPHA_MUL, ALPHA_MUL, ALPHA_MUL);
    const __m256i alpha_clean_mask = _mm256_set_epi32 (
        0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
        0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff);
    const __m256i * SMOL_RESTRICT my_in = (const __m256i * SMOL_RESTRICT) *in;
    __m256i * SMOL_RESTRICT my_out = (__m256i * SMOL_RESTRICT) *out;
    __m256i m0, m1, m2, m3, m4, m5, m6, m7, m8;

    SMOL_ASSUME_ALIGNED (my_in, const __m256i * SMOL_RESTRICT);

    if (opaque)
    {
        while ((ptrdiff_t) (my_out + 1) <= (ptrdiff_t) out_max)
        {
            LOAD_4;

            m5 = _mm256_srli_epi32 (m0, OPAQUE_SHIFT);
            m6 = _mm256_srli_epi32 (m1, OPAQUE_SHIFT);
            m7 = _mm256_srli_epi32 (m2, OPAQUE_SHIFT);
            m8 = _mm256_srli_epi32 (m3, OPAQUE_SHIFT);

            PACK_AND_STORE;
        }
    }
    else
    {
        while ((ptrdiff_t) (my_out + 1) <= (ptrdiff_t) out_max)
        {
            LOAD_4;

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

            PACK_AND_STORE;
        }
    }

    *out = (uint32_t * SMOL_RESTRICT) my_out;
    *in = (const uint64_t * SMOL_RESTRICT) my_in;

#undef LOAD_4
#undef PACK_AND_STORE
#undef OPAQUE_SHIFT
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
                     321,   24,  8, PREMUL8,       COMPRESSED) {
    while (dest_row != dest_row_max)
    {
        *(dest_row++) = src_row [1] >> 32;
        *(dest_row++) = src_row [0];
        *(dest_row++) = src_row [0] >> 32;
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
    static SMOL_INLINE uint32_t \
    pack_pixel_p8_to_##a##b##c##d##_u_128bpp (const uint64_t *in, int opaque) \
    { \
        uint64_t t [2]; \
        uint8_t alpha; \
        if (opaque) \
            return PACK_FROM_1234_128BPP (in, a, b, c, d); \
        alpha = in [1]; \
        unpremul_p8_to_u_128bpp (in, t, alpha); \
        t [1] = (t [1] & 0xffffffff00000000ULL) | alpha; \
        return PACK_FROM_1234_128BPP (t, a, b, c, d); \
    } \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL8,       COMPRESSED, \
                         a##b##c##d,  32, 32, UNASSOCIATED,  COMPRESSED) { \
        SMOL_PACK_128BPP_TO_32BPP_BATCHED (pack_pixel_p8_to_##a##b##c##d##_u_128bpp, \
                                         SMOL_ALPHA_MASK_P8); \
    } SMOL_REPACK_ROW_DEF_END \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL16,      COMPRESSED, \
                         a##b##c##d,  32, 32, UNASSOCIATED,  COMPRESSED) { \
        const __m256i channel_shuf = PACK_SHUF_MM256_EPI8_32_TO_128 ((a), (b), (c), (d)); \
        SMOL_REPACK_BATCH_LOOP (2, 1, \
            SMOL_BATCH_IS_OPAQUE_128BPP (src_row, SMOL_ALPHA_MASK_INFLATED), \
            { \
                const uint64_t *sp = src_row; \
                uint32_t *dp = dest_row; \
                if (n >= 8) \
                    pack_8x_123a_p16_to_xxxx_u_128bpp (&sp, &dp, dest_row + n, \
                                                       channel_shuf, batch_opacity); \
                for (i = (uint32_t) (dp - dest_row); i < n; i++) \
                { \
                    uint64_t t [2]; \
                    uint8_t alpha = src_row [i * 2 + 1] >> 8; \
                    unpremul_p16_to_u_128bpp (src_row + i * 2, t, alpha); \
                    t [1] = (t [1] & 0xffffffff00000000ULL) | alpha; \
                    dest_row [i] = PACK_FROM_1234_128BPP (t, a, b, c, d); \
                } \
            }); \
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
sum_parts_64bpp (const uint64_t * SMOL_RESTRICT *parts_in,
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
sum_parts_128bpp (const uint64_t * SMOL_RESTRICT *parts_in,
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
                        uint64_t * SMOL_RESTRICT *row_parts_out)
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
    *u64_inout = ((*u64_inout * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ff00ff00ff00ffULL;
}

static SMOL_INLINE void
apply_subpixel_opacity_128bpp_half (uint64_t * SMOL_RESTRICT u64_inout, uint16_t opacity)
{
    *u64_inout = ((*u64_inout * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ffffff00ffffffULL;
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

        /* batch_sample_perm_1h pairs each output pixel's two samples
         * across (m0, m1) and (m2, m3) in matching lanes, so the halving
         * is a plain vertical add with sums in output order. */
        interp_horizontal_bilinear_batch_64bpp (row_parts_in, precalc_x, &m0, &m1, &m2, &m3);

        s0 = _mm256_srli_epi16 (_mm256_add_epi16 (m0, m1), 1);
        s1 = _mm256_srli_epi16 (_mm256_add_epi16 (m2, m3), 1);

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
        __m256i m0, m1, m2, m3, t;

        /* batch_sample_perm_2h strides each output pixel's four samples
         * across m0..m3 in matching lanes: two halvings collapse into
         * three vertical adds with sums in output order. */
        interp_horizontal_bilinear_batch_64bpp (row_parts_in, precalc_x, &m0, &m1, &m2, &m3);

        t = _mm256_add_epi16 (_mm256_add_epi16 (m0, m1), _mm256_add_epi16 (m2, m3));
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
        __m256i m0, m1, m2, m3, s0, s1;

        /* batch_sample_perm_3h strides each output pixel's eight samples
         * across two batches, four per batch in matching lanes: each
         * batch folds to per-pixel partial sums with three vertical adds,
         * and one cross-batch add completes them, in output order. */
        interp_horizontal_bilinear_batch_64bpp (row_parts_in, precalc_x, &m0, &m1, &m2, &m3);
        s0 = _mm256_add_epi16 (_mm256_add_epi16 (m0, m1), _mm256_add_epi16 (m2, m3));
        interp_horizontal_bilinear_batch_64bpp (row_parts_in, precalc_x + 32, &m0, &m1, &m2, &m3);
        s1 = _mm256_add_epi16 (_mm256_add_epi16 (m0, m1), _mm256_add_epi16 (m2, m3));

        s0 = _mm256_srli_epi16 (_mm256_add_epi16 (s0, s1), 3);
        _mm256_store_si256 ((__m256i *) row_parts_out, s0);

        row_parts_out += 4;
        precalc_x += 64;
    }

    interp_horizontal_bilinear_epilogue_64bpp (row_parts_in, row_parts_out, row_parts_out_max, precalc_x, 3);
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
        n0 = _mm_load_si128 ((const __m128i *) p0);
        n1 = _mm_load_si128 ((const __m128i *) p0 + 1);

        p0 = row_parts_in + *(precalc_x++) * 2;
        n5 = _mm_set1_epi16 (*(precalc_x++));
        n2 = _mm_load_si128 ((const __m128i *) p0);
        n3 = _mm_load_si128 ((const __m128i *) p0 + 1);

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
        m0 = _mm_load_si128 ((const __m128i *) p0);
        m1 = _mm_load_si128 ((const __m128i *) p0 + 1);

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
            n0 = _mm_load_si128 ((const __m128i *) p0); \
            n1 = _mm_load_si128 ((const __m128i *) p0 + 1); \
\
            p0 = row_parts_in + *(precalc_x++) * 2; \
            n5 = _mm_set1_epi16 (*(precalc_x++)); \
            n2 = _mm_load_si128 ((const __m128i *) p0); \
            n3 = _mm_load_si128 ((const __m128i *) p0 + 1); \
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
        sum_parts_64bpp (&pp, &accum, n);
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

        sum_parts_128bpp (&pp, accum, n);

        weight_pixel_128bpp (pp, t, f1);
        accum [0] += t [0];
        accum [1] += t [1];

        scale_and_store_128bpp (accum,
                                scale_ctx->hdim.span_mul,
                                (uint64_t * SMOL_RESTRICT *) &dest_row_parts);
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

    memcpy (row_parts_out, row_parts_in + scale_ctx->hdim.clip_before_px,
            scale_ctx->hdim.placement_size_px * sizeof (uint64_t));
}

static void
interp_horizontal_copy_128bpp (const SmolScaleCtx *scale_ctx,
                               const uint64_t * SMOL_RESTRICT row_parts_in,
                               uint64_t * SMOL_RESTRICT row_parts_out)
{
    SMOL_ASSUME_ALIGNED (row_parts_in, const uint64_t *);
    SMOL_ASSUME_ALIGNED (row_parts_out, uint64_t *);

    memcpy (row_parts_out, row_parts_in + scale_ctx->hdim.clip_before_px * 2,
            scale_ctx->hdim.placement_size_px * 2 * sizeof (uint64_t));
}

static void
scale_horizontal (const SmolScaleCtx *scale_ctx,
                  SmolLocalCtx *local_ctx,
                  const char *src_row,
                  uint64_t *dest_row_parts)
{
    uint64_t * SMOL_RESTRICT src_row_unpacked;

    src_row_unpacked = local_ctx->parts_row [3];

    if ((((uintptr_t) src_row) & 3)
        && scale_ctx->src_pixel_type != SMOL_PIXEL_RGB8
        && scale_ctx->src_pixel_type != SMOL_PIXEL_BGR8)
    {
        /* 32-bit unpackers need 32-bit alignment */
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
    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < SMOL_OPACITY_MAX) \
        interp_vertical_bilinear_final_##n_halvings##h_with_opacity_64bpp (precalc_y [bilin_index * 2 + 1], \
                                                                           local_ctx->parts_row [0], \
                                                                           local_ctx->parts_row [1], \
                                                                           local_ctx->parts_row [2], \
                                                                           scale_ctx->hdim.placement_size_px, \
                                                                           scale_ctx->vdim.first_opacity); \
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < SMOL_OPACITY_MAX) \
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
    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < SMOL_OPACITY_MAX) \
        interp_vertical_bilinear_final_##n_halvings##h_with_opacity_128bpp (precalc_y [bilin_index * 2 + 1], \
                                                                            local_ctx->parts_row [0], \
                                                                            local_ctx->parts_row [1], \
                                                                            local_ctx->parts_row [2], \
                                                                            scale_ctx->hdim.placement_size_px * 2, \
                                                                            scale_ctx->vdim.first_opacity); \
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < SMOL_OPACITY_MAX) \
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

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < SMOL_OPACITY_MAX)
        interp_vertical_bilinear_store_with_opacity_64bpp (precalc_y [dest_row_index * 2 + 1],
                                                           local_ctx->parts_row [0],
                                                           local_ctx->parts_row [1],
                                                           local_ctx->parts_row [2],
                                                           scale_ctx->hdim.placement_size_px,
                                                           scale_ctx->vdim.first_opacity);
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < SMOL_OPACITY_MAX)
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

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < SMOL_OPACITY_MAX)
        interp_vertical_bilinear_store_with_opacity_128bpp (precalc_y [dest_row_index * 2 + 1],
                                                            local_ctx->parts_row [0],
                                                            local_ctx->parts_row [1],
                                                            local_ctx->parts_row [2],
                                                            scale_ctx->hdim.placement_size_px * 2,
                                                            scale_ctx->vdim.first_opacity);
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < SMOL_OPACITY_MAX)
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

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < SMOL_OPACITY_MAX)
        interp_vertical_bilinear_final_1h_with_opacity_64bpp (precalc_y [bilin_index * 2 + 1],
                                                              local_ctx->parts_row [0],
                                                              local_ctx->parts_row [1],
                                                              local_ctx->parts_row [2],
                                                              scale_ctx->hdim.placement_size_px,
                                                              scale_ctx->vdim.first_opacity);
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < SMOL_OPACITY_MAX)
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

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < SMOL_OPACITY_MAX)
        interp_vertical_bilinear_final_1h_with_opacity_128bpp (precalc_y [bilin_index * 2 + 1],
                                                               local_ctx->parts_row [0],
                                                               local_ctx->parts_row [1],
                                                               local_ctx->parts_row [2],
                                                               scale_ctx->hdim.placement_size_px * 2,
                                                               scale_ctx->vdim.first_opacity);
    else if (dest_row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < SMOL_OPACITY_MAX)
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

    /* First input row. With sequential dest rows this is the same source
     * row that ended the previous span, and parts_row [0] will still hold
     * its horizontal scaling (tracked by src_ofs). */

    if (ofs_y != local_ctx->src_ofs)
    {
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, ofs_y),
                          local_ctx->parts_row [0]);
        local_ctx->src_ofs = ofs_y;
    }
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
        local_ctx->src_ofs = ofs_y;
        add_parts (local_ctx->parts_row [0],
                   local_ctx->parts_row [1],
                   scale_ctx->hdim.placement_size_px);

        ofs_y++;
    }

    /* Last input row. Skipped when its weight is zero (aligned integer
     * ratios) or when the span runs flush with the source's edge. */

    if (w2 > 0 && ofs_y < scale_ctx->vdim.src_size_px)
    {
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, ofs_y),
                          local_ctx->parts_row [0]);
        local_ctx->src_ofs = ofs_y;
        add_weighted_parts_64bpp (local_ctx->parts_row [0],
                                  local_ctx->parts_row [1],
                                  scale_ctx->hdim.placement_size_px,
                                  w2);
    }

    /* Finalize. The output goes in parts_row [2] so parts_row [0] can
     * carry the boundary row over to the next span. */

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < SMOL_OPACITY_MAX)
    {
        finalize_vertical_with_opacity_64bpp (local_ctx->parts_row [1],
                                              scale_ctx->vdim.span_mul,
                                              local_ctx->parts_row [2],
                                              scale_ctx->hdim.placement_size_px,
                                              scale_ctx->vdim.first_opacity);
    }
    else if (dest_row_index == scale_ctx->vdim.placement_size_px - 1 && scale_ctx->vdim.last_opacity < SMOL_OPACITY_MAX)
    {
        finalize_vertical_with_opacity_64bpp (local_ctx->parts_row [1],
                                              scale_ctx->vdim.span_mul,
                                              local_ctx->parts_row [2],
                                              scale_ctx->hdim.placement_size_px,
                                              scale_ctx->vdim.last_opacity);
    }
    else
    {
        finalize_vertical_64bpp (local_ctx->parts_row [1],
                                 scale_ctx->vdim.span_mul,
                                 local_ctx->parts_row [2],
                                 scale_ctx->hdim.placement_size_px);
    }

    return 2;
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

    /* First input row. With sequential dest rows this is the same source
     * row that ended the previous span, and parts_row [0] will still hold
     * its horizontal scaling (tracked by src_ofs). */

    if (ofs_y != local_ctx->src_ofs)
    {
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, ofs_y),
                          local_ctx->parts_row [0]);
        local_ctx->src_ofs = ofs_y;
    }
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
        local_ctx->src_ofs = ofs_y;
        add_parts (local_ctx->parts_row [0],
                   local_ctx->parts_row [1],
                   scale_ctx->hdim.placement_size_px * 2);

        ofs_y++;
    }

    /* Last input row. Skipped when its weight is zero (aligned integer
     * ratios) or when the span runs flush with the source's edge. */

    if (w2 > 0 && ofs_y < scale_ctx->vdim.src_size_px)
    {
        scale_horizontal (scale_ctx,
                          local_ctx,
                          src_row_ofs_to_pointer (scale_ctx, ofs_y),
                          local_ctx->parts_row [0]);
        local_ctx->src_ofs = ofs_y;
        add_weighted_parts_128bpp (local_ctx->parts_row [0],
                                   local_ctx->parts_row [1],
                                   scale_ctx->hdim.placement_size_px,
                                   w2);
    }

    /* Finalize. The output goes in parts_row [2] so parts_row [0] can
     * carry the boundary row over to the next span. */

    if (dest_row_index == 0 && scale_ctx->vdim.first_opacity < SMOL_OPACITY_MAX)
    {
        finalize_vertical_with_opacity_128bpp (local_ctx->parts_row [1],
                                               scale_ctx->vdim.span_mul,
                                               local_ctx->parts_row [2],
                                               scale_ctx->hdim.placement_size_px,
                                               scale_ctx->vdim.first_opacity);
    }
    else if (dest_row_index == scale_ctx->vdim.placement_size_px - 1 && scale_ctx->vdim.last_opacity < SMOL_OPACITY_MAX)
    {
        finalize_vertical_with_opacity_128bpp (local_ctx->parts_row [1],
                                               scale_ctx->vdim.span_mul,
                                               local_ctx->parts_row [2],
                                               scale_ctx->hdim.placement_size_px,
                                               scale_ctx->vdim.last_opacity);
    }
    else
    {
        finalize_vertical_128bpp (local_ctx->parts_row [1],
                                  scale_ctx->vdim.span_mul,
                                  local_ctx->parts_row [2],
                                  scale_ctx->hdim.placement_size_px);
    }

    return 2;
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

    if (row_index == 0 && scale_ctx->vdim.first_opacity < SMOL_OPACITY_MAX)
    {
        apply_subpixel_opacity_row_copy_64bpp (local_ctx->parts_row [0],
                                               local_ctx->parts_row [1],
                                               scale_ctx->hdim.placement_size_px,
                                               scale_ctx->vdim.first_opacity);
    }
    else if (row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < SMOL_OPACITY_MAX)
    {
        apply_subpixel_opacity_row_copy_64bpp (local_ctx->parts_row [0],
                                               local_ctx->parts_row [1],
                                               scale_ctx->hdim.placement_size_px,
                                               scale_ctx->vdim.last_opacity);
    }
    else
    {
        return 0;
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

    if (row_index == 0 && scale_ctx->vdim.first_opacity < SMOL_OPACITY_MAX)
    {
        apply_subpixel_opacity_row_copy_128bpp (local_ctx->parts_row [0],
                                                local_ctx->parts_row [1],
                                                scale_ctx->hdim.placement_size_px,
                                                scale_ctx->vdim.first_opacity);
    }
    else if (row_index == (scale_ctx->vdim.placement_size_px - 1) && scale_ctx->vdim.last_opacity < SMOL_OPACITY_MAX)
    {
        apply_subpixel_opacity_row_copy_128bpp (local_ctx->parts_row [0],
                                                local_ctx->parts_row [1],
                                                scale_ctx->hdim.placement_size_px,
                                                scale_ctx->vdim.last_opacity);
    }
    else
    {
        return 0;
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
                      src_row_ofs_to_pointer (scale_ctx,
                                              row_index + scale_ctx->vdim.clip_before_px),
                      local_ctx->parts_row [0]);

    return 0;
}

/* ----------- *
 * Compositing *
 * ----------- */

/* This runs faster without opacity batch reduction */
static void
composite_over_color_p8_64bpp (const uint64_t *src_row,
                               uint64_t *dest_row,
                               const uint64_t * SMOL_RESTRICT color_pixel,
                               uint32_t n_pixels,
                               uint16_t opacity)
{
    /* Broadcast each pixel's low 16-bit lane (alpha) to all four lanes */
    const __m256i alpha_shuf = _mm256_set_epi8 (9, 8, 9, 8, 9, 8, 9, 8,
                                                1, 0, 1, 0, 1, 0, 1, 0,
                                                9, 8, 9, 8, 9, 8, 9, 8,
                                                1, 0, 1, 0, 1, 0, 1, 0);
    const __m256i ff = _mm256_set1_epi16 (0xff);
    const __m256i r128 = _mm256_set1_epi16 (0x80);
    const __m256i zero = _mm256_setzero_si256 ();
    const __m256i opv = _mm256_set1_epi16 ((short) opacity);
    const __m256i cv = _mm256_set1_epi64x ((long long) *color_pixel);
    const SmolBool scale_opacity = (opacity < SMOL_OPACITY_MAX);
    const uint64_t c = *color_pixel;
    uint32_t n4 = n_pixels & ~3U;  /* Whole pixel quads */
    uint32_t i;

    /* Four pixels (4 x uint64_t = 16 x 16-bit lanes) per iteration, with
     * the broadcast color in the dest role */

    for (i = 0; i < n4; i += 4)
    {
        __m256i s = _mm256_loadu_si256 ((const __m256i *) (src_row + i));
        __m256i a, az, t, u;

        if (scale_opacity)
            s = _mm256_srli_epi16 (_mm256_mullo_epi16 (s, opv), SMOL_OPACITY_SHIFT);

        a = _mm256_shuffle_epi8 (s, alpha_shuf);
        az = _mm256_cmpeq_epi16 (a, zero);   /* pixel-wide: all lanes match */

        /* t = color * (0xff - a) + 128. out = src (squelched if a == 0)
         * + (t + (t >> 8)) >> 8 */
        t = _mm256_add_epi16 (_mm256_mullo_epi16 (cv, _mm256_sub_epi16 (ff, a)), r128);
        u = _mm256_srli_epi16 (_mm256_add_epi16 (t, _mm256_srli_epi16 (t, 8)), 8);

        _mm256_storeu_si256 ((__m256i *) (dest_row + i),
                             _mm256_add_epi16 (_mm256_andnot_si256 (az, s), u));
    }

    /* Scalar epilogue for the last few pixels */

    for (i = n4; i < n_pixels; i++)
    {
        uint64_t s = src_row [i];
        uint64_t a, nz, t;

        if (scale_opacity)
            s = ((s * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ff00ff00ff00ffULL;

        a = s & 0xff;
        nz = (a + 0xffULL) >> 8;  /* 0 if a == 0, else 1 */

        t = c * (0xff - a) + 0x0080008000800080ULL;
        dest_row [i] = s * nz
            + (((t + ((t >> 8) & 0x00ff00ff00ff00ffULL)) >> 8) & 0x00ff00ff00ff00ffULL);
    }
}

static void
composite_over_color_p16_128bpp_span (const uint64_t *src_row,
                                      uint64_t *dest_row,
                                      const uint64_t * SMOL_RESTRICT color_pixel,
                                      uint32_t n_pixels,
                                      uint16_t opacity)
{
    const __m256i mask24 = _mm256_set1_epi32 (0x00ffffff);
    const __m256i ff = _mm256_set1_epi32 (0xff);
    const __m256i x100 = _mm256_set1_epi32 (0x100);
    const __m256i r128 = _mm256_set1_epi32 (128);
    const __m256i opv = _mm256_set1_epi32 (opacity);
    const __m256i cv = _mm256_broadcastsi128_si256 (
        _mm_loadu_si128 ((const __m128i *) color_pixel));
    const SmolBool scale_opacity = (opacity < SMOL_OPACITY_MAX);
    uint32_t n2 = n_pixels & ~1U;  /* Whole pixel pairs */
    uint32_t i;

    /* Two pixels (4 x uint64_t = 8 x 32-bit lanes) per iteration, with
     * the color pair broadcast to both pixel slots. */

    for (i = 0; i < n2; i += 2)
    {
        __m256i s = _mm256_loadu_si256 ((const __m256i *) (src_row + (size_t) i * 2));
        __m256i a, nz, w, d;

        if (scale_opacity)
            s = _mm256_and_si256 (_mm256_srli_epi32 (_mm256_mullo_epi32 (s, opv),
                                                     SMOL_OPACITY_SHIFT),
                                  mask24);

        /* Broadcast each pixel's source alpha across its four lanes */
        a = _mm256_shuffle_epi32 (_mm256_srli_epi32 (s, 8), SMOL_4X2BIT (2, 2, 2, 2));
        a = _mm256_and_si256 (a, ff);

        /* nz = 0 if a == 0, else 1. w = 256 when a == 0, else 255 - a */
        nz = _mm256_srli_epi32 (_mm256_add_epi32 (a, ff), 8);
        w = _mm256_sub_epi32 (x100, _mm256_add_epi32 (a, nz));

        /* out = src * nz + (color * w + 128) >> 8 */
        d = _mm256_and_si256 (_mm256_srli_epi32 (_mm256_add_epi32 (
                                  _mm256_mullo_epi32 (cv, w), r128), 8),
                              mask24);
        d = _mm256_add_epi32 (_mm256_mullo_epi32 (s, nz), d);

        _mm256_storeu_si256 ((__m256i *) (dest_row + (size_t) i * 2), d);
    }

    /* Scalar epilogue for a final odd pixel */

    for (i = n2; i < n_pixels; i++)
    {
        uint64_t s0 = src_row [(size_t) i * 2];
        uint64_t s1 = src_row [(size_t) i * 2 + 1];
        uint64_t a, nz, w;

        if (scale_opacity)
        {
            s0 = ((s0 * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ffffff00ffffffULL;
            s1 = ((s1 * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ffffff00ffffffULL;
        }

        a = (s1 >> 8) & 0xff;
        nz = (a + 0xffULL) >> 8;  /* 0 if a == 0, else 1 */
        w = 0x100 - a - nz;  /* 256 when a == 0, else 255 - a */

        dest_row [(size_t) i * 2] = s0 * nz
            + (((color_pixel [0] * w + 0x0000008000000080ULL) >> 8)
               & 0x00ffffff00ffffffULL);
        dest_row [(size_t) i * 2 + 1] = s1 * nz
            + (((color_pixel [1] * w + 0x0000008000000080ULL) >> 8)
               & 0x00ffffff00ffffffULL);
    }
}

static void
composite_over_color_p16_128bpp (const uint64_t *src_row,
                                 uint64_t *dest_row,
                                 const uint64_t * SMOL_RESTRICT color_pixel,
                                 uint32_t n_pixels,
                                 uint16_t opacity)
{
    SMOL_COMPOSITE_OVER_COLOR_BATCHED (composite_over_color_p16_128bpp_span,
                                       smol_batch_alpha_class_128bpp (src_row + (size_t) i * 2,
                                                                      SMOL_ALPHA_MASK_INFLATED),
                                       2);
}

/* No need for opacity batching here; it's already plenty fast. In fact,
 * the batch reduction could slow us down */
static void
composite_over_color_src_alpha_p8_64bpp (const uint64_t *src_row,
                                         uint64_t *dest_row,
                                         const uint64_t * SMOL_RESTRICT color_pixel,
                                         uint32_t n_pixels,
                                         uint16_t opacity)
{
    /* Broadcast each pixel's low 16-bit lane (alpha) to all four lanes */
    const __m256i alpha_shuf = _mm256_set_epi8 (9, 8, 9, 8, 9, 8, 9, 8,
                                                1, 0, 1, 0, 1, 0, 1, 0,
                                                9, 8, 9, 8, 9, 8, 9, 8,
                                                1, 0, 1, 0, 1, 0, 1, 0);
    const __m256i ff = _mm256_set1_epi16 (0xff);
    const __m256i one = _mm256_set1_epi16 (1);
    const __m256i r128 = _mm256_set1_epi16 (0x80);
    const __m256i zero = _mm256_setzero_si256 ();
    const __m256i opv = _mm256_set1_epi16 ((short) opacity);
    const __m256i cv = _mm256_set1_epi64x ((long long) *color_pixel);
    const SmolBool scale_opacity = (opacity < SMOL_OPACITY_MAX);
    const uint64_t c = *color_pixel;
    uint32_t n4 = n_pixels & ~3U;  /* Whole pixel quads */
    uint32_t i;

    /* Four pixels (4 x uint64_t = 16 x 16-bit lanes) per iteration, with
     * the broadcast color in the dest role */

    for (i = 0; i < n4; i += 4)
    {
        __m256i s = _mm256_loadu_si256 ((const __m256i *) (src_row + i));
        __m256i a, az, t, u;

        if (scale_opacity)
            s = _mm256_srli_epi16 (_mm256_mullo_epi16 (s, opv), SMOL_OPACITY_SHIFT);

        a = _mm256_shuffle_epi8 (s, alpha_shuf);
        az = _mm256_cmpeq_epi16 (a, zero);   /* pixel-wide: all lanes match */

        /* t = color * (0xff - a) + 128. blend = src (squelched if a == 0)
         * + (t + (t >> 8)) >> 8 */
        t = _mm256_add_epi16 (_mm256_mullo_epi16 (cv, _mm256_sub_epi16 (ff, a)), r128);
        u = _mm256_srli_epi16 (_mm256_add_epi16 (t, _mm256_srli_epi16 (t, 8)), 8);
        t = _mm256_add_epi16 (_mm256_andnot_si256 (az, s), u);

        /* Re-encode by the source alpha */
        t = _mm256_srli_epi16 (
            _mm256_sub_epi16 (_mm256_mullo_epi16 (_mm256_add_epi16 (t, one),
                                                  _mm256_add_epi16 (a, one)),
                              one), 8);

        /* Rebuild the alpha lane (word 0 of each pixel) */
        _mm256_storeu_si256 ((__m256i *) (dest_row + i),
                             _mm256_blend_epi16 (t, a, SMOL_8X1BIT (0, 0, 0, 1, 0, 0, 0, 1)));
    }

    /* Scalar epilogue for the last few pixels */

    for (i = n4; i < n_pixels; i++)
    {
        uint64_t s = src_row [i];
        uint64_t a, nz, t;

        if (scale_opacity)
            s = ((s * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ff00ff00ff00ffULL;

        a = s & 0xff;
        nz = (a + 0xffULL) >> 8;  /* 0 if a == 0, else 1 */

        t = c * (0xff - a) + 0x0080008000800080ULL;
        t = s * nz
            + (((t + ((t >> 8) & 0x00ff00ff00ff00ffULL)) >> 8) & 0x00ff00ff00ff00ffULL);

        t = (((t + 0x0001000100010001ULL) * (a + 1) - 0x0001000100010001ULL) >> 8)
            & 0x00ff00ff00ff00ffULL;
        dest_row [i] = (t & 0xffffffffffff0000ULL) | a;
    }
}

/* Selects the 128bpp alpha lane (32-bit lane 2 of each pixel) in both pixel slots */
#define ALPHA_MASK SMOL_8X1BIT (0, 1, 0, 0, 0, 1, 0, 0)

static void
composite_over_color_src_alpha_p8_128bpp_span (const uint64_t *src_row,
                                               uint64_t *dest_row,
                                               const uint64_t * SMOL_RESTRICT color_pixel,
                                               uint32_t n_pixels,
                                               uint16_t opacity)
{
    const __m256i mask24 = _mm256_set1_epi32 (0x00ffffff);
    const __m256i ff = _mm256_set1_epi32 (0xff);
    const __m256i one = _mm256_set1_epi32 (1);
    const __m256i r128 = _mm256_set1_epi32 (128);
    const __m256i zero = _mm256_setzero_si256 ();
    const __m256i opv = _mm256_set1_epi32 (opacity);
    const __m256i cv = _mm256_broadcastsi128_si256 (
        _mm_loadu_si128 ((const __m128i *) color_pixel));
    const SmolBool scale_opacity = (opacity < SMOL_OPACITY_MAX);
    uint32_t n2 = n_pixels & ~1U;  /* Whole pixel pairs */
    uint32_t i;

    /* Two pixels (4 x uint64_t = 8 x 32-bit lanes) per iteration, with
     * the color pair broadcast to both pixel slots. */

    for (i = 0; i < n2; i += 2)
    {
        __m256i s = _mm256_loadu_si256 ((const __m256i *) (src_row + (size_t) i * 2));
        __m256i a, az, t, u;

        if (scale_opacity)
            s = _mm256_and_si256 (_mm256_srli_epi32 (_mm256_mullo_epi32 (s, opv),
                                                     SMOL_OPACITY_SHIFT),
                                  mask24);

        /* Broadcast each pixel's source alpha across its four lanes. Unlike
         * P16/P8L, the P8 alpha lane holds the bare value. */
        a = _mm256_and_si256 (_mm256_shuffle_epi32 (s, SMOL_4X2BIT (2, 2, 2, 2)), ff);
        az = _mm256_cmpeq_epi32 (a, zero);

        /* t = color * (0xff - a) + 128. blend = src (squelched if a == 0)
         * + (t + (t >> 8)) >> 8 */
        t = _mm256_add_epi32 (_mm256_mullo_epi32 (cv, _mm256_sub_epi32 (ff, a)), r128);
        u = _mm256_srli_epi32 (_mm256_add_epi32 (t, _mm256_srli_epi32 (t, 8)), 8);
        t = _mm256_add_epi32 (_mm256_andnot_si256 (az, s), u);

        /* Re-encode by the source alpha and rebuild the alpha lane */
        t = _mm256_srli_epi32 (
            _mm256_sub_epi32 (_mm256_mullo_epi32 (_mm256_add_epi32 (t, one),
                                                  _mm256_add_epi32 (a, one)),
                              one), 8);

        _mm256_storeu_si256 ((__m256i *) (dest_row + (size_t) i * 2),
                             _mm256_blend_epi32 (t, a, ALPHA_MASK));
    }

    /* Scalar epilogue for a final odd pixel */

    for (i = n2; i < n_pixels; i++)
    {
        uint64_t s0 = src_row [(size_t) i * 2];
        uint64_t s1 = src_row [(size_t) i * 2 + 1];
        uint64_t a, nz, w, t0, t1;

        if (scale_opacity)
        {
            s0 = ((s0 * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ffffff00ffffffULL;
            s1 = ((s1 * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ffffff00ffffffULL;
        }

        a = s1 & 0xff;
        nz = (a + 0xffULL) >> 8;  /* 0 if a == 0, else 1 */
        w = 0xff - a;

        t0 = color_pixel [0] * w + 0x0000008000000080ULL;
        t1 = color_pixel [1] * w + 0x0000008000000080ULL;
        t0 = s0 * nz
            + (((t0 + ((t0 >> 8) & 0x00ffffff00ffffffULL)) >> 8) & 0x00ffffff00ffffffULL);
        t1 = s1 * nz
            + (((t1 + ((t1 >> 8) & 0x00ffffff00ffffffULL)) >> 8) & 0x00ffffff00ffffffULL);

        t0 = (((t0 + 0x0000000100000001ULL) * (a + 1) - 0x0000000100000001ULL) >> 8)
            & 0x000000ff000000ffULL;
        t1 = (((t1 + 0x0000000100000001ULL) * (a + 1) - 0x0000000100000001ULL) >> 8)
            & 0x000000ff000000ffULL;

        dest_row [(size_t) i * 2] = t0;
        dest_row [(size_t) i * 2 + 1] = (t1 & 0xffffffff00000000ULL) | a;
    }
}

static void
composite_over_color_src_alpha_p8_128bpp (const uint64_t *src_row,
                                          uint64_t *dest_row,
                                          const uint64_t * SMOL_RESTRICT color_pixel,
                                          uint32_t n_pixels,
                                          uint16_t opacity)
{
    SMOL_COMPOSITE_OVER_COLOR_BATCHED (composite_over_color_src_alpha_p8_128bpp_span,
                                       smol_batch_alpha_class_128bpp (src_row + (size_t) i * 2,
                                                                      SMOL_ALPHA_MASK_P8),
                                       2);
}

static void
composite_over_color_src_alpha_p8l_128bpp_span (const uint64_t *src_row,
                                                uint64_t *dest_row,
                                                const uint64_t * SMOL_RESTRICT color_pixel,
                                                uint32_t n_pixels,
                                                uint16_t opacity)
{
    const __m256i mask24 = _mm256_set1_epi32 (0x00ffffff);
    const __m256i mask12 = _mm256_set1_epi32 (0x00000fff);
    const __m256i ff = _mm256_set1_epi32 (0xff);
    const __m256i one = _mm256_set1_epi32 (1);
    const __m256i x100 = _mm256_set1_epi32 (0x100);
    const __m256i r128 = _mm256_set1_epi32 (128);
    const __m256i opv = _mm256_set1_epi32 (opacity);
    const __m256i cv = _mm256_broadcastsi128_si256 (
        _mm_loadu_si128 ((const __m128i *) color_pixel));
    const SmolBool scale_opacity = (opacity < SMOL_OPACITY_MAX);
    uint32_t n2 = n_pixels & ~1U;  /* Whole pixel pairs */
    uint32_t i;

    /* Two pixels (4 x uint64_t = 8 x 32-bit lanes) per iteration, with
     * the color pair broadcast to both pixel slots. */

    for (i = 0; i < n2; i += 2)
    {
        __m256i s = _mm256_loadu_si256 ((const __m256i *) (src_row + (size_t) i * 2));
        __m256i a, nz, w, d;

        if (scale_opacity)
            s = _mm256_and_si256 (_mm256_srli_epi32 (_mm256_mullo_epi32 (s, opv),
                                                     SMOL_OPACITY_SHIFT),
                                  mask24);

        /* Broadcast each pixel's source alpha across its four lanes */
        a = _mm256_shuffle_epi32 (_mm256_srli_epi32 (s, 8), SMOL_4X2BIT (2, 2, 2, 2));
        a = _mm256_and_si256 (a, ff);

        /* nz = 0 if a == 0, else 1. w = 256 when a == 0, else 255 - a */
        nz = _mm256_srli_epi32 (_mm256_add_epi32 (a, ff), 8);
        w = _mm256_sub_epi32 (x100, _mm256_add_epi32 (a, nz));

        /* blend = src * nz + (color * w + 128) >> 8 */
        d = _mm256_and_si256 (_mm256_srli_epi32 (_mm256_add_epi32 (
                                  _mm256_mullo_epi32 (cv, w), r128), 8),
                              mask24);
        d = _mm256_add_epi32 (_mm256_mullo_epi32 (s, nz), d);

        /* Re-encode by the source alpha and rebuild the alpha late */
        d = _mm256_and_si256 (
            _mm256_srli_epi32 (_mm256_mullo_epi32 (_mm256_and_si256 (d, mask12),
                                                   _mm256_add_epi32 (a, one)), 8),
            mask12);

        _mm256_storeu_si256 ((__m256i *) (dest_row + (size_t) i * 2),
                             _mm256_blend_epi32 (d, _mm256_or_si256 (
                                                     _mm256_slli_epi32 (a, 8), ff),
                                                 ALPHA_MASK));
    }

    /* Scalar epilogue for a final odd pixel */

    for (i = n2; i < n_pixels; i++)
    {
        uint64_t s0 = src_row [(size_t) i * 2];
        uint64_t s1 = src_row [(size_t) i * 2 + 1];
        uint64_t a, nz, w, t0, t1;

        if (scale_opacity)
        {
            s0 = ((s0 * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ffffff00ffffffULL;
            s1 = ((s1 * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ffffff00ffffffULL;
        }

        a = (s1 >> 8) & 0xff;
        nz = (a + 0xffULL) >> 8;  /* 0 if a == 0, else 1 */
        w = 0x100 - a - nz;  /* 256 when a == 0, else 255 - a */

        t0 = s0 * nz + (((color_pixel [0] * w + 0x0000008000000080ULL) >> 8)
                        & 0x00ffffff00ffffffULL);
        t1 = s1 * nz + (((color_pixel [1] * w + 0x0000008000000080ULL) >> 8)
                        & 0x00ffffff00ffffffULL);

        t0 = (((t0 & 0x00000fff00000fffULL) * (a + 1)) >> 8) & 0x00000fff00000fffULL;
        t1 = (((t1 & 0x00000fff00000fffULL) * (a + 1)) >> 8) & 0x00000fff00000fffULL;

        dest_row [(size_t) i * 2] = t0;
        dest_row [(size_t) i * 2 + 1] = (t1 & 0xffffffff00000000ULL) | (a << 8) | 0xff;
    }
}

static void
composite_over_color_src_alpha_p8l_128bpp (const uint64_t *src_row,
                                           uint64_t *dest_row,
                                           const uint64_t * SMOL_RESTRICT color_pixel,
                                           uint32_t n_pixels,
                                           uint16_t opacity)
{
    SMOL_COMPOSITE_OVER_COLOR_BATCHED (composite_over_color_src_alpha_p8l_128bpp_span,
                                       smol_batch_alpha_class_128bpp (src_row + (size_t) i * 2,
                                                                      SMOL_ALPHA_MASK_INFLATED),
                                       2);
}

/* Also serves p16l. Both encode channels as value * (alpha + 1) */
static void
composite_over_color_src_alpha_p16_128bpp_span (const uint64_t *src_row,
                                                uint64_t *dest_row,
                                                const uint64_t * SMOL_RESTRICT color_pixel,
                                                uint32_t n_pixels,
                                                uint16_t opacity)
{
    const __m256i mask24 = _mm256_set1_epi32 (0x00ffffff);
    const __m256i mask16 = _mm256_set1_epi32 (0x0000ffff);
    const __m256i ff = _mm256_set1_epi32 (0xff);
    const __m256i one = _mm256_set1_epi32 (1);
    const __m256i x100 = _mm256_set1_epi32 (0x100);
    const __m256i r128 = _mm256_set1_epi32 (128);
    const __m256i opv = _mm256_set1_epi32 (opacity);
    const __m256i cv = _mm256_broadcastsi128_si256 (
        _mm_loadu_si128 ((const __m128i *) color_pixel));
    const SmolBool scale_opacity = (opacity < SMOL_OPACITY_MAX);
    uint32_t n2 = n_pixels & ~1U;  /* Whole pixel pairs */
    uint32_t i;

    /* Two pixels (4 x uint64_t = 8 x 32-bit lanes) per iteration, with
     * the color pair broadcast to both pixel slots. */

    for (i = 0; i < n2; i += 2)
    {
        __m256i s = _mm256_loadu_si256 ((const __m256i *) (src_row + (size_t) i * 2));
        __m256i a, nz, w, d;

        if (scale_opacity)
            s = _mm256_and_si256 (_mm256_srli_epi32 (_mm256_mullo_epi32 (s, opv),
                                                     SMOL_OPACITY_SHIFT),
                                  mask24);

        /* Broadcast each pixel's source alpha across its four lanes */
        a = _mm256_shuffle_epi32 (_mm256_srli_epi32 (s, 8), SMOL_4X2BIT (2, 2, 2, 2));
        a = _mm256_and_si256 (a, ff);

        /* nz = 0 if a == 0, else 1. w = 256 when a == 0, else 255 - a */
        nz = _mm256_srli_epi32 (_mm256_add_epi32 (a, ff), 8);
        w = _mm256_sub_epi32 (x100, _mm256_add_epi32 (a, nz));

        /* blend = src * nz + (color * w + 128) >> 8 */
        d = _mm256_and_si256 (_mm256_srli_epi32 (_mm256_add_epi32 (
                                  _mm256_mullo_epi32 (cv, w), r128), 8),
                              mask24);
        d = _mm256_add_epi32 (_mm256_mullo_epi32 (s, nz), d);

        /* Re-encode by the source alpha, then rebuild the alpha lane */
        d = _mm256_mullo_epi32 (_mm256_and_si256 (_mm256_srli_epi32 (d, 8), mask16),
                                _mm256_add_epi32 (a, one));

        _mm256_storeu_si256 ((__m256i *) (dest_row + (size_t) i * 2),
                             _mm256_blend_epi32 (d, _mm256_or_si256 (
                                                     _mm256_slli_epi32 (a, 8), ff),
                                                 ALPHA_MASK));
    }

    /* Scalar epilogue for a final odd pixel */

    for (i = n2; i < n_pixels; i++)
    {
        uint64_t s0 = src_row [(size_t) i * 2];
        uint64_t s1 = src_row [(size_t) i * 2 + 1];
        uint64_t a, nz, w;

        if (scale_opacity)
        {
            s0 = ((s0 * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ffffff00ffffffULL;
            s1 = ((s1 * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ffffff00ffffffULL;
        }

        a = (s1 >> 8) & 0xff;
        nz = (a + 0xffULL) >> 8;  /* 0 if a == 0, else 1 */
        w = 0x100 - a - nz;  /* 256 when a == 0, else 255 - a */

        s0 = s0 * nz + (((color_pixel [0] * w + 0x0000008000000080ULL) >> 8)
                        & 0x00ffffff00ffffffULL);
        s1 = s1 * nz + (((color_pixel [1] * w + 0x0000008000000080ULL) >> 8)
                        & 0x00ffffff00ffffffULL);

        s0 = ((s0 >> 8) & 0x0000ffff0000ffffULL) * (a + 1);
        s1 = ((s1 >> 8) & 0x0000ffff0000ffffULL) * (a + 1);

        dest_row [(size_t) i * 2] = s0;
        dest_row [(size_t) i * 2 + 1] = (s1 & 0xffffffff00000000ULL) | (a << 8) | 0xff;
    }
}

static void
composite_over_color_src_alpha_p16_128bpp (const uint64_t *src_row,
                                           uint64_t *dest_row,
                                           const uint64_t * SMOL_RESTRICT color_pixel,
                                           uint32_t n_pixels,
                                           uint16_t opacity)
{
    SMOL_COMPOSITE_OVER_COLOR_BATCHED (composite_over_color_src_alpha_p16_128bpp_span,
                                       smol_batch_alpha_class_128bpp (src_row + (size_t) i * 2,
                                                                      SMOL_ALPHA_MASK_INFLATED),
                                       2);
}

#undef ALPHA_MASK

/* This runs faster without opacity batch reduction */
static void
composite_over_dest_p8_64bpp (const uint64_t *src_row,
                              uint64_t * SMOL_RESTRICT dest_row,
                              uint32_t n_pixels,
                              uint16_t opacity)
{
    /* Broadcast each pixel's low 16-bit lane (alpha) to all four lanes */
    const __m256i alpha_shuf = _mm256_set_epi8 (9, 8, 9, 8, 9, 8, 9, 8,
                                                1, 0, 1, 0, 1, 0, 1, 0,
                                                9, 8, 9, 8, 9, 8, 9, 8,
                                                1, 0, 1, 0, 1, 0, 1, 0);
    const __m256i ff = _mm256_set1_epi16 (0xff);
    const __m256i r128 = _mm256_set1_epi16 (0x80);
    const __m256i zero = _mm256_setzero_si256 ();
    const __m256i opv = _mm256_set1_epi16 ((short) opacity);
    const SmolBool scale_opacity = (opacity < SMOL_OPACITY_MAX);
    uint32_t n4 = n_pixels & ~3U;
    uint32_t i;

    /* Four pixels (4 x uint64_t = 16 x 16-bit lanes) per iteration. */

    for (i = 0; i < n4; i += 4)
    {
        __m256i s = _mm256_loadu_si256 ((const __m256i *) (src_row + i));
        __m256i d = _mm256_loadu_si256 ((const __m256i *) (dest_row + i));
        __m256i a, az, t, u;

        if (scale_opacity)
            s = _mm256_srli_epi16 (_mm256_mullo_epi16 (s, opv), SMOL_OPACITY_SHIFT);

        a = _mm256_shuffle_epi8 (s, alpha_shuf);
        az = _mm256_cmpeq_epi16 (a, zero);   /* pixel-wide: all lanes match */

        /* t = dest * (0xff - a) + 128; dest' = src (squelched if a == 0)
         * + (t + (t >> 8)) >> 8 */
        t = _mm256_add_epi16 (_mm256_mullo_epi16 (d, _mm256_sub_epi16 (ff, a)), r128);
        u = _mm256_srli_epi16 (_mm256_add_epi16 (t, _mm256_srli_epi16 (t, 8)), 8);

        _mm256_storeu_si256 ((__m256i *) (dest_row + i),
                             _mm256_add_epi16 (_mm256_andnot_si256 (az, s), u));
    }

    /* Scalar epilogue for the last few pixels */

    for (i = n4; i < n_pixels; i++)
    {
        uint64_t s = src_row [i];
        uint64_t a, nz, t;

        if (scale_opacity)
            s = ((s * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ff00ff00ff00ffULL;

        a = s & 0xff;
        nz = (a + 0xffULL) >> 8;  /* 0 if a == 0, else 1 */

        t = dest_row [i] * (0xff - a) + 0x0080008000800080ULL;
        dest_row [i] = s * nz
            + (((t + ((t >> 8) & 0x00ff00ff00ff00ffULL)) >> 8) & 0x00ff00ff00ff00ffULL);
    }
}

static void
composite_over_dest_p16_128bpp_span (const uint64_t *src_row,
                                     uint64_t * SMOL_RESTRICT dest_row,
                                     uint32_t n_pixels,
                                     uint16_t opacity)
{
    const __m256i mask24 = _mm256_set1_epi32 (0x00ffffff);
    const __m256i ff = _mm256_set1_epi32 (0xff);
    const __m256i x100 = _mm256_set1_epi32 (0x100);
    const __m256i r128 = _mm256_set1_epi32 (128);
    const __m256i opv = _mm256_set1_epi32 (opacity);
    const SmolBool scale_opacity = (opacity < SMOL_OPACITY_MAX);
    uint32_t n2 = n_pixels & ~1U;  /* Whole pixel pairs */
    uint32_t i;

    /* Two pixels (4 x uint64_t = 8 x 32-bit lanes) per iteration. */

    for (i = 0; i < n2; i += 2)
    {
        __m256i s = _mm256_loadu_si256 ((const __m256i *) (src_row + (size_t) i * 2));
        __m256i d = _mm256_loadu_si256 ((const __m256i *) (dest_row + (size_t) i * 2));
        __m256i a, nz, w;

        if (scale_opacity)
            s = _mm256_and_si256 (_mm256_srli_epi32 (_mm256_mullo_epi32 (s, opv),
                                                     SMOL_OPACITY_SHIFT),
                                  mask24);

        /* Broadcast each pixel's source alpha across its four lanes. */
        a = _mm256_shuffle_epi32 (_mm256_srli_epi32 (s, 8), SMOL_4X2BIT (2, 2, 2, 2));
        a = _mm256_and_si256 (a, ff);

        /* nz = (a + 0xff) >> 8: 0 if a == 0, else 1. w = 0x100 - a - nz:
         * 256 when a == 0 so dest passes through bit-exactly, else 255 - a.
         * Must stay in lockstep with the generic implementation. */
        nz = _mm256_srli_epi32 (_mm256_add_epi32 (a, ff), 8);
        w = _mm256_sub_epi32 (x100, _mm256_add_epi32 (a, nz));

        /* dest = src * nz + (dest * w + 128) >> 8. */
        d = _mm256_and_si256 (_mm256_srli_epi32 (_mm256_add_epi32 (
                                  _mm256_mullo_epi32 (d, w), r128), 8),
                              mask24);
        d = _mm256_add_epi32 (_mm256_mullo_epi32 (s, nz), d);

        _mm256_storeu_si256 ((__m256i *) (dest_row + (size_t) i * 2), d);
    }

    /* Scalar epilogue for a final odd pixel */

    for (i = n2; i < n_pixels; i++)
    {
        uint64_t s0 = src_row [(size_t) i * 2];
        uint64_t s1 = src_row [(size_t) i * 2 + 1];
        uint64_t a, nz, w;

        if (scale_opacity)
        {
            s0 = ((s0 * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ffffff00ffffffULL;
            s1 = ((s1 * opacity) >> SMOL_OPACITY_SHIFT) & 0x00ffffff00ffffffULL;
        }

        a = (s1 >> 8) & 0xff;
        nz = (a + 0xffULL) >> 8;    /* 0 if a == 0, else 1 */
        w = 0x100 - a - nz;         /* 256 when a == 0, else 255 - a */

        dest_row [(size_t) i * 2] = s0 * nz
            + (((dest_row [(size_t) i * 2] * w + 0x0000008000000080ULL) >> 8)
               & 0x00ffffff00ffffffULL);
        dest_row [(size_t) i * 2 + 1] = s1 * nz
            + (((dest_row [(size_t) i * 2 + 1] * w + 0x0000008000000080ULL) >> 8)
               & 0x00ffffff00ffffffULL);
    }
}

static void
composite_over_dest_p16_128bpp (const uint64_t *src_row,
                                uint64_t * SMOL_RESTRICT dest_row,
                                uint32_t n_pixels,
                                uint16_t opacity)
{
    SMOL_COMPOSITE_OVER_DEST_BATCHED (composite_over_dest_p16_128bpp_span,
                                      smol_batch_alpha_class_128bpp (src_row + (size_t) i * 2,
                                                                     SMOL_ALPHA_MASK_INFLATED),
                                      2);
}

/* ---------------------- *
 * sRGB/linear conversion *
 * ---------------------- */

/* We unpack and pack by computing the gamma-2 curve in both directions
 * instead of gathering from the shared LUTs. There's an identity ramp in
 * low values to preserve reversibility.
 *
 * The forward curve is max ((c * c + 16) >> 5 + correction, c). */

static SMOL_INLINE __m256i
from_srgb_16x (__m256i c)
{
    __m256i m = _mm256_srli_epi16 (_mm256_add_epi16 (
        _mm256_mullo_epi16 (c, c), _mm256_set1_epi16 (16)), 5);

    m = _mm256_add_epi16 (m, _mm256_srli_epi16 (m, 7));
    return _mm256_max_epu16 (m, c);
}

/* The inverse curve is min (round (sqrt (l * 32530 / 1024)), l), which
 * reproduces _smol_to_srgb_lut exactly on all 2048 inputs. The min() is
 * the identity ramp. Input lanes must be within [0, 2047] (the packs mask
 * for this), output lanes are [0, 255] with the upper bits clear. */

static SMOL_INLINE __m256i
to_srgb_8x (__m256i l)
{
    __m256 x = _mm256_mul_ps (_mm256_cvtepi32_ps (l),
                              _mm256_set1_ps (32530.0f / 1024.0f));
    __m256 s = _mm256_add_ps (_mm256_sqrt_ps (x), _mm256_set1_ps (0.5f));

    return _mm256_min_epi32 (_mm256_cvttps_epi32 (s), l);
}

/* Four pixel-major ymm, two u64 words per pixel:
 * (hi0 << 32 | lo0), (hi1 << 32 | lo1)), stored to dest. */
static SMOL_INLINE void
store_8px_128bpp (uint64_t *dest, __m256i lo0, __m256i hi0, __m256i lo1, __m256i hi1)
{
    __m256i w0a = _mm256_unpacklo_epi32 (lo0, hi0);  /* px 0,1 | 4,5 */
    __m256i w0b = _mm256_unpackhi_epi32 (lo0, hi0);  /* px 2,3 | 6,7 */
    __m256i w1a = _mm256_unpacklo_epi32 (lo1, hi1);
    __m256i w1b = _mm256_unpackhi_epi32 (lo1, hi1);
    __m256i o0 = _mm256_unpacklo_epi64 (w0a, w1a);  /* px0 | px4 */
    __m256i o1 = _mm256_unpackhi_epi64 (w0a, w1a);  /* px1 | px5 */
    __m256i o2 = _mm256_unpacklo_epi64 (w0b, w1b);  /* px2 | px6 */
    __m256i o3 = _mm256_unpackhi_epi64 (w0b, w1b);  /* px3 | px7 */

    _mm256_storeu_si256 ((__m256i *) dest, _mm256_permute2x128_si256 (o0, o1, 0x20));
    _mm256_storeu_si256 ((__m256i *) dest + 1, _mm256_permute2x128_si256 (o2, o3, 0x20));
    _mm256_storeu_si256 ((__m256i *) dest + 2, _mm256_permute2x128_si256 (o0, o1, 0x31));
    _mm256_storeu_si256 ((__m256i *) dest + 3, _mm256_permute2x128_si256 (o2, o3, 0x31));
}

/* Load 8 pixel-major 128bpp pixels and return the four word vectors:
 * w0[ab] hold word 0 of px {0,1|4,5} / {2,3|6,7}, w1[ab] word 1. */
static SMOL_INLINE void
load_8px_128bpp (const uint64_t *src, __m256i *w0a, __m256i *w1a,
                 __m256i *w0b, __m256i *w1b)
{
    __m256i in01 = _mm256_loadu_si256 ((const __m256i *) src);
    __m256i in23 = _mm256_loadu_si256 ((const __m256i *) src + 1);
    __m256i in45 = _mm256_loadu_si256 ((const __m256i *) src + 2);
    __m256i in67 = _mm256_loadu_si256 ((const __m256i *) src + 3);
    __m256i q0 = _mm256_permute2x128_si256 (in01, in45, 0x20);
    __m256i q1 = _mm256_permute2x128_si256 (in01, in45, 0x31);
    __m256i q2 = _mm256_permute2x128_si256 (in23, in67, 0x20);
    __m256i q3 = _mm256_permute2x128_si256 (in23, in67, 0x31);

    *w0a = _mm256_unpacklo_epi64 (q0, q1);
    *w1a = _mm256_unpackhi_epi64 (q0, q1);
    *w0b = _mm256_unpacklo_epi64 (q2, q3);
    *w1b = _mm256_unpackhi_epi64 (q2, q3);
}

/* Unpremultiply eight 32-bit fields with a single 32-bit multiply.
 * lut32 carries the factor in every 32-bit field. */
static SMOL_INLINE __m256i
unpremul_word_8x_32 (__m256i w, __m256i lut32, int shift, uint32_t mask)
{
    return _mm256_and_si256 (
        _mm256_srli_epi32 (_mm256_mullo_epi32 (w, lut32), shift),
        _mm256_set1_epi32 (mask));
}

static SMOL_INLINE __m256i
order_ch_8x (int digit, __m256i c1, __m256i c2, __m256i c3, __m256i alpha)
{
    return digit == 1 ? c1 : digit == 2 ? c2 : digit == 3 ? c3 : alpha;
}

static SMOL_INLINE __m256i
pack_order_8x (__m256i c1, __m256i c2, __m256i c3, __m256i alpha,
               int a, int b, int c, int d)
{
    return _mm256_or_si256 (
        _mm256_or_si256 (
            _mm256_slli_epi32 (order_ch_8x (a, c1, c2, c3, alpha), 24),
            _mm256_slli_epi32 (order_ch_8x (b, c1, c2, c3, alpha), 16)),
        _mm256_or_si256 (
            _mm256_slli_epi32 (order_ch_8x (c, c1, c2, c3, alpha), 8),
            order_ch_8x (d, c1, c2, c3, alpha)));
}

static SMOL_INLINE uint32_t
order_ch_1x (int digit, uint32_t c1, uint32_t c2, uint32_t c3, uint32_t alpha)
{
    return digit == 1 ? c1 : digit == 2 ? c2 : digit == 3 ? c3 : alpha;
}

static SMOL_INLINE uint32_t
pack_order_1x (uint32_t c1, uint32_t c2, uint32_t c3, uint32_t alpha,
               int a, int b, int c, int d)
{
    return (order_ch_1x (a, c1, c2, c3, alpha) << 24)
        | (order_ch_1x (b, c1, c2, c3, alpha) << 16)
        | (order_ch_1x (c, c1, c2, c3, alpha) << 8)
        | order_ch_1x (d, c1, c2, c3, alpha);
}

/* Unpack 32bpp -> PREMUL16 LINEAR. The byte lanes b_hi0/b_lo0/b_hi1 give
 * the source bytes landing in the mid-order channel slots; a_shift the
 * alpha byte's position. Layouts follow the generic unpack helpers. */
static SMOL_INLINE void
unpack_u32_to_p16l (const uint32_t * SMOL_RESTRICT src_row,
                    uint64_t * SMOL_RESTRICT dest_row,
                    uint32_t n, int alpha_high)
{
    const __m256i m8 = _mm256_set1_epi32 (0xff);
    const __m256i m16 = _mm256_set1_epi32 (0xffff);
    const __m256i mbytes = _mm256_set1_epi32 (0x00ff00ff);
    const __m256i one = _mm256_set1_epi32 (1);
    uint32_t i = 0;

    for (; i + 8 <= n; i += 8)
    {
        __m256i p = _mm256_loadu_si256 ((const __m256i *) (src_row + i));
        __m256i fe = from_srgb_16x (_mm256_and_si256 (p, mbytes));
        __m256i fo = from_srgb_16x (_mm256_and_si256 (
            _mm256_srli_epi32 (p, 8), mbytes));
        __m256i al = alpha_high ? _mm256_srli_epi32 (p, 24)
                                : _mm256_and_si256 (p, m8);
        __m256i f_hi0 = alpha_high ? _mm256_srli_epi32 (fe, 16)   /* F(b2) */
                                   : _mm256_srli_epi32 (fo, 16);  /* F(b3) */
        __m256i f_lo0 = alpha_high ? _mm256_and_si256 (fo, m16)   /* F(b1) */
                                   : _mm256_srli_epi32 (fe, 16);  /* F(b2) */
        __m256i f_hi1 = alpha_high ? _mm256_and_si256 (fe, m16)   /* F(b0) */
                                   : _mm256_and_si256 (fo, m16);  /* F(b1) */
        __m256i ap1 = _mm256_add_epi32 (al, one);
        __m256i hi0 = _mm256_mullo_epi32 (f_hi0, ap1);
        __m256i lo0 = _mm256_mullo_epi32 (f_lo0, ap1);
        __m256i hi1 = _mm256_mullo_epi32 (f_hi1, ap1);
        __m256i lo1 = _mm256_or_si256 (_mm256_slli_epi32 (al, 8), m8);

        store_8px_128bpp (dest_row + (size_t) i * 2, lo0, hi0, lo1, hi1);
    }

    for ( ; i < n; i++)
    {
        uint32_t p = src_row [i];
        uint32_t alpha = alpha_high ? (p >> 24) : (p & 0xff);
        uint64_t h0 = _smol_from_srgb_lut [(p >> (alpha_high ? 16 : 24)) & 0xff] * (alpha + 1);
        uint64_t l0 = _smol_from_srgb_lut [(p >> (alpha_high ? 8 : 16)) & 0xff] * (alpha + 1);
        uint64_t h1 = _smol_from_srgb_lut [(p >> (alpha_high ? 0 : 8)) & 0xff] * (alpha + 1);

        dest_row [(size_t) i * 2] = (h0 << 32) | l0;
        dest_row [(size_t) i * 2 + 1] = (h1 << 32) | ((uint64_t) alpha << 8) | 0xff;
    }
}

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     1234, 128, 64, PREMUL16,     LINEAR) {
    unpack_u32_to_p16l (src_row, dest_row,
                        (uint32_t) ((dest_row_max - dest_row) / 2), FALSE);
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234,  32, 32, UNASSOCIATED, COMPRESSED,
                     2341, 128, 64, PREMUL16,     LINEAR) {
    unpack_u32_to_p16l (src_row, dest_row,
                        (uint32_t) ((dest_row_max - dest_row) / 2), TRUE);
} SMOL_REPACK_ROW_DEF_END

static SMOL_INLINE void
unpack_p24_to_p8l (const uint8_t * SMOL_RESTRICT src_row,
                   uint64_t * SMOL_RESTRICT dest_row,
                   uint32_t n, int to_3214)
{
    const __m256i sh0 = _mm256_setr_epi8 (0, -1, -1, -1, 3, -1, -1, -1,
                                          6, -1, -1, -1, 9, -1, -1, -1,
                                          0, -1, -1, -1, 3, -1, -1, -1,
                                          6, -1, -1, -1, 9, -1, -1, -1);
    const __m256i sh1 = _mm256_setr_epi8 (1, -1, -1, -1, 4, -1, -1, -1,
                                          7, -1, -1, -1, 10, -1, -1, -1,
                                          1, -1, -1, -1, 4, -1, -1, -1,
                                          7, -1, -1, -1, 10, -1, -1, -1);
    const __m256i sh2 = _mm256_setr_epi8 (2, -1, -1, -1, 5, -1, -1, -1,
                                          8, -1, -1, -1, 11, -1, -1, -1,
                                          2, -1, -1, -1, 5, -1, -1, -1,
                                          8, -1, -1, -1, 11, -1, -1, -1);
    const __m256i opaque = _mm256_set1_epi32 (0xffff);
    uint32_t i = 0;

    for ( ; i + 10 <= n; i += 8)
    {
        const uint8_t *s = src_row + (size_t) i * 3;
        __m256i p = _mm256_set_m128i (_mm_loadu_si128 ((const __m128i *) (s + 12)),
                                      _mm_loadu_si128 ((const __m128i *) s));
        __m256i c0 = _mm256_shuffle_epi8 (p, sh0);
        __m256i c1 = _mm256_shuffle_epi8 (p, sh1);
        __m256i c2 = _mm256_shuffle_epi8 (p, sh2);

        store_8px_128bpp (dest_row + (size_t) i * 2,
                          from_srgb_16x (c1),
                          from_srgb_16x (to_3214 ? c2 : c0),
                          opaque,
                          from_srgb_16x (to_3214 ? c0 : c2));
    }

    for ( ; i < n; i++)
    {
        const uint8_t *s = src_row + (size_t) i * 3;
        uint64_t h0 = _smol_from_srgb_lut [to_3214 ? s [2] : s [0]];
        uint64_t l0 = _smol_from_srgb_lut [s [1]];
        uint64_t h1 = _smol_from_srgb_lut [to_3214 ? s [0] : s [2]];

        dest_row [(size_t) i * 2] = (h0 << 32) | l0;
        dest_row [(size_t) i * 2 + 1] = (h1 << 32) | 0xffff;
    }
}

/* Unpack 32bpp PREMUL8 COMPRESSED -> PREMUL8 LINEAR: unpremultiply
 * (compressed), linearize, re-premultiply (linear). */
static SMOL_INLINE void
unpack_p32_to_p8l (const uint32_t * SMOL_RESTRICT src_row,
                   uint64_t * SMOL_RESTRICT dest_row,
                   uint32_t n, int alpha_high, int opaque)
{
    const __m256i m8 = _mm256_set1_epi32 (0xff);
    const __m256i m11 = _mm256_set1_epi32 (0x7ff);
    const __m256i one = _mm256_set1_epi32 (1);
    uint32_t i = 0;

    if (opaque)
    {
        const __m256i opaque_lane = _mm256_set1_epi32 (0xffff);

        for (; i + 8 <= n; i += 8)
        {
            __m256i p = _mm256_loadu_si256 ((const __m256i *) (src_row + i));
            __m256i b0 = _mm256_and_si256 (p, m8);
            __m256i b1 = _mm256_and_si256 (_mm256_srli_epi32 (p, 8), m8);
            __m256i b2 = _mm256_and_si256 (_mm256_srli_epi32 (p, 16), m8);
            __m256i b3 = _mm256_srli_epi32 (p, 24);

            store_8px_128bpp (dest_row + (size_t) i * 2,
                              from_srgb_16x (alpha_high ? b1 : b2),
                              from_srgb_16x (alpha_high ? b2 : b3),
                              opaque_lane,
                              from_srgb_16x (alpha_high ? b0 : b1));
        }

        for ( ; i < n; i++)
        {
            uint32_t p = src_row [i];
            uint64_t h0 = _smol_from_srgb_lut [(p >> (alpha_high ? 16 : 24)) & 0xff];
            uint64_t l0 = _smol_from_srgb_lut [(p >> (alpha_high ? 8 : 16)) & 0xff];
            uint64_t h1 = _smol_from_srgb_lut [(p >> (alpha_high ? 0 : 8)) & 0xff];

            dest_row [(size_t) i * 2] = (h0 << 32) | l0;
            dest_row [(size_t) i * 2 + 1] = (h1 << 32) | 0xffff;
        }

        return;
    }

    for (; i + 8 <= n; i += 8)
    {
        __m256i p = _mm256_loadu_si256 ((const __m256i *) (src_row + i));
        __m256i b0 = _mm256_and_si256 (p, m8);
        __m256i b1 = _mm256_and_si256 (_mm256_srli_epi32 (p, 8), m8);
        __m256i b2 = _mm256_and_si256 (_mm256_srli_epi32 (p, 16), m8);
        __m256i b3 = _mm256_srli_epi32 (p, 24);
        __m256i al = alpha_high ? b3 : b0;
        __m256i ch_hi0 = alpha_high ? b2 : b3;
        __m256i ch_lo0 = alpha_high ? b1 : b2;
        __m256i ch_hi1 = alpha_high ? b0 : b1;
        __m256i ap1 = _mm256_add_epi32 (al, one);
        __m256i lut = _mm256_i32gather_epi32 (
            (const int *) (const void *) _smol_inv_div_p8_lut, al, 4);
        __m256i u_hi0, u_lo0, u_hi1, hi0, lo0, hi1, lo1;

        u_hi0 = _mm256_and_si256 (_mm256_srli_epi32 (
            _mm256_mullo_epi32 (ch_hi0, lut), INVERTED_DIV_SHIFT_P8), m8);
        u_lo0 = _mm256_and_si256 (_mm256_srli_epi32 (
            _mm256_mullo_epi32 (ch_lo0, lut), INVERTED_DIV_SHIFT_P8), m8);
        u_hi1 = _mm256_and_si256 (_mm256_srli_epi32 (
            _mm256_mullo_epi32 (ch_hi1, lut), INVERTED_DIV_SHIFT_P8), m8);

        hi0 = _mm256_and_si256 (_mm256_srli_epi32 (
            _mm256_mullo_epi32 (from_srgb_16x (u_hi0), ap1), 8), m11);
        lo0 = _mm256_and_si256 (_mm256_srli_epi32 (
            _mm256_mullo_epi32 (from_srgb_16x (u_lo0), ap1), 8), m11);
        hi1 = _mm256_and_si256 (_mm256_srli_epi32 (
            _mm256_mullo_epi32 (from_srgb_16x (u_hi1), ap1), 8), m11);
        lo1 = _mm256_or_si256 (_mm256_slli_epi32 (al, 8), m8);

        store_8px_128bpp (dest_row + (size_t) i * 2, lo0, hi0, lo1, hi1);
    }

    for ( ; i < n; i++)
    {
        uint32_t p = src_row [i];
        uint32_t alpha = alpha_high ? (p >> 24) : (p & 0xff);
        uint32_t lut = _smol_inv_div_p8_lut [alpha];
        uint64_t h0 = ((p >> (alpha_high ? 16 : 24)) & 0xff);
        uint64_t l0 = ((p >> (alpha_high ? 8 : 16)) & 0xff);
        uint64_t h1 = ((p >> (alpha_high ? 0 : 8)) & 0xff);

        h0 = ((h0 * lut) >> INVERTED_DIV_SHIFT_P8) & 0xff;
        l0 = ((l0 * lut) >> INVERTED_DIV_SHIFT_P8) & 0xff;
        h1 = ((h1 * lut) >> INVERTED_DIV_SHIFT_P8) & 0xff;
        h0 = ((_smol_from_srgb_lut [h0] * (alpha + 1)) >> 8) & 0x7ff;
        l0 = ((_smol_from_srgb_lut [l0] * (alpha + 1)) >> 8) & 0x7ff;
        h1 = ((_smol_from_srgb_lut [h1] * (alpha + 1)) >> 8) & 0x7ff;

        dest_row [(size_t) i * 2] = (h0 << 32) | l0;
        dest_row [(size_t) i * 2 + 1] = (h1 << 32) | ((uint64_t) alpha << 8) | 0xff;
    }
}

#define UNPACK_P32_TO_P8L_BATCHED(alpha_high, alpha_ch) \
    SMOL_REPACK_BATCHED_2WAY (1, 2, \
        SMOL_BATCH_IS_OPAQUE_32BPP (src_row, \
                                    SMOL_32BPP_ALPHA_MASK (alpha_ch)), \
        unpack_p32_to_p8l (src_row, dest_row, n, alpha_high, TRUE), \
        unpack_p32_to_p8l (src_row, dest_row, n, alpha_high, FALSE))

SMOL_REPACK_ROW_DEF (1234,  32, 32, PREMUL8, COMPRESSED,
                     1234, 128, 64, PREMUL8, LINEAR) {
    UNPACK_P32_TO_P8L_BATCHED (FALSE, 4);
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234,  32, 32, PREMUL8, COMPRESSED,
                     2341, 128, 64, PREMUL8, LINEAR) {
    UNPACK_P32_TO_P8L_BATCHED (TRUE, 1);
} SMOL_REPACK_ROW_DEF_END

/* Serves both 128bpp p8l and p16l -> 32bpp u8 */
#define DEF_REPACK_PL_TO_U32(func_name, inv_div_lut, inv_div_shift, opaque_shift, \
                             transparent_keeps_color) \
static SMOL_INLINE void \
func_name (const uint64_t *src_row, \
           uint32_t * SMOL_RESTRICT dest_row, \
           uint32_t n, int a, int b, int c, int d, \
           SmolBatchOpacity batch_opacity) \
{ \
    const __m256i m8 = _mm256_set1_epi32 (0xff); \
    uint32_t i = 0; \
\
    for ( ; i + 8 <= n; i += 8) \
    { \
        __m256i w0a, w1a, w0b, w1b, alpha; \
        __m256i t0a, t1a, t0b, t1b, s0a, s1, s0b; \
        __m256i outa, outb; \
\
        load_8px_128bpp (src_row + (size_t) i * 2, &w0a, &w1a, &w0b, &w1b); \
\
        if (batch_opacity == SMOL_BATCH_TRANSPARENT) \
        { \
            const __m256i m11 = _mm256_set1_epi32 (0x7ff); \
            alpha = _mm256_setzero_si256 (); \
            if (transparent_keeps_color) \
            { \
                t0a = _mm256_and_si256 (w0a, m11); \
                t1a = _mm256_and_si256 (w1a, m11); \
                t0b = _mm256_and_si256 (w0b, m11); \
                t1b = _mm256_and_si256 (w1b, m11); \
            } \
            else \
            { \
                t0a = t1a = t0b = t1b = _mm256_setzero_si256 (); \
            } \
        } \
        else if (batch_opacity == SMOL_BATCH_OPAQUE) \
        { \
            const __m256i m11 = _mm256_set1_epi32 (0x7ff); \
            alpha = m8; \
            t0a = _mm256_and_si256 (_mm256_srli_epi32 (w0a, opaque_shift), m11); \
            t1a = _mm256_and_si256 (_mm256_srli_epi32 (w1a, opaque_shift), m11); \
            t0b = _mm256_and_si256 (_mm256_srli_epi32 (w0b, opaque_shift), m11); \
            t1b = _mm256_and_si256 (_mm256_srli_epi32 (w1b, opaque_shift), m11); \
        } \
        else \
        { \
            __m256i la, lb, lut, lut32_a, lut32_b; \
\
            la = _mm256_shuffle_epi32 (w1a, 0x88); \
            lb = _mm256_shuffle_epi32 (w1b, 0x88); \
            alpha = _mm256_and_si256 (_mm256_srli_epi32 ( \
                _mm256_blend_epi32 (la, lb, 0xcc), 8), m8); \
\
            lut = _mm256_i32gather_epi32 ( \
                (const int *) (const void *) inv_div_lut, alpha, 4); \
            lut32_a = _mm256_unpacklo_epi32 (lut, lut); \
            lut32_b = _mm256_unpackhi_epi32 (lut, lut); \
\
            t0a = unpremul_word_8x_32 (w0a, lut32_a, inv_div_shift, 0x7ff); \
            t1a = unpremul_word_8x_32 (w1a, lut32_a, inv_div_shift, 0x7ff); \
            t0b = unpremul_word_8x_32 (w0b, lut32_b, inv_div_shift, 0x7ff); \
            t1b = unpremul_word_8x_32 (w1b, lut32_b, inv_div_shift, 0x7ff); \
        } \
\
        s0a = to_srgb_8x (t0a); \
        s0b = to_srgb_8x (t0b); \
        s1 = to_srgb_8x (_mm256_blend_epi32 ( \
            _mm256_shuffle_epi32 (t1a, 0xdd), \
            _mm256_shuffle_epi32 (t1b, 0xdd), 0xcc)); \
\
        outa = pack_order_8x (_mm256_shuffle_epi32 (s0a, 0xdd), \
                              _mm256_shuffle_epi32 (s0a, 0x88), \
                              s1, alpha, a, b, c, d); \
        outb = pack_order_8x (_mm256_shuffle_epi32 (s0b, 0xdd), \
                              _mm256_shuffle_epi32 (s0b, 0x88), \
                              _mm256_shuffle_epi32 (s1, 0xee), \
                              _mm256_shuffle_epi32 (alpha, 0xee), \
                              a, b, c, d); \
\
        _mm256_storeu_si256 ((__m256i *) (dest_row + i), \
                             _mm256_unpacklo_epi64 (outa, outb)); \
    } \
\
    for ( ; i < n; i++) \
    { \
        const uint64_t *s = src_row + (size_t) i * 2; \
        uint32_t alpha = (uint8_t) (s [1] >> 8); \
        uint32_t lut = (batch_opacity == SMOL_BATCH_OPAQUE) \
            ? (1U << ((inv_div_shift) - (opaque_shift))) \
            : inv_div_lut [alpha]; \
        uint64_t t0 = ((s [0] * lut) >> inv_div_shift) & 0x000007ff000007ffULL; \
        uint64_t t1 = ((s [1] * lut) >> inv_div_shift) & 0x000007ff000007ffULL; \
        uint32_t c1 = _smol_to_srgb_lut [t0 >> 32]; \
        uint32_t c2 = _smol_to_srgb_lut [t0 & 0xffff]; \
        uint32_t c3 = _smol_to_srgb_lut [t1 >> 32]; \
\
        dest_row [i] = pack_order_1x (c1, c2, c3, alpha, a, b, c, d); \
    } \
}

DEF_REPACK_PL_TO_U32(repack_p16l_to_u32, _smol_inv_div_p16l_lut,
                     INVERTED_DIV_SHIFT_P16L, 8, 1)
DEF_REPACK_PL_TO_U32(repack_p8l_to_u32, _smol_inv_div_p8l_lut,
                     INVERTED_DIV_SHIFT_P8L, 0, 0)

#define PACK_P16L_TO_U32_BATCHED(a, b, c, d) \
    SMOL_REPACK_BATCH_LOOP (2, 1, \
        smol_batch_alpha_class_128bpp (src_row, SMOL_ALPHA_MASK_INFLATED), \
        repack_p16l_to_u32 (src_row, dest_row, n, (a), (b), (c), (d), batch_opacity))

#define DEF_PACK_P16L_TO_U32_ROW(a, b, c, d) \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL16,     LINEAR, \
                         a##b##c##d,  32, 32, UNASSOCIATED, COMPRESSED) { \
        PACK_P16L_TO_U32_BATCHED ((a), (b), (c), (d)); \
    } SMOL_REPACK_ROW_DEF_END

DEF_PACK_P16L_TO_U32_ROW (1, 2, 3, 4)
DEF_PACK_P16L_TO_U32_ROW (3, 2, 1, 4)
DEF_PACK_P16L_TO_U32_ROW (4, 1, 2, 3)
DEF_PACK_P16L_TO_U32_ROW (4, 3, 2, 1)

#define PACK_P8L_TO_U32_BATCHED(a, b, c, d) \
    SMOL_REPACK_BATCH_LOOP (2, 1, \
        smol_batch_alpha_class_128bpp (src_row, SMOL_ALPHA_MASK_INFLATED), \
        repack_p8l_to_u32 (src_row, dest_row, n, (a), (b), (c), (d), batch_opacity))

#define PACK_P8L_TO_P32_BATCHED(a, b, c, d) \
    SMOL_REPACK_BATCHED_3WAY (2, 1, \
        smol_batch_alpha_class_128bpp (src_row, SMOL_ALPHA_MASK_INFLATED), \
        n * sizeof (uint32_t), \
        repack_p8l_to_p32 (src_row, dest_row, n, (a), (b), (c), (d), TRUE), \
        repack_p8l_to_p32 (src_row, dest_row, n, (a), (b), (c), (d), FALSE))

/* PREMUL8 LINEAR -> 32bpp PREMUL8 COMPRESSED. */
static SMOL_INLINE void
repack_p8l_to_p32 (const uint64_t *src_row,
                   uint32_t * SMOL_RESTRICT dest_row,
                   uint32_t n, int a, int b, int c, int d, int batch_is_opaque)
{
    const __m256i m8 = _mm256_set1_epi32 (0xff);
    const __m256i one16 = _mm256_set1_epi16 (1);
    uint32_t i = 0;

    for ( ; i + 8 <= n; i += 8)
    {
        __m256i w0a, w1a, w0b, w1b, alpha;
        __m256i t0a, t1a, t0b, t1b, t1;
        __m256i ap1_a, ap1_b, outa, outb;

        load_8px_128bpp (src_row + (size_t) i * 2, &w0a, &w1a, &w0b, &w1b);

        if (batch_is_opaque)
        {
            alpha = m8;
            t0a = w0a; t1a = w1a; t0b = w0b; t1b = w1b;
        }
        else
        {
            __m256i la, lb, lut, lut32_a, lut32_b;

            la = _mm256_shuffle_epi32 (w1a, 0x88);
            lb = _mm256_shuffle_epi32 (w1b, 0x88);
            alpha = _mm256_and_si256 (_mm256_srli_epi32 (
                _mm256_blend_epi32 (la, lb, 0xcc), 8), m8);

            lut = _mm256_i32gather_epi32 (
                (const int *) (const void *) _smol_inv_div_p8l_lut, alpha, 4);
            lut32_a = _mm256_unpacklo_epi32 (lut, lut);
            lut32_b = _mm256_unpackhi_epi32 (lut, lut);

            t0a = unpremul_word_8x_32 (w0a, lut32_a, INVERTED_DIV_SHIFT_P8L, 0x7ff);
            t1a = unpremul_word_8x_32 (w1a, lut32_a, INVERTED_DIV_SHIFT_P8L, 0x7ff);
            t0b = unpremul_word_8x_32 (w0b, lut32_b, INVERTED_DIV_SHIFT_P8L, 0x7ff);
            t1b = unpremul_word_8x_32 (w1b, lut32_b, INVERTED_DIV_SHIFT_P8L, 0x7ff);
        }

        t0a = to_srgb_8x (t0a);
        t0b = to_srgb_8x (t0b);
        t1 = to_srgb_8x (_mm256_blend_epi32 (
            _mm256_shuffle_epi32 (t1a, 0xDD),
            _mm256_shuffle_epi32 (t1b, 0xDD), 0xCC));

        /* Re-premultiply (compressed): ((c + 1) * (alpha + 1) - 1) >> 8,
         * in 16-bit lanes: the one overflowing product, 256 * 256, wraps
         * to 0 and borrows to 0xffff, which is what we want. The u32
         * lanes' high halves come out zero, so no masking is needed. */

        if (!batch_is_opaque)
        {
            ap1_a = _mm256_add_epi16 (_mm256_shuffle_epi32 (alpha, 0x50), one16);
            ap1_b = _mm256_add_epi16 (_mm256_shuffle_epi32 (alpha, 0xfa), one16);

            t0a = _mm256_srli_epi16 (_mm256_sub_epi16 (_mm256_mullo_epi16 (
                _mm256_add_epi16 (t0a, one16), ap1_a), one16), 8);
            t0b = _mm256_srli_epi16 (_mm256_sub_epi16 (_mm256_mullo_epi16 (
                _mm256_add_epi16 (t0b, one16), ap1_b), one16), 8);
            t1 = _mm256_srli_epi16 (_mm256_sub_epi16 (_mm256_mullo_epi16 (
                _mm256_add_epi16 (t1, one16),
                _mm256_add_epi16 (alpha, one16)), one16), 8);
        }

        outa = pack_order_8x (_mm256_shuffle_epi32 (t0a, 0xDD),
                              _mm256_shuffle_epi32 (t0a, 0x88),
                              t1, alpha, a, b, c, d);
        outb = pack_order_8x (_mm256_shuffle_epi32 (t0b, 0xDD),
                              _mm256_shuffle_epi32 (t0b, 0x88),
                              _mm256_shuffle_epi32 (t1, 0xEE),
                              _mm256_shuffle_epi32 (alpha, 0xee),
                              a, b, c, d);

        _mm256_storeu_si256 ((__m256i *) (dest_row + i),
                             _mm256_unpacklo_epi64 (outa, outb));
    }

    for ( ; i < n; i++)
    {
        const uint64_t *s = src_row + (size_t) i * 2;
        uint32_t alpha = batch_is_opaque ? 0xff : (uint8_t) (s [1] >> 8);
        uint32_t lut = batch_is_opaque ? (1U << INVERTED_DIV_SHIFT_P8L)
            : _smol_inv_div_p8l_lut [alpha];
        uint64_t t0 = ((s [0] * lut) >> INVERTED_DIV_SHIFT_P8L) & 0x000007ff000007ffULL;
        uint64_t t1 = ((s [1] * lut) >> INVERTED_DIV_SHIFT_P8L) & 0x000007ff000007ffULL;
        uint32_t c1 = _smol_to_srgb_lut [t0 >> 32];
        uint32_t c2 = _smol_to_srgb_lut [t0 & 0xffff];
        uint32_t c3 = _smol_to_srgb_lut [t1 >> 32];

        if (!batch_is_opaque)
        {
            c1 = (((c1 + 1) * (alpha + 1) - 1) >> 8) & 0xff;
            c2 = (((c2 + 1) * (alpha + 1) - 1) >> 8) & 0xff;
            c3 = (((c3 + 1) * (alpha + 1) - 1) >> 8) & 0xff;
        }

        dest_row [i] = pack_order_1x (c1, c2, c3, alpha, a, b, c, d);
    }
}

#define DEF_PACK_P8L_TO_P32_ROW(a, b, c, d) \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL8, LINEAR, \
                         a##b##c##d,  32, 32, PREMUL8, COMPRESSED) { \
        PACK_P8L_TO_P32_BATCHED ((a), (b), (c), (d)); \
    } SMOL_REPACK_ROW_DEF_END

DEF_PACK_P8L_TO_P32_ROW (1, 2, 3, 4)
DEF_PACK_P8L_TO_P32_ROW (3, 2, 1, 4)
DEF_PACK_P8L_TO_P32_ROW (4, 1, 2, 3)
DEF_PACK_P8L_TO_P32_ROW (4, 3, 2, 1)

#define PACK_P8L_TO_P24_BATCHED(to_321) \
    SMOL_REPACK_BATCHED_3WAY (2, 3, \
        smol_batch_alpha_class_128bpp (src_row, SMOL_ALPHA_MASK_INFLATED), \
        n * 3, \
        repack_p8l_to_p24 (src_row, dest_row, n, to_321, TRUE), \
        repack_p8l_to_p24 (src_row, dest_row, n, to_321, FALSE))

/* PREMUL8 LINEAR -> 24bpp PREMUL8 COMPRESSED, 123 or 321 byte order. */
static SMOL_INLINE void
repack_p8l_to_p24 (const uint64_t *src_row,
                        uint8_t * SMOL_RESTRICT dest_row,
                        uint32_t n, int to_321, int opaque)
{
    const __m256i m8 = _mm256_set1_epi32 (0xff);
    const __m256i one16 = _mm256_set1_epi16 (1);
    const __m256i drop_alpha = to_321
        ? _mm256_setr_epi8 (2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12,
                            -1, -1, -1, -1,
                            2, 1, 0, 6, 5, 4, 10, 9, 8, 14, 13, 12,
                            -1, -1, -1, -1)
        : _mm256_setr_epi8 (0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14,
                            -1, -1, -1, -1,
                            0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13, 14,
                            -1, -1, -1, -1);
    const __m256i close_gap = _mm256_setr_epi32 (0, 1, 2, 4, 5, 6, 6, 6);
    uint32_t i = 0;

    for (; i + 8 <= n; i += 8)
    {
        __m256i w0a, w1a, w0b, w1b, alpha;
        __m256i t0a, t1a, t0b, t1b, t1;
        __m256i ap1_a, ap1_b, outa, outb, out;

        load_8px_128bpp (src_row + (size_t) i * 2, &w0a, &w1a, &w0b, &w1b);

        if (opaque)
        {
            alpha = m8;
            t0a = w0a; t1a = w1a; t0b = w0b; t1b = w1b;
        }
        else
        {
            __m256i la, lb, lut, lut32_a, lut32_b;

            la = _mm256_shuffle_epi32 (w1a, 0x88);
            lb = _mm256_shuffle_epi32 (w1b, 0x88);
            alpha = _mm256_and_si256 (_mm256_srli_epi32 (
                _mm256_blend_epi32 (la, lb, 0xcc), 8), m8);

            lut = _mm256_i32gather_epi32 (
                (const int *) (const void *) _smol_inv_div_p8l_lut, alpha, 4);
            lut32_a = _mm256_unpacklo_epi32 (lut, lut);
            lut32_b = _mm256_unpackhi_epi32 (lut, lut);

            t0a = unpremul_word_8x_32 (w0a, lut32_a, INVERTED_DIV_SHIFT_P8L, 0x7ff);
            t1a = unpremul_word_8x_32 (w1a, lut32_a, INVERTED_DIV_SHIFT_P8L, 0x7ff);
            t0b = unpremul_word_8x_32 (w0b, lut32_b, INVERTED_DIV_SHIFT_P8L, 0x7ff);
            t1b = unpremul_word_8x_32 (w1b, lut32_b, INVERTED_DIV_SHIFT_P8L, 0x7ff);
        }

        t0a = to_srgb_8x (t0a);
        t0b = to_srgb_8x (t0b);
        t1 = to_srgb_8x (_mm256_blend_epi32 (
            _mm256_shuffle_epi32 (t1a, 0xdd),
            _mm256_shuffle_epi32 (t1b, 0xdd), 0xcc));

        if (!opaque)
        {
            ap1_a = _mm256_add_epi16 (_mm256_shuffle_epi32 (alpha, 0x50), one16);
            ap1_b = _mm256_add_epi16 (_mm256_shuffle_epi32 (alpha, 0xfa), one16);

            t0a = _mm256_srli_epi16 (_mm256_sub_epi16 (_mm256_mullo_epi16 (
                _mm256_add_epi16 (t0a, one16), ap1_a), one16), 8);
            t0b = _mm256_srli_epi16 (_mm256_sub_epi16 (_mm256_mullo_epi16 (
                _mm256_add_epi16 (t0b, one16), ap1_b), one16), 8);
            t1 = _mm256_srli_epi16 (_mm256_sub_epi16 (_mm256_mullo_epi16 (
                _mm256_add_epi16 (t1, one16),
                _mm256_add_epi16 (alpha, one16)), one16), 8);
        }

        outa = pack_order_8x (_mm256_shuffle_epi32 (t0a, 0xdd),
                              _mm256_shuffle_epi32 (t0a, 0x88),
                              t1, alpha, 4, 3, 2, 1);
        outb = pack_order_8x (_mm256_shuffle_epi32 (t0b, 0xdd),
                              _mm256_shuffle_epi32 (t0b, 0x88),
                              _mm256_shuffle_epi32 (t1, 0xee),
                              _mm256_shuffle_epi32 (alpha, 0xee),
                              4, 3, 2, 1);

        out = _mm256_unpacklo_epi64 (outa, outb);
        out = _mm256_shuffle_epi8 (out, drop_alpha);
        out = _mm256_permutevar8x32_epi32 (out, close_gap);

        _mm_storeu_si128 ((__m128i *) (dest_row + (size_t) i * 3),
                          _mm256_castsi256_si128 (out));
        _mm_storel_epi64 ((__m128i *) (dest_row + (size_t) i * 3 + 16),
                          _mm256_extracti128_si256 (out, 1));
    }

    for ( ; i < n; i++)
    {
        const uint64_t *s = src_row + (size_t) i * 2;
        uint8_t *d = dest_row + (size_t) i * 3;
        uint32_t alpha = opaque ? 0xff : (uint8_t) (s [1] >> 8);
        uint32_t lut = opaque ? (1U << INVERTED_DIV_SHIFT_P8L)
                              : _smol_inv_div_p8l_lut [alpha];
        uint64_t t0 = ((s [0] * lut) >> INVERTED_DIV_SHIFT_P8L) & 0x000007ff000007ffULL;
        uint64_t t1 = ((s [1] * lut) >> INVERTED_DIV_SHIFT_P8L) & 0x000007ff000007ffULL;
        uint32_t c1 = _smol_to_srgb_lut [t0 >> 32];
        uint32_t c2 = _smol_to_srgb_lut [t0 & 0xffff];
        uint32_t c3 = _smol_to_srgb_lut [t1 >> 32];

        if (!opaque)
        {
            c1 = (((c1 + 1) * (alpha + 1) - 1) >> 8) & 0xff;
            c2 = (((c2 + 1) * (alpha + 1) - 1) >> 8) & 0xff;
            c3 = (((c3 + 1) * (alpha + 1) - 1) >> 8) & 0xff;
        }

        d [0] = to_321 ? c3 : c1;
        d [1] = c2;
        d [2] = to_321 ? c1 : c3;
    }
}

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL8, LINEAR,
                     123,   24,  8, PREMUL8, COMPRESSED) {
    PACK_P8L_TO_P24_BATCHED (FALSE);
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (1234, 128, 64, PREMUL8, LINEAR,
                     321,   24,  8, PREMUL8, COMPRESSED) {
    PACK_P8L_TO_P24_BATCHED (TRUE);
} SMOL_REPACK_ROW_DEF_END

#define DEF_PACK_P8L_TO_U32_ROW(a, b, c, d) \
    SMOL_REPACK_ROW_DEF (1234,       128, 64, PREMUL8,      LINEAR, \
                         a##b##c##d,  32, 32, UNASSOCIATED, COMPRESSED) { \
        PACK_P8L_TO_U32_BATCHED ((a), (b), (c), (d)); \
    } SMOL_REPACK_ROW_DEF_END

DEF_PACK_P8L_TO_U32_ROW (1, 2, 3, 4)
DEF_PACK_P8L_TO_U32_ROW (3, 2, 1, 4)
DEF_PACK_P8L_TO_U32_ROW (4, 1, 2, 3)
DEF_PACK_P8L_TO_U32_ROW (4, 3, 2, 1)

SMOL_REPACK_ROW_DEF (123,   24,  8, PREMUL8, COMPRESSED,
                     1234, 128, 64, PREMUL8, LINEAR) {
    unpack_p24_to_p8l (src_row, dest_row,
                       (uint32_t) ((dest_row_max - dest_row) / 2), FALSE);
} SMOL_REPACK_ROW_DEF_END

SMOL_REPACK_ROW_DEF (123,   24,  8, PREMUL8, COMPRESSED,
                     3214, 128, 64, PREMUL8, LINEAR) {
    unpack_p24_to_p8l (src_row, dest_row,
                       (uint32_t) ((dest_row_max - dest_row) / 2), TRUE);
} SMOL_REPACK_ROW_DEF_END

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

    R (123,   24, PREMUL8,      COMPRESSED, 1234, 128, PREMUL8,       LINEAR),
    R (123,   24, PREMUL8,      COMPRESSED, 3214, 128, PREMUL8,       LINEAR),

    R (1234,  32, UNASSOCIATED, COMPRESSED, 1234, 128, PREMUL16,      LINEAR),
    R (1234,  32, UNASSOCIATED, COMPRESSED, 2341, 128, PREMUL16,      LINEAR),
    R (1234,  32, PREMUL8,      COMPRESSED, 1234, 128, PREMUL8,       LINEAR),
    R (1234,  32, PREMUL8,      COMPRESSED, 2341, 128, PREMUL8,       LINEAR),

    R (1234, 128, PREMUL16,     LINEAR,     1234,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     LINEAR,     3214,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     LINEAR,     4123,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL16,     LINEAR,     4321,  32, UNASSOCIATED,  COMPRESSED),

    R (1234, 128, PREMUL8,      LINEAR,     1234,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     3214,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     4123,  32, UNASSOCIATED,  COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     4321,  32, UNASSOCIATED,  COMPRESSED),

    R (1234, 128, PREMUL8,      LINEAR,     1234,  32, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     3214,  32, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     4123,  32, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     4321,  32, PREMUL8,       COMPRESSED),

    R (1234, 128, PREMUL8,      LINEAR,     123,   24, PREMUL8,       COMPRESSED),
    R (1234, 128, PREMUL8,      LINEAR,     321,   24, PREMUL8,       COMPRESSED),

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
            scale_dest_row_box_128bpp
        }
    },
    {
        /* Composite over color */

        { { NULL, NULL, NULL }, { NULL, NULL, NULL } },  /* 24bpp - unused */
        { { NULL, NULL, NULL }, { NULL, NULL, NULL } },  /* 32bpp - unused */

        /* 64bpp: p8 compressed. The linear row is unreachable: linear
         * gamma always selects 128bpp storage. */
        {
            { NULL, composite_over_color_p8_64bpp, NULL },  /* compressed */
            { NULL, NULL, NULL }                            /* linear - unused */
        },

        /* 128bpp: p16 in both gammas */
        {
            { NULL, NULL, composite_over_color_p16_128bpp },  /* compressed */
            { NULL, composite_over_color_p16_128bpp, composite_over_color_p16_128bpp }  /* linear */
        }
    },
    {
        /* Composite over color, keeping source alpha */

        { { NULL, NULL, NULL }, { NULL, NULL, NULL } },  /* 24bpp - unused */
        { { NULL, NULL, NULL }, { NULL, NULL, NULL } },  /* 32bpp - unused */

        /* 64bpp: p8 compressed. The linear row is unreachable: linear
         * gamma always selects 128bpp storage. */
        {
            { NULL, composite_over_color_src_alpha_p8_64bpp, NULL },  /* compressed */
            { NULL, NULL, NULL }                                      /* linear - unused */
        },

        /* 128bpp: p8 and p16 compressed, p8l and p16l linear */
        {
            /* compressed */
            {
                NULL,  /* unassociated - unused */
                composite_over_color_src_alpha_p8_128bpp,
                composite_over_color_src_alpha_p16_128bpp
            },
            /* linear (p8l has its own channel scale, p16l shares p16's) */
            {
                NULL,  /* unassociated - unused */
                composite_over_color_src_alpha_p8l_128bpp,
                composite_over_color_src_alpha_p16_128bpp
            }
        }
    },
    {
        /* Composite over dest */

        { { NULL, NULL, NULL }, { NULL, NULL, NULL } },  /* 24bpp - unused */
        { { NULL, NULL, NULL }, { NULL, NULL, NULL } },  /* 32bpp - unused */

        /* 64bpp: p8 compressed. The linear row is unreachable: linear
         * gamma always selects 128bpp storage. */
        {
            { NULL, composite_over_dest_p8_64bpp, NULL },  /* compressed */
            { NULL, NULL, NULL }                           /* linear - unused */
        },

        /* 128bpp: p16 in both gammas */
        {
            { NULL, NULL, composite_over_dest_p16_128bpp },  /* compressed */
            { NULL, composite_over_dest_p16_128bpp, composite_over_dest_p16_128bpp }  /* linear */
        }
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
