/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2022 Hans Petter Jansson
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

#include "smolscale/smolscale.h"
#include "chafa.h"
#include "internal/chafa-batch.h"
#include "internal/chafa-private.h"

typedef struct
{
    ChafaIndexedImage *indexed_image;
    ChafaColorSpace color_space;
    ChafaPixelType src_pixel_type;
    gconstpointer src_pixels;
    gint src_width, src_height, src_rowstride;
    gint dest_width, dest_height;

    SmolScaleCtx *scale_ctx;
    guint32 *scaled_data;

    /* BG color with alpha multiplier 255-0 */
    guint32 bg_color_lut [256];
}
DrawPixelsCtx;

static void
gen_color_lut_rgba8 (guint32 *color_lut, ChafaColor col)
{
    gint i;

    for (i = 0; i < 256; i++)
    {
        ChafaColor ncol;

        ncol.ch [0] = (col.ch [0] * (255 - i)) / 255;
        ncol.ch [1] = (col.ch [1] * (255 - i)) / 255;
        ncol.ch [2] = (col.ch [2] * (255 - i)) / 255;
        ncol.ch [3] = 0;

        chafa_color8_store_to_rgba8 (ncol, &color_lut [i]);
    }
}

static void
post_scale_row (guint32 *row_inout, int width, void *user_data)
{
    const DrawPixelsCtx *ctx = user_data;
    guint32 *row_inout_end = row_inout + width;

    /* Composite on solid background color */

    for ( ; row_inout < row_inout_end; row_inout++)
    {
        ChafaColor c = chafa_color8_fetch_from_rgba8 (row_inout);
        *row_inout += ctx->bg_color_lut [c.ch [3]];
    }
}

static void
draw_pixels_pass_1_worker (ChafaBatchInfo *batch, const DrawPixelsCtx *ctx)
{
    smol_scale_batch_full (ctx->scale_ctx,
                           ctx->scaled_data + (ctx->dest_width * batch->first_row),
                           batch->first_row,
                           batch->n_rows);
}

static gint
quantize_pixel (const ChafaPalette *palette, ChafaColorSpace color_space,
                ChafaColorHash *color_hash, ChafaColor color)
{
    ChafaColor cached_color;
    gint index;

    if ((gint) (color.ch [3]) < chafa_palette_get_alpha_threshold (palette))
        return chafa_palette_get_transparent_index (palette);

    /* Sixel color resolution is only slightly less than 7 bits per channel,
     * so eliminate the low-order bits to get better hash performance. Also
     * mask out the alpha channel. */
    CHAFA_COLOR8_U32 (cached_color) = CHAFA_COLOR8_U32 (color) & GUINT32_FROM_BE (0xfefefe00);

    index = chafa_color_hash_lookup (color_hash, CHAFA_COLOR8_U32 (cached_color));

    if (index < 0)
    {
        if (color_space == CHAFA_COLOR_SPACE_DIN99D)
            chafa_color_rgb_to_din99d (&color, &color);

        index = chafa_palette_lookup_nearest (palette,
                                              color_space,
                                              &color,
                                              NULL)
          - chafa_palette_get_first_color (palette);

        /* Don't insert transparent pixels, since color hash does not store transparency */
        if (index != chafa_palette_get_transparent_index (palette))
            chafa_color_hash_replace (color_hash, CHAFA_COLOR8_U32 (cached_color), index);
    }

    return index;
}

static gint
quantize_pixel_with_error (const ChafaPalette *palette, ChafaColorSpace color_space,
                           ChafaColor color, ChafaColorAccum *error_inout)
{
    gint index;

    if ((gint) (color.ch [3]) < chafa_palette_get_alpha_threshold (palette))
    {
        gint i;

        /* Don't propagate error across transparency */
        for (i = 0; i < 4; i++)
            error_inout->ch [i] = 0;

        return chafa_palette_get_transparent_index (palette);
    }

    if (color_space == CHAFA_COLOR_SPACE_DIN99D)
        chafa_color_rgb_to_din99d (&color, &color);

    index = chafa_palette_lookup_with_error (palette,
                                             color_space,
                                             color,
                                             error_inout)
      - chafa_palette_get_first_color (palette);

    return index;
}

static void
draw_pixels_pass_2_nodither (ChafaBatchInfo *batch, const DrawPixelsCtx *ctx,
                             ChafaColorHash *chash)
{
    const guint32 *src_p;
    guint8 *dest_p, *dest_end_p;

    src_p = ctx->scaled_data + (ctx->dest_width * batch->first_row);
    dest_p = ctx->indexed_image->pixels + (ctx->dest_width * batch->first_row);
    dest_end_p = dest_p + (ctx->dest_width * batch->n_rows);

    for ( ; dest_p < dest_end_p; src_p++, dest_p++)
    {
        ChafaColor col;
        gint index;

        col = chafa_color8_fetch_from_rgba8 (src_p);
        index = quantize_pixel (&ctx->indexed_image->palette, ctx->color_space, chash, col);
        *dest_p = index;
    }
}

static void
draw_pixels_pass_2_bayer (ChafaBatchInfo *batch, const DrawPixelsCtx *ctx,
                          ChafaColorHash *chash)
{
    const guint32 *src_p;
    guint8 *dest_p, *dest_end_p;
    gint x, y;

    src_p = ctx->scaled_data + (ctx->dest_width * batch->first_row);
    dest_p = ctx->indexed_image->pixels + (ctx->dest_width * batch->first_row);
    dest_end_p = dest_p + (ctx->dest_width * batch->n_rows);

    x = 0;
    y = batch->first_row;

    for ( ; dest_p < dest_end_p; src_p++, dest_p++)
    {
        ChafaColor col;
        gint index;

        col = chafa_color8_fetch_from_rgba8 (src_p);
        col = chafa_dither_color_ordered (&ctx->indexed_image->dither, col, x, y);
        index = quantize_pixel (&ctx->indexed_image->palette, ctx->color_space, chash, col);
        *dest_p = index;

        if (++x >= ctx->dest_width)
        {
            x = 0;
            y++;
        }
    }
}

static void
distribute_error (ChafaColorAccum error_in, ChafaColorAccum *error_out_0,
                  ChafaColorAccum *error_out_1, ChafaColorAccum *error_out_2,
                  ChafaColorAccum *error_out_3, gdouble intensity)
{
    gint i;

    for (i = 0; i < 3; i++)
    {
        gint16 ch = error_in.ch [i];

        error_out_0->ch [i] += (ch * 7) * intensity;
        error_out_1->ch [i] += (ch * 1) * intensity;
        error_out_2->ch [i] += (ch * 5) * intensity;
        error_out_3->ch [i] += (ch * 3) * intensity;
    }
}

static guint8
fs_dither_pixel (const DrawPixelsCtx *ctx, G_GNUC_UNUSED ChafaColorHash *chash,
                 const guint32 *inpixel_p,
                 ChafaColorAccum error_in,
                 ChafaColorAccum *error_out_0, ChafaColorAccum *error_out_1,
                 ChafaColorAccum *error_out_2, ChafaColorAccum *error_out_3)
{
    ChafaColor col = chafa_color8_fetch_from_rgba8 (inpixel_p);
    guint8 index;

    index = quantize_pixel_with_error (&ctx->indexed_image->palette, ctx->color_space, col, &error_in);
    distribute_error (error_in,
                      error_out_0, error_out_1, error_out_2, error_out_3,
                      ctx->indexed_image->dither.intensity);
    return index;
}

static void
fs_dither_row (const DrawPixelsCtx *ctx, ChafaColorHash *chash, const guint32 *inrow_p,
               guint8 *outrow_p, ChafaColorAccum *error_row, ChafaColorAccum *next_error_row,
               gint width, gint y)
{
    gint x;

    if (y & 1)
    {
        /* Forwards pass */

        outrow_p [0] = fs_dither_pixel (ctx, chash, &inrow_p [0], error_row [0],
                                        &error_row [1],
                                        &next_error_row [1],
                                        &next_error_row [0],
                                        &next_error_row [1]);

        for (x = 1; x < width - 1; x++)
        {
            outrow_p [x] = fs_dither_pixel (ctx, chash, &inrow_p [x], error_row [x],
                                            &error_row [x + 1],
                                            &next_error_row [x + 1],
                                            &next_error_row [x],
                                            &next_error_row [x - 1]);
        }

        outrow_p [x] = fs_dither_pixel (ctx, chash, &inrow_p [x], error_row [x],
                                        &next_error_row [x],
                                        &next_error_row [x],
                                        &next_error_row [x - 1],
                                        &next_error_row [x - 1]);
    }
    else
    {
        /* Backwards pass */

        x = width - 1;

        outrow_p [x] = fs_dither_pixel (ctx, chash, &inrow_p [x], error_row [x],
                                        &error_row [x - 1],
                                        &next_error_row [x - 1],
                                        &next_error_row [x],
                                        &next_error_row [x - 1]);

        for (x--; x >= 1; x--)
        {
            outrow_p [x] = fs_dither_pixel (ctx, chash, &inrow_p [x], error_row [x],
                                            &error_row [x - 1],
                                            &next_error_row [x - 1],
                                            &next_error_row [x],
                                            &next_error_row [x + 1]);
        }

        outrow_p [0] = fs_dither_pixel (ctx, chash, &inrow_p [0], error_row [0],
                                        &next_error_row [0],
                                        &next_error_row [0],
                                        &next_error_row [1],
                                        &next_error_row [1]);
    }
}

static void
draw_pixels_pass_2_fs (ChafaBatchInfo *batch, const DrawPixelsCtx *ctx,
                       ChafaColorHash *chash)
{
    ChafaColorAccum *error_row [2];
    const guint32 *src_p;
    guint8 *dest_end_p, *dest_p;
    gint y;

    error_row [0] = g_malloc (ctx->dest_width * sizeof (ChafaColorAccum));
    error_row [1] = g_malloc (ctx->dest_width * sizeof (ChafaColorAccum));

    src_p = ctx->scaled_data + (ctx->dest_width * batch->first_row);
    dest_p = ctx->indexed_image->pixels + (ctx->dest_width * batch->first_row);
    dest_end_p = dest_p + (ctx->dest_width * batch->n_rows);

    y = batch->first_row;

    memset (error_row [0], 0, ctx->dest_width * sizeof (ChafaColorAccum));

    for ( ; dest_p < dest_end_p; src_p += ctx->dest_width, dest_p += ctx->dest_width, y++)
    {
        ChafaColorAccum *error_row_temp;

        memset (error_row [1], 0, ctx->dest_width * sizeof (ChafaColorAccum));

        fs_dither_row (ctx, chash, src_p, dest_p, error_row [0], error_row [1],
                       ctx->dest_width, y);

        error_row_temp = error_row [0];
        error_row [0] = error_row [1];
        error_row [1] = error_row_temp;
    }

    g_free (error_row [1]);
    g_free (error_row [0]);
}

static void
draw_pixels_pass_2_worker (ChafaBatchInfo *batch, const DrawPixelsCtx *ctx)
{
    ChafaColorHash chash;

    chafa_color_hash_init (&chash);

    switch (ctx->indexed_image->dither.mode)
    {
        case CHAFA_DITHER_MODE_NONE:
            draw_pixels_pass_2_nodither (batch, ctx, &chash);
            break;

        case CHAFA_DITHER_MODE_ORDERED:
            draw_pixels_pass_2_bayer (batch, ctx, &chash);
            break;

        case CHAFA_DITHER_MODE_DIFFUSION:
            draw_pixels_pass_2_fs (batch, ctx, &chash);
            break;

        case CHAFA_DITHER_MODE_MAX:
            g_assert_not_reached ();
            break;
    }

    chafa_color_hash_deinit (&chash);
}

static void
draw_pixels (DrawPixelsCtx *ctx)
{
    chafa_process_batches (ctx,
                           (GFunc) draw_pixels_pass_1_worker,
                           NULL,
                           ctx->dest_height,
                           chafa_get_n_actual_threads (),
                           1);

    chafa_palette_generate (&ctx->indexed_image->palette,
                            ctx->scaled_data, ctx->dest_width * ctx->dest_height,
                            ctx->color_space);

    /* Single thread only for diffusion; it's a fully serial operation */
    chafa_process_batches (ctx,
                           (GFunc) draw_pixels_pass_2_worker,
                           NULL,
                           ctx->dest_height,
                           ctx->indexed_image->dither.mode == CHAFA_DITHER_MODE_DIFFUSION
                             ? 1 : chafa_get_n_actual_threads (),
                           1);
}

ChafaIndexedImage *
chafa_indexed_image_new (gint width, gint height,
                         const ChafaPalette *palette,
                         const ChafaDither *dither)
{
    ChafaIndexedImage *indexed_image;

    indexed_image = g_new0 (ChafaIndexedImage, 1);
    indexed_image->width = width;
    indexed_image->height = height;
    indexed_image->pixels = g_malloc (width * height);

    chafa_palette_copy (palette, &indexed_image->palette);
    chafa_palette_set_transparent_index (&indexed_image->palette, 255);

    chafa_dither_copy (dither, &indexed_image->dither);

    return indexed_image;
}

void
chafa_indexed_image_destroy (ChafaIndexedImage *indexed_image)
{
    chafa_dither_deinit (&indexed_image->dither);
    g_free (indexed_image->pixels);
    g_free (indexed_image);
}

void
chafa_indexed_image_draw_pixels (ChafaIndexedImage *indexed_image,
                                 ChafaColorSpace color_space,
                                 ChafaPixelType src_pixel_type,
                                 gconstpointer src_pixels,
                                 gint src_width, gint src_height, gint src_rowstride,
                                 gint dest_width, gint dest_height)
{
    DrawPixelsCtx ctx;

    g_return_if_fail (dest_width == indexed_image->width);
    g_return_if_fail (dest_height <= indexed_image->height);

    dest_width = MIN (dest_width, indexed_image->width);
    dest_height = MIN (dest_height, indexed_image->height);

    ctx.indexed_image = indexed_image;
    ctx.color_space = color_space;
    ctx.src_pixel_type = src_pixel_type;
    ctx.src_pixels = src_pixels;
    ctx.src_width = src_width;
    ctx.src_height = src_height;
    ctx.src_rowstride = src_rowstride;
    ctx.dest_width = dest_width;
    ctx.dest_height = dest_height;

    gen_color_lut_rgba8 (ctx.bg_color_lut,
                         *chafa_palette_get_color (&indexed_image->palette,
                                                   CHAFA_COLOR_SPACE_RGB,
                                                   CHAFA_PALETTE_INDEX_BG));

    ctx.scaled_data = g_new (guint32, dest_width * dest_height);
    ctx.scale_ctx = smol_scale_new_full ((SmolPixelType) src_pixel_type,
                                         (const guint32 *) src_pixels,
                                         src_width,
                                         src_height,
                                         src_rowstride,
                                         SMOL_PIXEL_RGBA8_PREMULTIPLIED,
                                         NULL,
                                         dest_width,
                                         dest_height,
                                         dest_width * sizeof (guint32),
                                         post_scale_row,
                                         &ctx);

    draw_pixels (&ctx);

    memset (indexed_image->pixels + indexed_image->width * dest_height,
            0,
            indexed_image->width * (indexed_image->height - dest_height));

    smol_scale_destroy (ctx.scale_ctx);
    g_free (ctx.scaled_data);
}
