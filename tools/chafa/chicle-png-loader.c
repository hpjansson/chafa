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
#include <lodepng.h>
#include <lodepng-apng.h>
#include "chicle-png-loader.h"
#include "chicle-util.h"

#define IMAGE_BUFFER_SIZE_MAX (0xffffffffU >> 2)

/* Used when an APNG frame has a zero delay, like the GIF loader does */
#define DEFAULT_FRAME_DELAY_MS 50

struct ChiclePngLoader
{
    ChicleFileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    gint width, height;
    ChafaPixelType pixel_type;
    gint bytes_per_pixel;

    /* Still image */
    gpointer frame_data;

    /* Animation. The canvas belongs to anim. */
    LodePNGAnim *anim;
    LodePNGFrameControl fctl;
    const guint8 *canvas;
    gint frame_index;
    gint n_frames;
};

/* Read the header. If the file is an RGB888 image with no transparency chunk,
 * we can take LodePNG's output and convert it in Smolscale, which is much more
 * efficient in both CPU time and peak RSS. */
static gboolean
inspect_still (const guint8 *data, size_t len,
               guint *width_out, guint *height_out, gboolean *is_rgb8_out)
{
    LodePNGState state;
    unsigned width, height;
    gboolean result = FALSE;

    lodepng_state_init (&state);

    if (lodepng_inspect (&width, &height, &state, data, len) != 0)
        goto out;

    *width_out = width;
    *height_out = height;
    *is_rgb8_out = (state.info_png.color.colortype == LCT_RGB
                    && state.info_png.color.bitdepth == 8
                    && lodepng_chunk_find_const (data, data + len, "tRNS") == NULL)
        ? TRUE : FALSE;
    result = TRUE;

out:
    lodepng_state_cleanup (&state);
    return result;
}

static void
init_decoder_settings (LodePNGDecoderSettings *settings)
{
    settings->zlibsettings.max_output_size = IMAGE_BUFFER_SIZE_MAX;

    /* We build LodePNG without CRC verification. Adler-32 is pointless. */
    settings->zlibsettings.ignore_adler32 = 1;
}

static ChiclePngLoader *
chicle_png_loader_new (void)
{
    return g_new0 (ChiclePngLoader, 1);
}

static gboolean
load_animation (ChiclePngLoader *loader)
{
    LodePNGAnim *anim;
    gboolean success = FALSE;

    anim = g_new (LodePNGAnim, 1);
    lodepng_anim_init (anim);
    init_decoder_settings (&anim->state.decoder);

    if (lodepng_anim_inspect (anim, loader->file_data, loader->file_data_len) != 0)
        goto out;

    /* Not an APNG, or none of its frames are usable */
    if (anim->num_frames == 0)
        goto out;

    if (!chicle_calc_image_buffer_size (anim->width, anim->height, 4,
                                        IMAGE_BUFFER_SIZE_MAX, NULL))
        goto out;

    if (lodepng_anim_decode_next (anim, &loader->canvas, &loader->fctl) != 0)
        goto out;

    loader->width = (gint) anim->width;
    loader->height = (gint) anim->height;
    loader->pixel_type = CHAFA_PIXEL_RGBA8_UNASSOCIATED;
    loader->bytes_per_pixel = 4;
    loader->frame_index = 0;
    loader->n_frames = (gint) anim->num_frames;

    loader->anim = anim;
    anim = NULL;
    success = TRUE;

out:
    if (anim)
    {
        lodepng_anim_cleanup (anim);
        g_free (anim);
    }

    return success;
}

static gboolean
load_still (ChiclePngLoader *loader)
{
    gboolean success = FALSE;
    guint width, height;
    gboolean is_rgb8;
    unsigned char *frame_data = NULL;
    LodePNGState lode_state;
    gint lode_error;

    lodepng_state_init (&lode_state);

    if (!inspect_still (loader->file_data, loader->file_data_len, &width, &height, &is_rgb8))
        goto out;

    if (is_rgb8)
    {
        lode_state.info_raw.colortype = LCT_RGB;
        loader->pixel_type = CHAFA_PIXEL_RGB8;
        loader->bytes_per_pixel = 3;
    }
    else
    {
        lode_state.info_raw.colortype = LCT_RGBA;
        loader->pixel_type = CHAFA_PIXEL_RGBA8_UNASSOCIATED;
        loader->bytes_per_pixel = 4;
    }

    lode_state.info_raw.bitdepth = 8;
    init_decoder_settings (&lode_state.decoder);

    /* Check before decoding; LodePNG sizes its buffers from the header */
    if (!chicle_calc_image_buffer_size (width, height, loader->bytes_per_pixel,
                                          IMAGE_BUFFER_SIZE_MAX, NULL))
        goto out;

    /* Decodes to RGB8 or RGBA8 */
    if ((lode_error = lodepng_decode (&frame_data, &width, &height,
                                      &lode_state,
                                      loader->file_data, loader->file_data_len)) != 0)
        goto out;

    loader->frame_data = frame_data;
    loader->width = (gint) width;
    loader->height = (gint) height;

    success = TRUE;

out:
    if (!success && frame_data)
        free (frame_data);

    lodepng_state_cleanup (&lode_state);
    return success;
}

ChiclePngLoader *
chicle_png_loader_new_from_mapping (ChicleFileMapping *mapping)
{
    ChiclePngLoader *loader = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail (mapping != NULL, NULL);

    if (!chicle_file_mapping_has_magic (mapping, 0, "\x89PNG", 4))
        goto out;

    loader = chicle_png_loader_new ();
    loader->mapping = mapping;

    loader->file_data = chicle_file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    /* If the animation is unusable, the default image may still work */
    if (!load_animation (loader) && !load_still (loader))
        goto out;

    success = TRUE;

out:
    if (!success && loader)
    {
        g_free (loader);
        loader = NULL;
    }

    return loader;
}

void
chicle_png_loader_destroy (ChiclePngLoader *loader)
{
    if (loader->mapping)
        chicle_file_mapping_destroy (loader->mapping);

    if (loader->frame_data)
        free (loader->frame_data);

    if (loader->anim)
    {
        lodepng_anim_cleanup (loader->anim);
        g_free (loader->anim);
    }

    g_free (loader);
}

gboolean
chicle_png_loader_get_is_animation (ChiclePngLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return loader->anim && loader->n_frames > 1 ? TRUE : FALSE;
}

gconstpointer
chicle_png_loader_get_frame_data (ChiclePngLoader *loader,
                                  ChafaPixelType *pixel_type_out,
                                  gint *width_out,
                                  gint *height_out,
                                  gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    if (pixel_type_out)
        *pixel_type_out = loader->pixel_type;
    if (width_out)
        *width_out = loader->width;
    if (height_out)
        *height_out = loader->height;
    if (rowstride_out)
        *rowstride_out = loader->width * loader->bytes_per_pixel;

    return loader->anim ? (gconstpointer) loader->canvas : loader->frame_data;
}

gint
chicle_png_loader_get_frame_delay (ChiclePngLoader *loader)
{
    guint num, den;
    gint delay_ms;

    g_return_val_if_fail (loader != NULL, 0);

    if (!loader->anim)
        return 0;

    /* The delay is num / den seconds. A zero denominator means 100. */
    num = loader->fctl.delay_num;
    den = loader->fctl.delay_den ? loader->fctl.delay_den : 100;
    delay_ms = (gint) ((num * 1000 + den / 2) / den);

    /* A zero delay means "show the next frame as soon as possible". Since
     * that would make playback spin, go with the same default as for GIF. */
    if (delay_ms == 0)
        delay_ms = DEFAULT_FRAME_DELAY_MS;

    return delay_ms;
}

void
chicle_png_loader_goto_first_frame (ChiclePngLoader *loader)
{
    g_return_if_fail (loader != NULL);

    if (!loader->anim || loader->frame_index == 0)
        return;

    lodepng_anim_rewind (loader->anim);
    loader->frame_index = 0;

    /* This frame decoded fine before, so it can only fail to allocate */
    if (lodepng_anim_decode_next (loader->anim, &loader->canvas, &loader->fctl) != 0)
        loader->canvas = NULL;
}

gboolean
chicle_png_loader_goto_next_frame (ChiclePngLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    if (!loader->anim || !loader->canvas
        || loader->frame_index + 1 >= loader->n_frames)
        return FALSE;

    if (lodepng_anim_decode_next (loader->anim, &loader->canvas, &loader->fctl) != 0)
    {
        /* Corrupt frame. Play the frames before it from now on. */
        loader->n_frames = loader->frame_index + 1;
        return FALSE;
    }

    loader->frame_index++;
    return TRUE;
}
