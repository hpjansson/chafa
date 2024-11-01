/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright © 2019-2024 Hans Petter Jansson. See COPYING for details. */

#include <stdint.h>

#ifndef _SMOLSCALE_H_
#define _SMOLSCALE_H_

#ifdef __cplusplus
extern "C" {
#endif

#define SMOL_SUBPIXEL_SHIFT 8
#define SMOL_SUBPIXEL_MUL (1 << (SMOL_SUBPIXEL_SHIFT))

/* Applies modulo twice, yielding a positive fraction for negative offsets */
#define SMOL_SUBPIXEL_MOD(n) ((((n) % SMOL_SUBPIXEL_MUL) + SMOL_SUBPIXEL_MUL) % SMOL_SUBPIXEL_MUL)

#define SMOL_PX_TO_SPX(px) ((px) * (SMOL_SUBPIXEL_MUL))
#define SMOL_SPX_TO_PX(spx) (((spx) + (SMOL_SUBPIXEL_MUL) - 1) / (SMOL_SUBPIXEL_MUL))

typedef enum
{
    SMOL_NO_FLAGS                   = 0,
    SMOL_DISABLE_ACCELERATION       = (1 << 0),
    SMOL_DISABLE_SRGB_LINEARIZATION = (1 << 1)
}
SmolFlags;

typedef enum
{
    /* 32 bits per pixel */

    SMOL_PIXEL_RGBA8_PREMULTIPLIED,
    SMOL_PIXEL_BGRA8_PREMULTIPLIED,
    SMOL_PIXEL_ARGB8_PREMULTIPLIED,
    SMOL_PIXEL_ABGR8_PREMULTIPLIED,

    SMOL_PIXEL_RGBA8_UNASSOCIATED,
    SMOL_PIXEL_BGRA8_UNASSOCIATED,
    SMOL_PIXEL_ARGB8_UNASSOCIATED,
    SMOL_PIXEL_ABGR8_UNASSOCIATED,

    /* 24 bits per pixel */

    SMOL_PIXEL_RGB8,
    SMOL_PIXEL_BGR8,

    SMOL_PIXEL_MAX
}
SmolPixelType;

typedef enum
{
    SMOL_COMPOSITE_SRC,
    SMOL_COMPOSITE_SRC_CLEAR_DEST,
    SMOL_COMPOSITE_SRC_OVER_DEST
}
SmolCompositeOp;

typedef void (SmolPostRowFunc) (void *row_inout,
                                int width,
                                void *user_data);

typedef struct SmolScaleCtx SmolScaleCtx;

/* Simple API: Scales an entire image in one shot. You must provide pointers to
 * the source memory and an existing allocation to receive the output data.
 * This interface can only be used from a single thread. */

void smol_scale_simple (const void *src_pixels,
                        SmolPixelType src_pixel_type,
                        uint32_t src_width,
                        uint32_t src_height,
                        uint32_t src_rowstride,
                        void *dest_pixels,
                        SmolPixelType dest_pixel_type,
                        uint32_t dest_width,
                        uint32_t dest_height,
                        uint32_t dest_rowstride,
                        SmolFlags flags);

/* Batch API: Allows scaling a few rows at a time. Suitable for multithreading. */

SmolScaleCtx *smol_scale_new_simple (const void *src_pixels,
                                     SmolPixelType src_pixel_type,
                                     uint32_t src_width,
                                     uint32_t src_height,
                                     uint32_t src_rowstride,
                                     void *dest_pixels,
                                     SmolPixelType dest_pixel_type,
                                     uint32_t dest_width,
                                     uint32_t dest_height,
                                     uint32_t dest_rowstride,
                                     SmolFlags flags);

SmolScaleCtx *smol_scale_new_full (const void *src_pixels,
                                   SmolPixelType src_pixel_type,
                                   uint32_t src_width,
                                   uint32_t src_height,
                                   uint32_t src_rowstride,
                                   const void *color_pixel,
                                   SmolPixelType color_pixel_type,
                                   void *dest_pixels,
                                   SmolPixelType dest_pixel_type,
                                   uint32_t dest_width,
                                   uint32_t dest_height,
                                   uint32_t dest_rowstride,
                                   int32_t placement_x,
                                   int32_t placement_y,
                                   uint32_t placement_width,
                                   uint32_t placement_height,
                                   SmolCompositeOp composite_op,
                                   SmolFlags flags,
                                   SmolPostRowFunc post_row_func,
                                   void *user_data);

void smol_scale_destroy (SmolScaleCtx *scale_ctx);

/* It's ok to call smol_scale_batch() without locking from multiple concurrent
 * threads, as long as the outrows do not overlap. Make sure all workers are
 * finished before you call smol_scale_destroy(). */

void smol_scale_batch (const SmolScaleCtx *scale_ctx, int32_t first_outrow, int32_t n_outrows);

/* Like smol_scale_batch(), but will write the output rows to outrows_dest
 * instead of relative to pixels_out address handed to smol_scale_new(). The
 * other parameters from init (size, rowstride, etc) will still be used. */

void smol_scale_batch_full (const SmolScaleCtx *scale_ctx,
                            void *outrows_dest,
                            int32_t first_outrow, int32_t n_outrows);

#ifdef __cplusplus
}
#endif

#endif
