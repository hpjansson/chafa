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

#include <chafa.h>
#include "xwd-loader.h"

#define DEBUG(x)

typedef struct
{
    guint32 header_size;          /* Size of the header in bytes */
    guint32 file_version;         /* X11WD file version (always 07h) */
    guint32 pixmap_format;        /* Pixmap format */
    guint32 pixmap_depth;         /* Pixmap depth in pixels */
    guint32 pixmap_width;         /* Pixmap width in pixels */
    guint32 pixmap_height;        /* Pixmap height in pixels */
    guint32 x_offset;             /* Bitmap X offset */
    guint32 byte_order;           /* Byte order of image data */
    guint32 bitmap_unit;          /* Bitmap base data size */
    guint32 bitmap_bit_order;     /* Bit-order of image data */
    guint32 bitmap_pad;           /* Bitmap scan-line pad*/
    guint32 bits_per_pixel;       /* Bits per pixel */
    guint32 bytes_per_line;       /* Bytes per scan-line */
    guint32 visual_class;         /* Class of the image */
    guint32 red_mask;             /* Red mask */
    guint32 green_mask;           /* Green mask */
    guint32 blue_mask;            /* Blue mask */
    guint32 bits_per_rgb;         /* Size of each color mask in bits */
    guint32 n_colors;             /* Number of colors in image */
    guint32 color_map_entries;    /* Number of entries in color map */
    guint32 window_width;         /* Window width */
    guint32 window_height;        /* Window height */
    gint32  window_x;             /* Window upper left X coordinate */
    gint32  window_y;             /* Window upper left Y coordinate */
    guint32 window_border_width;  /* Window border width */
}
XwdHeader;

typedef struct
{
    guint32 pixel;
    guint16 red;
    guint16 green;
    guint16 blue;
    guint8 flags;
    guint8 pad;
}
XwdColor;

struct XwdLoader
{
    FileMapping *mapping;
    gconstpointer file_data;
    gconstpointer image_data;
    gsize file_data_len;
    XwdHeader header;
};

DEBUG (
static void
dump_header (XwdHeader *header)
{
    g_printerr ("Header size: %u\n"
                "File version: %u\n"
                "Pixmap format: %u\n"
                "Pixmap depth: %u\n"
                "Pixmap width: %u\n"
                "Pixmap height: %u\n"
                "X offset: %u\n"
                "Byte order: %u\n"
                "Bitmap unit: %u\n"
                "Bitmap bit order: %u\n"
                "Bitmap pad: %u\n"
                "Bits per pixel: %u\n"
                "Bytes per line: %u\n"
                "Visual class: %u\n"
                "Red mask: %u\n"
                "Green mask: %u\n"
                "Blue mask: %u\n"
                "Bits per RGB: %u\n"
                "Number of colors: %u\n"
                "Color map entries: %u\n"
                "Window width: %u\n"
                "Window height: %u\n"
                "Window X: %d\n"
                "Window Y: %d\n"
                "Window border width: %u\n---\n",
                header->header_size,
                header->file_version,
                header->pixmap_format,
                header->pixmap_depth,
                header->pixmap_width,
                header->pixmap_height,
                header->x_offset,
                header->byte_order,
                header->bitmap_unit,
                header->bitmap_bit_order,
                header->bitmap_pad,
                header->bits_per_pixel,
                header->bytes_per_line,
                header->visual_class,
                header->red_mask,
                header->green_mask,
                header->blue_mask,
                header->bits_per_rgb,
                header->n_colors,
                header->color_map_entries,
                header->window_width,
                header->window_height,
                header->window_x,
                header->window_y,
                header->window_border_width);
}
)

static ChafaPixelType
compute_pixel_type (XwdLoader *loader)
{
    XwdHeader *h = &loader->header;

    if (h->bits_per_pixel == 24)
    {
        if (h->byte_order == 0)
            return CHAFA_PIXEL_BGR8;
        else
            return CHAFA_PIXEL_RGB8;
    }

    if (h->bits_per_pixel == 32)
    {
        if (h->byte_order == 0)
            return CHAFA_PIXEL_BGRA8_PREMULTIPLIED;
        else
            return CHAFA_PIXEL_ARGB8_PREMULTIPLIED;
    }

    return CHAFA_PIXEL_MAX;
}

#define ASSERT_HEADER(x) if (!(x)) return FALSE

static gboolean
load_header (XwdLoader *loader) // gconstpointer in, gsize in_max_len, XwdHeader *header_out)
{
    XwdHeader *h = &loader->header;
    XwdHeader in;
    const guint32 *p = (const guint32 *) &in;

    if (!file_mapping_taste (loader->mapping, &in, 0, sizeof (in)))
        return FALSE;

    h->header_size = g_ntohl (*(p++));
    h->file_version = g_ntohl (*(p++));
    h->pixmap_format = g_ntohl (*(p++));
    h->pixmap_depth = g_ntohl (*(p++));
    h->pixmap_width = g_ntohl (*(p++));
    h->pixmap_height = g_ntohl (*(p++));
    h->x_offset = g_ntohl (*(p++));
    h->byte_order = g_ntohl (*(p++));
    h->bitmap_unit = g_ntohl (*(p++));
    h->bitmap_bit_order = g_ntohl (*(p++));
    h->bitmap_pad = g_ntohl (*(p++));
    h->bits_per_pixel = g_ntohl (*(p++));
    h->bytes_per_line = g_ntohl (*(p++));
    h->visual_class = g_ntohl (*(p++));
    h->red_mask = g_ntohl (*(p++));
    h->green_mask = g_ntohl (*(p++));
    h->blue_mask = g_ntohl (*(p++));
    h->bits_per_rgb = g_ntohl (*(p++));
    h->color_map_entries = g_ntohl (*(p++));
    h->n_colors = g_ntohl (*(p++));
    h->window_width = g_ntohl (*(p++));
    h->window_height = g_ntohl (*(p++));
    h->window_x = g_ntohl (*(p++));
    h->window_y = g_ntohl (*(p++));
    h->window_border_width = g_ntohl (*(p++));

    /* Only support the most common/useful subset of XWD files out there;
     * namely, that corresponding to screen dumps from modern X.Org servers. */

    ASSERT_HEADER (h->header_size >= sizeof (XwdHeader));
    ASSERT_HEADER (h->file_version == 7);
    ASSERT_HEADER (h->pixmap_depth == 24);

    /* Xvfb sets bits_per_rgb to 8, but 'convert' uses 24 for the same image data. One
     * of them is likely misunderstanding. Let's be lenient and accept either. */
    ASSERT_HEADER (h->bits_per_rgb == 8 || h->bits_per_rgb == 24);

    ASSERT_HEADER (h->bytes_per_line >= h->pixmap_width * (h->bits_per_pixel / 8));
    ASSERT_HEADER (compute_pixel_type (loader) < CHAFA_PIXEL_MAX);

    loader->file_data = file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        return FALSE;

    ASSERT_HEADER (loader->file_data_len >= h->header_size
                   + h->n_colors * sizeof (XwdColor)
                   + h->pixmap_height * h->bytes_per_line);

    loader->image_data = (const guint8 *) loader->file_data
        + h->header_size + h->n_colors * sizeof (XwdColor);

    return TRUE;
}

static XwdLoader *
xwd_loader_new (void)
{
    return g_new0 (XwdLoader, 1);
}

XwdLoader *
xwd_loader_new_from_mapping (FileMapping *mapping)
{
    XwdLoader *loader = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail (mapping != NULL, NULL);

    loader = xwd_loader_new ();
    loader->mapping = mapping;

    if (!load_header (loader))
    {
        g_free (loader);
        return NULL;
    }

    DEBUG (dump_header (&loader->header));

    if (loader->header.pixmap_width < 1 || loader->header.pixmap_width >= (1 << 28)
        || loader->header.pixmap_height < 1 || loader->header.pixmap_height >= (1 << 28)
        || (loader->header.pixmap_width * (guint64) loader->header.pixmap_height >= (1 << 29)))
        goto out;

    success = TRUE;

out:
    if (!success)
    {
        g_free (loader);
        loader = NULL;
    }

    return loader;
}

void
xwd_loader_destroy (XwdLoader *loader)
{
    if (loader->mapping)
        file_mapping_destroy (loader->mapping);

    g_free (loader);
}

gboolean
xwd_loader_get_is_animation (G_GNUC_UNUSED XwdLoader *loader)
{
    return FALSE;
}

gconstpointer
xwd_loader_get_frame_data (XwdLoader *loader, ChafaPixelType *pixel_type_out,
                           gint *width_out, gint *height_out, gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    if (pixel_type_out)
        *pixel_type_out = compute_pixel_type (loader);
    if (width_out)
        *width_out = loader->header.pixmap_width;
    if (height_out)
        *height_out = loader->header.pixmap_height;
    if (rowstride_out)
        *rowstride_out = loader->header.bytes_per_line;

    return loader->image_data;
}

gint
xwd_loader_get_frame_delay (G_GNUC_UNUSED XwdLoader *loader)
{
    return 0;
}

void
xwd_loader_goto_first_frame (G_GNUC_UNUSED XwdLoader *loader)
{
}

gboolean
xwd_loader_goto_next_frame (G_GNUC_UNUSED XwdLoader *loader)
{
    return FALSE;
}

