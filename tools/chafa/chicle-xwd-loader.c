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
#include "chicle-xwd-loader.h"

#define DEBUG(x)

#define IMAGE_BUFFER_SIZE_MAX (0xffffffffU >> 2)

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

struct ChicleXwdLoader
{
    ChicleFileMapping *mapping;
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
compute_pixel_type (ChicleXwdLoader *loader)
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
#define UNPACK_FIELD_U32(dest, src, field) ((dest)->field = GUINT32_FROM_BE ((src)->field))
#define UNPACK_FIELD_S32(dest, src, field) ((dest)->field = GINT32_FROM_BE ((src)->field))

static gboolean
load_header (ChicleXwdLoader *loader)
{
    XwdHeader *h = &loader->header;
    XwdHeader in;
    const XwdHeader *inp;

    if (!chicle_file_mapping_taste (loader->mapping, &in, 0, sizeof (in)))
        return FALSE;

    inp = &in;

    UNPACK_FIELD_U32 (h, inp, header_size);
    UNPACK_FIELD_U32 (h, inp, file_version);
    UNPACK_FIELD_U32 (h, inp, pixmap_format);
    UNPACK_FIELD_U32 (h, inp, pixmap_depth);
    UNPACK_FIELD_U32 (h, inp, pixmap_width);
    UNPACK_FIELD_U32 (h, inp, pixmap_height);
    UNPACK_FIELD_U32 (h, inp, x_offset);
    UNPACK_FIELD_U32 (h, inp, byte_order);
    UNPACK_FIELD_U32 (h, inp, bitmap_unit);
    UNPACK_FIELD_U32 (h, inp, bitmap_bit_order);
    UNPACK_FIELD_U32 (h, inp, bitmap_pad);
    UNPACK_FIELD_U32 (h, inp, bits_per_pixel);
    UNPACK_FIELD_U32 (h, inp, bytes_per_line);
    UNPACK_FIELD_U32 (h, inp, visual_class);
    UNPACK_FIELD_U32 (h, inp, red_mask);
    UNPACK_FIELD_U32 (h, inp, green_mask);
    UNPACK_FIELD_U32 (h, inp, blue_mask);
    UNPACK_FIELD_U32 (h, inp, bits_per_rgb);
    UNPACK_FIELD_U32 (h, inp, color_map_entries);
    UNPACK_FIELD_U32 (h, inp, n_colors);
    UNPACK_FIELD_U32 (h, inp, window_width);
    UNPACK_FIELD_U32 (h, inp, window_height);
    UNPACK_FIELD_S32 (h, inp, window_x);
    UNPACK_FIELD_S32 (h, inp, window_y);
    UNPACK_FIELD_U32 (h, inp, window_border_width);

    /* Only support the most common/useful subset of XWD files out there;
     * namely, that corresponding to screen dumps from modern X.Org servers.
     * We could check visual_class == 5 too, but the other fields convey all
     * the info we need. */

    ASSERT_HEADER (h->header_size >= sizeof (XwdHeader));
    ASSERT_HEADER (h->header_size <= 65535);

    ASSERT_HEADER (h->file_version == 7);
    ASSERT_HEADER (h->pixmap_depth == 24);

    /* Should be zero for truecolor/directcolor. Cap it to prevent overflows. */
    ASSERT_HEADER (h->color_map_entries <= 256);

    /* Xvfb sets bits_per_rgb to 8, but 'convert' uses 24 for the same image data. One
     * of them is likely misunderstanding. Let's be lenient and accept either. */
    ASSERT_HEADER (h->bits_per_rgb == 8 || h->bits_per_rgb == 24);

    /* These are the pixel formats we allow. */
    ASSERT_HEADER (h->bits_per_pixel == 24 || h->bits_per_pixel == 32);

    /* Enforce sane dimensions. */
    ASSERT_HEADER (h->pixmap_width >= 1 && h->pixmap_width <= 65535);
    ASSERT_HEADER (h->pixmap_height >= 1 && h->pixmap_height <= 65535);

    /* Make sure rowstride can actually hold a row's worth of data but is not padded to
     * something ridiculous. */
    ASSERT_HEADER (h->bytes_per_line >= h->pixmap_width * (h->bits_per_pixel / 8));
    ASSERT_HEADER (h->bytes_per_line <= h->pixmap_width * (h->bits_per_pixel / 8) + 1024);

    /* If each pixel is four bytes, reject unaligned rowstrides */
    ASSERT_HEADER (h->bits_per_pixel != 32 || h->bytes_per_line % 4 == 0);

    /* Make sure the total allocation/map is not too big. */
    ASSERT_HEADER (h->bytes_per_line * h->pixmap_height < (1UL << 31) - 65536 - 256 * 32);

    ASSERT_HEADER (compute_pixel_type (loader) < CHAFA_PIXEL_MAX);

    if (h->pixmap_height * (gsize) h->bytes_per_line > IMAGE_BUFFER_SIZE_MAX)
        return FALSE;

    loader->file_data = chicle_file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        return FALSE;

    ASSERT_HEADER (loader->file_data_len >= h->header_size
                   + h->color_map_entries * sizeof (XwdColor)
                   + h->pixmap_height * (gsize) h->bytes_per_line);

    loader->image_data = (const guint8 *) loader->file_data
        + h->header_size + h->color_map_entries * sizeof (XwdColor);

    return TRUE;
}

static ChicleXwdLoader *
chicle_xwd_loader_new (void)
{
    return g_new0 (ChicleXwdLoader, 1);
}

ChicleXwdLoader *
chicle_xwd_loader_new_from_mapping (ChicleFileMapping *mapping)
{
    ChicleXwdLoader *loader = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail (mapping != NULL, NULL);

    loader = chicle_xwd_loader_new ();
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
chicle_xwd_loader_destroy (ChicleXwdLoader *loader)
{
    if (loader->mapping)
        chicle_file_mapping_destroy (loader->mapping);

    g_free (loader);
}

gboolean
chicle_xwd_loader_get_is_animation (G_GNUC_UNUSED ChicleXwdLoader *loader)
{
    return FALSE;
}

gconstpointer
chicle_xwd_loader_get_frame_data (ChicleXwdLoader *loader,
                                  ChafaPixelType *pixel_type_out,
                                  gint *width_out,
                                  gint *height_out,
                                  gint *rowstride_out)
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
chicle_xwd_loader_get_frame_delay (G_GNUC_UNUSED ChicleXwdLoader *loader)
{
    return 0;
}

void
chicle_xwd_loader_goto_first_frame (G_GNUC_UNUSED ChicleXwdLoader *loader)
{
}

gboolean
chicle_xwd_loader_goto_next_frame (G_GNUC_UNUSED ChicleXwdLoader *loader)
{
    return FALSE;
}

