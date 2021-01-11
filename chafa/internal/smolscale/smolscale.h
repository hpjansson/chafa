/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright © 2019-2021 Hans Petter Jansson. See COPYING for details. */

#include <stdint.h>

#ifndef _SMOLSCALE_H_
#define _SMOLSCALE_H_

#ifdef __cplusplus
extern "C" {
#endif

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

typedef void (SmolPostRowFunc) (uint32_t *row_inout,
                                int width,
                                void *user_data);

typedef struct SmolScaleCtx SmolScaleCtx;

/* Simple API: Scales an entire image in one shot. You must provide pointers to
 * the source memory and an existing allocation to receive the output data.
 * This interface can only be used from a single thread. */

void smol_scale_simple (SmolPixelType pixel_type_in, const uint32_t *pixels_in,
                        uint32_t width_in, uint32_t height_in, uint32_t rowstride_in,
                        SmolPixelType pixel_type_out, uint32_t *pixels_out,
                        uint32_t width_out, uint32_t height_out, uint32_t rowstride_out);

/* Batch API: Allows scaling a few rows at a time. Suitable for multithreading. */

SmolScaleCtx *smol_scale_new (SmolPixelType pixel_type_in, const uint32_t *pixels_in,
                              uint32_t width_in, uint32_t height_in, uint32_t rowstride_in,
                              SmolPixelType pixel_type_out, uint32_t *pixels_out,
                              uint32_t width_out, uint32_t height_out, uint32_t rowstride_out);

SmolScaleCtx *smol_scale_new_full (SmolPixelType pixel_type_in, const uint32_t *pixels_in,
                                   uint32_t width_in, uint32_t height_in, uint32_t rowstride_in,
                                   SmolPixelType pixel_type_out, uint32_t *pixels_out,
                                   uint32_t width_out, uint32_t height_out, uint32_t rowstride_out,
                                   SmolPostRowFunc post_row_func, void *user_data);

void smol_scale_destroy (SmolScaleCtx *scale_ctx);

/* It's ok to call smol_scale_batch() without locking from multiple concurrent
 * threads, as long as the outrows do not overlap. Make sure all workers are
 * finished before you call smol_scale_destroy(). */

void smol_scale_batch (const SmolScaleCtx *scale_ctx, uint32_t first_outrow, uint32_t n_outrows);

/* Like smol_scale_batch(), but will write the output rows to outrows_dest
 * instead of relative to pixels_out address handed to smol_scale_new(). The
 * other parameters from init (size, rowstride, etc) will still be used. */

void smol_scale_batch_full (const SmolScaleCtx *scale_ctx,
                            void *outrows_dest,
                            uint32_t first_outrow, uint32_t n_outrows);

#ifdef __cplusplus
}
#endif

#endif
