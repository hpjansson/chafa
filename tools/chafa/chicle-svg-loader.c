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
#include <math.h>

#include <librsvg/rsvg.h>

#include <chafa.h>
#include "chicle-svg-loader.h"

#define DIMENSION_MAX 4096
#define MAGIC_BUF_SIZE 4096
#define BYTES_PER_PIXEL 4
#define IMAGE_BUFFER_SIZE_MAX (0xffffffffU >> 2)

/* Cairo uses native byte order */
#if G_BYTE_ORDER == G_BIG_ENDIAN
# define PIXEL_TYPE CHAFA_PIXEL_ARGB8_PREMULTIPLIED
#else
# define PIXEL_TYPE CHAFA_PIXEL_BGRA8_PREMULTIPLIED
#endif

struct ChicleSvgLoader
{
    ChicleFileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    cairo_surface_t *surface;
};

static ChicleSvgLoader *
chicle_svg_loader_new (void)
{
    return g_new0 (ChicleSvgLoader, 1);
}

static void
calc_dimensions (RsvgHandle *rsvg,
                 gint target_width, gint target_height,
                 guint *width_out, guint *height_out)
{
#if !LIBRSVG_CHECK_VERSION(2, 52, 0)
    RsvgDimensionData dim = { 0 };
#endif
    gdouble width, height;

    rsvg_handle_set_dpi (rsvg, 150.0);

#if LIBRSVG_CHECK_VERSION(2, 52, 0)
    if (!rsvg_handle_get_intrinsic_size_in_pixels (rsvg, &width, &height))
    {
        width = height = (gdouble) DIMENSION_MAX;
    }
#else
    rsvg_handle_get_dimensions (rsvg, &dim);
    width = dim.width;
    height = dim.height;
#endif

    width = MAX (width, 1.0);
    height = MAX (height, 1.0);

    /* Target dimensions can be zero or negative if unspecified */

    if (target_width < 1)
        target_width = (gint) lrint (width);
    if (target_height < 1)
        target_height = (gint) lrint (height);

    /* Avoid generating tiny images that will be subjected to ugly
     * upscaling later */

    if ((width < target_width && height < target_height)
        || (width > target_width && height > target_height))
    {
        if (target_width / width > target_height / height)
        {
            height *= (gdouble) target_width / width;
            width *= (gdouble) target_width / width;
        }
        else
        {
            width *= (gdouble) target_height / height;
            height *= (gdouble) target_height / height;
        }
    }

    /* Enforce maximum surface size */

    if (width > DIMENSION_MAX || height > DIMENSION_MAX)
    {
        if (width > height)
        {
            height *= (gdouble) DIMENSION_MAX / width;
            width = (gdouble) DIMENSION_MAX;
        }
        else
        {
            width *= (gdouble) DIMENSION_MAX / height;
            height = (gdouble) DIMENSION_MAX;
        }
    }

    *width_out = (guint) lrint (width);
    *height_out = (guint) lrint (height);
}

ChicleSvgLoader *
chicle_svg_loader_new_from_mapping (ChicleFileMapping *mapping, gint target_width, gint target_height)
{
    static GMutex mutex;
    ChicleSvgLoader *loader = NULL;
    gboolean success = FALSE;
    RsvgHandle *rsvg = NULL;
    cairo_t *cr = NULL;
#if LIBRSVG_CHECK_VERSION(2, 46, 0)
    RsvgRectangle viewport = { 0 };
#endif
    guint width, height;

    g_return_val_if_fail (mapping != NULL, NULL);

    if (chicle_file_mapping_has_magic (mapping, 0, "<svg", 4))
    {
        /* Quick check: Ok */
    }
    else
    {
        guint8 buf [MAGIC_BUF_SIZE];
        ssize_t len;

        len = chicle_file_mapping_read (mapping, buf, 0, MAGIC_BUF_SIZE - 1);
        if (len < 4)
            goto out;

        buf [len] = '\0';
        if (!strstr ((gchar *) buf, "<svg") && !strstr ((gchar *) buf, "<SVG"))
            goto out;

        /* Slow check: Ok */
    }

    loader = chicle_svg_loader_new ();
    loader->mapping = mapping;

    loader->file_data = chicle_file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    g_mutex_lock (&mutex);

    /* Malformed SVGs will typically fail here */
    rsvg = rsvg_handle_new_from_data (loader->file_data, loader->file_data_len, NULL);
    if (!rsvg)
        goto locked_out;

    calc_dimensions (rsvg, target_width, target_height, &width, &height);
    if (width < 1 || width >= (1 << 28)
        || height < 1 || height >= (1 << 28)
        || (width * (guint64) height * BYTES_PER_PIXEL > IMAGE_BUFFER_SIZE_MAX))
        goto locked_out;

    loader->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    if (!loader->surface)
        goto locked_out;

    cr = cairo_create (loader->surface);

#if LIBRSVG_CHECK_VERSION(2, 46, 0)
    viewport.width = width;
    viewport.height = height;
    if (!rsvg_handle_render_document (rsvg, cr, &viewport, NULL))
        goto locked_out;
#else
    if (!rsvg_handle_render_cairo (rsvg, cr))
        goto locked_out;
#endif

    success = TRUE;

locked_out:
    g_mutex_unlock (&mutex);

out:
    if (cr)
        cairo_destroy (cr);

    if (!success)
    {
        if (loader)
        {
            if (loader->surface)
                cairo_surface_destroy (loader->surface);
            g_free (loader);
            loader = NULL;
        }
    }

    if (rsvg)
        g_object_unref (rsvg);

    return loader;
}

void
chicle_svg_loader_destroy (ChicleSvgLoader *loader)
{
    if (loader->mapping)
        chicle_file_mapping_destroy (loader->mapping);

    if (loader->surface)
        cairo_surface_destroy (loader->surface);

    g_free (loader);
}

gboolean
chicle_svg_loader_get_is_animation (ChicleSvgLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return FALSE;
}

gconstpointer
chicle_svg_loader_get_frame_data (ChicleSvgLoader *loader,
                                  ChafaPixelType *pixel_type_out,
                                  gint *width_out,
                                  gint *height_out,
                                  gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    if (pixel_type_out)
        *pixel_type_out = PIXEL_TYPE;
    if (width_out)
        *width_out = cairo_image_surface_get_width (loader->surface);
    if (height_out)
        *height_out = cairo_image_surface_get_height (loader->surface);
    if (rowstride_out)
        *rowstride_out = cairo_image_surface_get_stride (loader->surface);

    return cairo_image_surface_get_data (loader->surface);
}

gint
chicle_svg_loader_get_frame_delay (ChicleSvgLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return 0;
}

void
chicle_svg_loader_goto_first_frame (ChicleSvgLoader *loader)
{
    g_return_if_fail (loader != NULL);
}

gboolean
chicle_svg_loader_goto_next_frame (ChicleSvgLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    return FALSE;
}
