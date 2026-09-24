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
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <webp/demux.h>

#include <chafa.h>
#include "chicle-webp-loader.h"
#include "chicle-util.h"

#define DEFAULT_FRAME_DURATION_MS 50
#define BYTES_PER_PIXEL 4
#define IMAGE_BUFFER_SIZE_MAX (0xffffffffU >> 2)

struct ChicleWebpLoader
{
    ChicleFileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    gint width, height;
    ChafaPixelType pixel_type;
    WebPAnimDecoder *anim_decoder;
    gpointer this_frame_data, next_frame_data;
    gint this_timestamp, next_timestamp;
    guint is_animation : 1;
};

static gboolean
decode_next_frame (ChicleWebpLoader *loader, uint8_t **buf, int *timestamp)
{
    if (!loader->anim_decoder)
        return FALSE;
    return WebPAnimDecoderGetNext (loader->anim_decoder, buf, timestamp);
}

/* Optimized decoding for stills. These decode straight into our buffer,
 * and we allow libwebp to downscale on the fly by 1/2, 1/4 or 1/8, which
 * allows it to skip some work. */
static gboolean
decode_still (ChicleWebpLoader *loader, const WebPBitstreamFeatures *features,
              gint target_width, gint target_height)
{
    WebPDecoderConfig config;
    gint out_width = features->width, out_height = features->height;
    gsize frame_size;

    if (!WebPInitDecoderConfig (&config))
        return FALSE;

    if (target_width > 0 && target_height > 0)
    {
        gdouble ratio = MAX (features->width / (gdouble) target_width,
                             features->height / (gdouble) target_height);
        gint denom = ratio >= 8.0 ? 8 : ratio >= 4.0 ? 4 : ratio >= 2.0 ? 2 : 1;

        if (denom > 1)
        {
            out_width = (features->width + denom - 1) / denom;
            out_height = (features->height + denom - 1) / denom;
            config.options.use_scaling = 1;
            config.options.scaled_width = out_width;
            config.options.scaled_height = out_height;
        }
    }

    config.options.use_threads = 1;

    if (!chicle_checked_image_buffer_size (out_width, out_height, BYTES_PER_PIXEL,
                                          IMAGE_BUFFER_SIZE_MAX, &frame_size))
        return FALSE;

    loader->this_frame_data = g_malloc (frame_size);
    config.output.colorspace = MODE_RGBA;
    config.output.is_external_memory = 1;
    config.output.u.RGBA.rgba = loader->this_frame_data;
    config.output.u.RGBA.stride = out_width * BYTES_PER_PIXEL;
    config.output.u.RGBA.size = frame_size;

    if (WebPDecode (loader->file_data, loader->file_data_len, &config) != VP8_STATUS_OK)
    {
        g_free (loader->this_frame_data);
        loader->this_frame_data = NULL;
        return FALSE;
    }

    loader->width = out_width;
    loader->height = out_height;
    return TRUE;
}

static gboolean
maybe_decode_frame (ChicleWebpLoader *loader)
{
    uint8_t *buf;
    gsize frame_size;

    if (loader->this_frame_data)
        return TRUE;

    if (decode_next_frame (loader, &buf, &loader->this_timestamp))
    {
        if (!chicle_checked_image_buffer_size (loader->width, loader->height, BYTES_PER_PIXEL,
                                              IMAGE_BUFFER_SIZE_MAX, &frame_size))
            return FALSE;

        loader->this_frame_data = g_memdup (buf, frame_size);
    }

    return loader->this_frame_data ? TRUE : FALSE;
}

static ChicleWebpLoader *
chicle_webp_loader_new (void)
{
    return g_new0 (ChicleWebpLoader, 1);
}

ChicleWebpLoader *
chicle_webp_loader_new_from_mapping (ChicleFileMapping *mapping,
                                     gint target_width, gint target_height)
{
    ChicleWebpLoader *loader = NULL;
    gboolean success = FALSE;
    WebPBitstreamFeatures features;
    WebPAnimDecoderOptions anim_decoder_options;
    WebPData webp_data;
    WebPAnimInfo anim_info;

    g_return_val_if_fail (mapping != NULL, NULL);

    /* Basic validation and info extraction */

    if (!chicle_file_mapping_has_magic (mapping, 0, "RIFF", 4)
        || !chicle_file_mapping_has_magic (mapping, 8, "WEBP", 4))
        goto out;

    loader = chicle_webp_loader_new ();
    loader->mapping = mapping;

    loader->file_data = chicle_file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    if (!WebPGetInfo (loader->file_data, loader->file_data_len, &features.width, &features.height))
        goto out;

    if (WebPGetFeatures (loader->file_data, loader->file_data_len, &features) != VP8_STATUS_OK)
        goto out;

    /* Fast path for still images */

    if (!features.has_animation)
    {
        loader->pixel_type = features.has_alpha ? CHAFA_PIXEL_RGBA8_UNASSOCIATED : CHAFA_PIXEL_RGBA8_PREMULTIPLIED;
        if (!decode_still (loader, &features, target_width, target_height))
            goto out;
        success = TRUE;
        goto out;
    }

    /* Set up the animation decoder */

    webp_data.bytes = loader->file_data;
    webp_data.size = loader->file_data_len;

    WebPAnimDecoderOptionsInit (&anim_decoder_options);
    anim_decoder_options.color_mode = MODE_RGBA;
    anim_decoder_options.use_threads = TRUE;

    loader->anim_decoder = WebPAnimDecoderNew (&webp_data, &anim_decoder_options);
    if (!loader->anim_decoder)
        goto out;

    /* Get animation info and validate */

    if (!WebPAnimDecoderGetInfo (loader->anim_decoder, &anim_info))
        goto out;

    if (anim_info.canvas_width < 1 || anim_info.canvas_width >= (1 << 28)
        || anim_info.canvas_height < 1 || anim_info.canvas_height >= (1 << 28)
        || (anim_info.canvas_width * (guint64) anim_info.canvas_height * BYTES_PER_PIXEL > IMAGE_BUFFER_SIZE_MAX))
        goto out;

    if (anim_info.frame_count < 1)
        goto out;

    /* Store parameters */

    if (anim_info.frame_count > 1)
        loader->is_animation = TRUE;

    loader->width = anim_info.canvas_width;
    loader->height = anim_info.canvas_height;

    /* An opaque image with unassociated alpha set to 0xff is equivalent to
     * premultiplied alpha. This will speed up resampling later on. */
    loader->pixel_type = features.has_alpha ? CHAFA_PIXEL_RGBA8_UNASSOCIATED : CHAFA_PIXEL_RGBA8_PREMULTIPLIED;

    /* Ensure we can decode a frame. If not, we'll try other loaders */
    if (!maybe_decode_frame (loader))
        return NULL;

    success = TRUE;

out:
    if (!success)
    {
        if (loader)
        {
            g_free (loader);
            loader = NULL;
        }
    }

    return loader;
}

void
chicle_webp_loader_destroy (ChicleWebpLoader *loader)
{
    if (loader->anim_decoder)
        WebPAnimDecoderDelete (loader->anim_decoder);

    if (loader->mapping)
        chicle_file_mapping_destroy (loader->mapping);

    g_free (loader->this_frame_data);
    loader->this_frame_data = NULL;
    g_free (loader->next_frame_data);
    loader->next_frame_data = NULL;

    g_free (loader);
}

gboolean
chicle_webp_loader_get_is_animation (ChicleWebpLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return loader->is_animation;
}

gconstpointer
chicle_webp_loader_get_frame_data (ChicleWebpLoader *loader,
                                   ChafaPixelType *pixel_type_out,
                                   gint *width_out,
                                   gint *height_out,
                                   gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    if (!maybe_decode_frame (loader))
        return NULL;

    if (pixel_type_out)
        *pixel_type_out = loader->pixel_type;
    if (width_out)
        *width_out = loader->width;
    if (height_out)
        *height_out = loader->height;
    if (rowstride_out)
        *rowstride_out = loader->width * BYTES_PER_PIXEL;

    return loader->this_frame_data;
}

gint
chicle_webp_loader_get_frame_delay (ChicleWebpLoader *loader)
{
    uint8_t *buf;
    gsize frame_size;

    g_return_val_if_fail (loader != NULL, 0);

    /* The libwebp API complicates this a little. We need to load the next frame
     * in advance to know how long to hold this frame. */

    maybe_decode_frame (loader);

    if (!loader->next_frame_data)
    {
        if (decode_next_frame (loader, &buf, &loader->next_timestamp))
        {
            if (!chicle_checked_image_buffer_size (loader->width, loader->height, BYTES_PER_PIXEL,
                                                  IMAGE_BUFFER_SIZE_MAX, &frame_size))
                return DEFAULT_FRAME_DURATION_MS;

            loader->next_frame_data = g_memdup (buf, frame_size);
        }
    }

    return loader->next_frame_data ?
        (loader->next_timestamp - loader->this_timestamp)
        : DEFAULT_FRAME_DURATION_MS;
}

void
chicle_webp_loader_goto_first_frame (ChicleWebpLoader *loader)
{
    g_return_if_fail (loader != NULL);

    if (!loader->anim_decoder)
        return;

    WebPAnimDecoderReset (loader->anim_decoder);
    g_free (loader->this_frame_data);
    loader->this_frame_data = NULL;
    g_free (loader->next_frame_data);
    loader->next_frame_data = NULL;
}

gboolean
chicle_webp_loader_goto_next_frame (ChicleWebpLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    g_free (loader->this_frame_data);
    loader->this_frame_data = loader->next_frame_data;
    loader->next_frame_data = NULL;

    if (loader->this_frame_data)
    {
        loader->this_timestamp = loader->next_timestamp;
        return TRUE;
    }

    return (loader->anim_decoder && WebPAnimDecoderHasMoreFrames (loader->anim_decoder))
        ? TRUE : FALSE;
}
