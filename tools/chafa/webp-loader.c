/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2022 Hans Petter Jansson
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
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <webp/decode.h>

#include <chafa.h>
#include "webp-loader.h"

#define BYTES_PER_PIXEL 4

struct WebpLoader
{
    FileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    gpointer frame_data;
    gint width, height;
    ChafaPixelType pixel_type;
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
    uint8_t *frame_data = NULL;
    WebPBitstreamFeatures features;

    g_return_val_if_fail (mapping != NULL, NULL);

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

    if (features.width < 1 || features.width > (1 << 30)
        || features.height < 1 || features.height > (1 << 30))
        goto out;

    frame_data = WebPDecodeRGBA (loader->file_data, loader->file_data_len,
                                 &features.width, &features.height);
    if (!frame_data)
        goto out;

    loader->frame_data = frame_data;
    loader->width = features.width;
    loader->height = features.height;

    /* An opaque image with unassociated alpha set to 0xff is equivalent to
     * premultiplied alpha. This will speed up resampling later on. */
    loader->pixel_type = features.has_alpha ? CHAFA_PIXEL_RGBA8_UNASSOCIATED : CHAFA_PIXEL_RGBA8_PREMULTIPLIED;

    success = TRUE;

out:
    if (!success)
    {
        if (frame_data)
            WebPFree (frame_data);

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
    if (loader->mapping)
        file_mapping_destroy (loader->mapping);

    if (loader->frame_data)
        WebPFree (loader->frame_data);

    g_free (loader);
}

gboolean
webp_loader_get_is_animation (WebpLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return FALSE;
}

gconstpointer
webp_loader_get_frame_data (WebpLoader *loader, ChafaPixelType *pixel_type_out,
                            gint *width_out, gint *height_out, gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    if (pixel_type_out)
        *pixel_type_out = loader->pixel_type;
    if (width_out)
        *width_out = loader->width;
    if (height_out)
        *height_out = loader->height;
    if (rowstride_out)
        *rowstride_out = loader->width * BYTES_PER_PIXEL;

    return loader->frame_data;
}

gint
webp_loader_get_frame_delay (WebpLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return 0;
}

void
webp_loader_goto_first_frame (WebpLoader *loader)
{
    g_return_if_fail (loader != NULL);
}

gboolean
webp_loader_goto_next_frame (WebpLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    return FALSE;
}
