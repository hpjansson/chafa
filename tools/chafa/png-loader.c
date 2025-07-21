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
#include "png-loader.h"

#define BYTES_PER_PIXEL 4
#define IMAGE_BUFFER_SIZE_MAX (0xffffffffU >> 2)

struct PngLoader
{
    FileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    gpointer frame_data;
    gint width, height;
};

static PngLoader *
png_loader_new (void)
{
    return g_new0 (PngLoader, 1);
}

PngLoader *
png_loader_new_from_mapping (FileMapping *mapping)
{
    PngLoader *loader = NULL;
    gboolean success = FALSE;
    guint width, height;
    unsigned char *frame_data = NULL;
    LodePNGState lode_state;
    gint lode_error;

    g_return_val_if_fail (mapping != NULL, NULL);

    lodepng_state_init (&lode_state);

    if (!file_mapping_has_magic (mapping, 0, "\x89PNG", 4))
        goto out;

    loader = png_loader_new ();
    loader->mapping = mapping;

    loader->file_data = file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    lode_state.info_raw.colortype = LCT_RGBA;
    lode_state.info_raw.bitdepth = 8;
    lode_state.decoder.zlibsettings.max_output_size = IMAGE_BUFFER_SIZE_MAX;

    /* Decodes to RGBA8 */
    if ((lode_error = lodepng_decode (&frame_data, &width, &height,
                                      &lode_state,
                                      loader->file_data, loader->file_data_len)) != 0)
        goto out;

    if (width < 1 || width >= (1 << 28)
        || height < 1 || height >= (1 << 28))
        goto out;

    loader->frame_data = frame_data;
    loader->width = (gint) width;
    loader->height = (gint) height;

    success = TRUE;

out:
    if (!success)
    {
        if (loader)
        {
            g_free (loader);
            loader = NULL;
        }

        if (frame_data)
            free (frame_data);
    }

    lodepng_state_cleanup (&lode_state);
    return loader;
}

void
png_loader_destroy (PngLoader *loader)
{
    if (loader->mapping)
        file_mapping_destroy (loader->mapping);

    if (loader->frame_data)
        free (loader->frame_data);

    g_free (loader);
}

gboolean
png_loader_get_is_animation (PngLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return FALSE;
}

gconstpointer
png_loader_get_frame_data (PngLoader *loader, ChafaPixelType *pixel_type_out,
                           gint *width_out, gint *height_out, gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    if (pixel_type_out)
        *pixel_type_out = CHAFA_PIXEL_RGBA8_UNASSOCIATED;
    if (width_out)
        *width_out = loader->width;
    if (height_out)
        *height_out = loader->height;
    if (rowstride_out)
        *rowstride_out = loader->width * BYTES_PER_PIXEL;

    return loader->frame_data;
}

gint
png_loader_get_frame_delay (PngLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return 0;
}

void
png_loader_goto_first_frame (PngLoader *loader)
{
    g_return_if_fail (loader != NULL);
}

gboolean
png_loader_goto_next_frame (PngLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    return FALSE;
}
