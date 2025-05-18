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
#include "webp-loader.h"

#define DEFAULT_FRAME_DURATION_MS 50
#define BYTES_PER_PIXEL 4

struct WebpLoader
{
    FileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    gint width, height;
    ChafaPixelType pixel_type;
    WebPAnimDecoder *anim_dec;
    gpointer this_frame_data, next_frame_data;
    gint this_timestamp, next_timestamp;
    guint is_animation : 1;
};

static WebpLoader *
webp_loader_new (void)
{
    return g_new0 (WebpLoader, 1);
}

WebpLoader *
webp_loader_new_from_mapping (FileMapping *mapping)
{
    WebpLoader *loader = NULL;
    gboolean success = FALSE;
    WebPBitstreamFeatures features;
    WebPAnimDecoderOptions anim_dec_options;
    WebPData webp_data;
    WebPAnimInfo anim_info;

    g_return_val_if_fail (mapping != NULL, NULL);

    /* Basic validation and info extraction */

    if (!file_mapping_has_magic (mapping, 0, "RIFF", 4)
        || !file_mapping_has_magic (mapping, 8, "WEBP", 4))
        goto out;

    loader = webp_loader_new ();
    loader->mapping = mapping;

    loader->file_data = file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    if (!WebPGetInfo (loader->file_data, loader->file_data_len, &features.width, &features.height))
        goto out;

    if (WebPGetFeatures (loader->file_data, loader->file_data_len, &features) != VP8_STATUS_OK)
        goto out;

    /* Set up the animation decoder */

    webp_data.bytes = loader->file_data;
    webp_data.size = loader->file_data_len;

    WebPAnimDecoderOptionsInit (&anim_dec_options);
    anim_dec_options.color_mode = MODE_RGBA;
    anim_dec_options.use_threads = TRUE;

    loader->anim_dec = WebPAnimDecoderNew (&webp_data, &anim_dec_options);
    if (!loader->anim_dec)
        goto out;

    /* Get animation info and validate */

    if (!WebPAnimDecoderGetInfo (loader->anim_dec, &anim_info))
        goto out;

    if (anim_info.canvas_width < 1 || anim_info.canvas_width >= (1 << 28)
        || anim_info.canvas_height < 1 || anim_info.canvas_height >= (1 << 28)
        || (anim_info.canvas_width * (guint64) anim_info.canvas_height >= (1 << 29)))
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
webp_loader_destroy (WebpLoader *loader)
{
    if (loader->anim_dec)
        WebPAnimDecoderDelete (loader->anim_dec);

    if (loader->mapping)
        file_mapping_destroy (loader->mapping);

    g_free (loader->this_frame_data);
    loader->this_frame_data = NULL;
    g_free (loader->next_frame_data);
    loader->next_frame_data = NULL;

    g_free (loader);
}

gboolean
webp_loader_get_is_animation (WebpLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return loader->is_animation;
}

static gboolean
decode_next_frame (WebpLoader *loader, uint8_t **buf, int *timestamp)
{
    return WebPAnimDecoderGetNext (loader->anim_dec, buf, timestamp);
}

static gboolean
maybe_decode_frame (WebpLoader *loader)
{
    uint8_t *buf;

    if (loader->this_frame_data)
        return TRUE;

    if (decode_next_frame (loader, &buf, &loader->this_timestamp))
    {
        loader->this_frame_data = g_memdup (buf, loader->width * BYTES_PER_PIXEL * loader->height);
    }

    return loader->this_frame_data ? TRUE : FALSE;
}

gconstpointer
webp_loader_get_frame_data (WebpLoader *loader, ChafaPixelType *pixel_type_out,
                            gint *width_out, gint *height_out, gint *rowstride_out)
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
webp_loader_get_frame_delay (WebpLoader *loader)
{
    uint8_t *buf;

    g_return_val_if_fail (loader != NULL, 0);

    /* The libwebp API complicates this a little. We need to load the next frame
     * in advance to know how long to hold this frame. */

    maybe_decode_frame (loader);

    if (!loader->next_frame_data)
    {
        if (decode_next_frame (loader, &buf, &loader->next_timestamp))
        {
            loader->next_frame_data = g_memdup (buf, loader->width * BYTES_PER_PIXEL * loader->height);
        }
    }

    return loader->next_frame_data ?
        (loader->next_timestamp - loader->this_timestamp)
        : DEFAULT_FRAME_DURATION_MS;
}

void
webp_loader_goto_first_frame (WebpLoader *loader)
{
    g_return_if_fail (loader != NULL);

    WebPAnimDecoderReset (loader->anim_dec);
    g_free (loader->this_frame_data);
    loader->this_frame_data = NULL;
    g_free (loader->next_frame_data);
    loader->next_frame_data = NULL;
}

gboolean
webp_loader_goto_next_frame (WebpLoader *loader)
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

    return WebPAnimDecoderHasMoreFrames (loader->anim_dec) ? TRUE : FALSE;
}
