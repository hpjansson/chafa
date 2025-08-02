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

#include <chafa.h>
#include <avif/avif.h>
#include "chicle-avif-loader.h"
#include "chicle-util.h"

#define N_CHANNELS 4
#define BYTES_PER_PIXEL N_CHANNELS
#define IMAGE_BUFFER_SIZE_MAX (0xffffffffU >> 2)

struct ChicleAvifLoader
{
    ChicleFileMapping *mapping;
    gconstpointer file_data;
    size_t file_data_len;
    gpointer frame_data;
    guint width, height;
    guint rowstride;
    avifDecoder *decoder;
    gint current_frame_index;
    guint frame_is_decoded : 1;
    guint frame_is_success : 1;
};

static ChicleRotationType rotation [4] [3] =
{
    { CHICLE_ROTATION_180_MIRROR, CHICLE_ROTATION_0_MIRROR,   CHICLE_ROTATION_NONE },
    { CHICLE_ROTATION_270_MIRROR, CHICLE_ROTATION_90_MIRROR,  CHICLE_ROTATION_270  },
    { CHICLE_ROTATION_0_MIRROR,   CHICLE_ROTATION_180_MIRROR, CHICLE_ROTATION_180  },
    { CHICLE_ROTATION_90_MIRROR,  CHICLE_ROTATION_270_MIRROR, CHICLE_ROTATION_90   }
};

static ChicleRotationType
calc_rotation (avifTransformFlags tflags, guint angle, guint axis)
{
    guint8 rot, mir;

    if (angle > 3 || axis > 1)
        return CHICLE_ROTATION_NONE;

    rot = (tflags & AVIF_TRANSFORM_IROT) ? angle : 0;
    mir = (tflags & AVIF_TRANSFORM_IMIR) ? axis : 2;

    return rotation [rot] [mir];
}

static gboolean
maybe_decode_frame (ChicleAvifLoader *loader)
{
    avifResult avif_result;
    avifImage *image;
    avifRGBImage rgb;
    guint axis;

    if (loader->frame_is_decoded)
        return loader->frame_is_success;

    loader->frame_is_success = FALSE;
    rgb.pixels = NULL;

    avif_result = avifDecoderNextImage (loader->decoder);
    if (avif_result != AVIF_RESULT_OK)
        goto out;

    image = loader->decoder->image;
    avifRGBImageSetDefaults (&rgb, image);

    rgb.depth = 8;
    rgb.format = AVIF_RGB_FORMAT_RGBA;
    rgb.rowBytes = image->width * BYTES_PER_PIXEL;
    rgb.pixels = g_malloc (image->height * rgb.rowBytes);

    avif_result = avifImageYUVToRGB(image, &rgb);
    if (avif_result != AVIF_RESULT_OK)
        goto out;

    loader->width = image->width;
    loader->height = image->height;
    loader->rowstride = rgb.rowBytes;

    g_free (loader->frame_data);
    loader->frame_data = rgb.pixels;

#if AVIF_VERSION_MAJOR >= 1
    axis = image->imir.axis;
#else
    axis = image->imir.mode;
#endif

    chicle_rotate_image (&loader->frame_data,
                         &loader->width, &loader->height,
                         &loader->rowstride, N_CHANNELS,
                         calc_rotation (image->transformFlags,
                                        image->irot.angle,
                                        axis));

    loader->frame_is_success = TRUE;

out:
    if (!loader->frame_is_success)
    {
        g_free (rgb.pixels);
    }

    return loader->frame_is_success;
}

static ChicleAvifLoader *
chicle_avif_loader_new (void)
{
    return g_new0 (ChicleAvifLoader, 1);
}

ChicleAvifLoader *
chicle_avif_loader_new_from_mapping (ChicleFileMapping *mapping)
{
    ChicleAvifLoader *loader = NULL;
    gboolean success = FALSE;
    avifResult avif_result;
    avifImage *image;

    g_return_val_if_fail (mapping != NULL, NULL);

    /* Quick check for the ISOBMFF ftyp box to filter out files that are
     * something else entirely */
    if (!chicle_file_mapping_has_magic (mapping, 4, "ftyp", 4))
        goto out;

    loader = chicle_avif_loader_new ();
    loader->mapping = mapping;

    loader->file_data = chicle_file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    loader->decoder = avifDecoderCreate ();

    /* Allow for missing PixelInformationProperty, invalid clap box and
     * missing ImageSpatialExtentsProperty in alpha auxiliary image items */
    loader->decoder->strictFlags = AVIF_STRICT_DISABLED;

    avif_result = avifDecoderSetIOMemory (loader->decoder, loader->file_data, loader->file_data_len);
    if (avif_result != AVIF_RESULT_OK)
        goto out;

    avif_result = avifDecoderParse (loader->decoder);
    if (avif_result != AVIF_RESULT_OK)
        goto out;

    image = loader->decoder->image;

    if (image->width < 1 || image->width >= (1 << 28)
        || image->height < 1 || image->height >= (1 << 28))
        goto out;

    loader->width = image->width;
    loader->height = image->height;
    loader->rowstride = loader->width * BYTES_PER_PIXEL;

    if (loader->height * loader->rowstride > IMAGE_BUFFER_SIZE_MAX)
        goto out;

    success = TRUE;

out:
    if (!success)
    {
        if (loader)
        {
            if (loader->decoder)
            {
                avifDecoderDestroy (loader->decoder);
                loader->decoder = NULL;
            }

            g_free (loader);
            loader = NULL;
        }
    }

    return loader;
}

void
chicle_avif_loader_destroy (ChicleAvifLoader *loader)
{
    if (loader->decoder)
        avifDecoderDestroy (loader->decoder);

    if (loader->mapping)
        chicle_file_mapping_destroy (loader->mapping);

    if (loader->frame_data)
        g_free (loader->frame_data);

    g_free (loader);
}

gboolean
chicle_avif_loader_get_is_animation (ChicleAvifLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return loader->decoder->imageCount > 1 ? TRUE : FALSE;
}

gconstpointer
chicle_avif_loader_get_frame_data (ChicleAvifLoader *loader,
                                   ChafaPixelType *pixel_type_out,
                                   gint *width_out,
                                   gint *height_out,
                                   gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    if (!maybe_decode_frame (loader))
        return NULL;

    if (width_out)
        *width_out = loader->width;
    if (height_out)
        *height_out = loader->height;
    if (pixel_type_out)
        *pixel_type_out = CHAFA_PIXEL_RGBA8_UNASSOCIATED;
    if (rowstride_out)
        *rowstride_out = loader->rowstride;

    return loader->frame_data;
}

gint
chicle_avif_loader_get_frame_delay (ChicleAvifLoader *loader)
{
    gdouble duration_s;

    g_return_val_if_fail (loader != NULL, 0);

    duration_s = loader->decoder->imageTiming.duration;

    /* NaN or very small? */
    if (duration_s != duration_s || duration_s < 0.000001)
        duration_s = 0.05;

    /* Infinite or very big? Set frame duration to a bit more than a day's time. */
    if (duration_s > 99999.0)
        duration_s = 99999.0;

    return duration_s * 1000.0;
}

void
chicle_avif_loader_goto_first_frame (ChicleAvifLoader *loader)
{
    g_return_if_fail (loader != NULL);

    if (loader->current_frame_index == 0)
        return;

    loader->current_frame_index = 0;
    loader->frame_is_decoded = FALSE;
    loader->frame_is_success = FALSE;

    avifDecoderReset (loader->decoder);
}

gboolean
chicle_avif_loader_goto_next_frame (ChicleAvifLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    if (loader->current_frame_index + 1 >= (gint) loader->decoder->imageCount)
        return FALSE;

    loader->current_frame_index++;
    loader->frame_is_decoded = FALSE;
    loader->frame_is_success = FALSE;
    return TRUE;
}
