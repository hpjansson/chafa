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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#include <jpeglib.h>
#include <jerror.h>

#include <chafa.h>
#include "jpeg-loader.h"

/* ----------------------- *
 * Global macros and types *
 * ----------------------- */

#undef ENABLE_DEBUG

#define BYTES_PER_PIXEL 3
#define ROWSTRIDE_ALIGN 16

#define PAD_TO_N(p, n) (((p) + ((n) - 1)) & ~((unsigned) (n) - 1))
#define ROWSTRIDE_PAD(rowstride) (PAD_TO_N ((rowstride), (ROWSTRIDE_ALIGN)))

struct JpegLoader
{
    FileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    gpointer frame_data;
    gint width, height, rowstride;
};

/* ------------------- *
 * Rotation transforms *
 * ------------------- */

typedef enum
{
    ROTATION_NONE = 0,
    ROTATION_0 = 1,
    ROTATION_0_MIRROR = 2,
    ROTATION_180 = 3,
    ROTATION_180_MIRROR = 4,
    ROTATION_270_MIRROR = 5,
    ROTATION_270 = 6,
    ROTATION_90_MIRROR = 7,
    ROTATION_90 = 8,
    ROTATION_UNDEFINED = 9,

    ROTATION_MAX
}
RotationType;

static RotationType
undo_rotation (RotationType rot)
{
    switch (rot)
    {
        case ROTATION_90:
            rot = ROTATION_270;
            break;
        case ROTATION_270:
            rot = ROTATION_90;
            break;
        default:
            break;
    }

    return rot;
}

static void
transform (const guchar *src, gint src_pixstride, gint src_rowstride,
           guchar *dest, gint dest_pixstride, gint dest_rowstride,
           gint src_width, gint src_height, gint pixsize)
{
    const guchar *src_row = src;
    const guchar *src_row_end = src + src_pixstride * src_width;
    const guchar *src_end = src + src_rowstride * src_height;
    guchar *dest_row = dest;

    while (src_row != src_end)
    {
        src = src_row;
        dest = dest_row;

        while (src != src_row_end)
        {
            memcpy (dest, src, pixsize);
            src += src_pixstride;
            dest += dest_pixstride;
        }

        src_row += src_rowstride;
        src_row_end += src_rowstride;
        dest_row += dest_rowstride;
    }
}

static void
rotate_frame (guchar **src, guint *width, guint *height, guint *rowstride, guint n_channels,
              RotationType rot)
{
    gint src_width, src_height;
    gint src_pixstride, src_rowstride;
    guchar *dest, *dest_start;
    gint dest_width, dest_height;
    gint dest_pixstride, dest_rowstride, dest_trans_rowstride;

    g_assert (n_channels == 3 || n_channels == 4);

    if (rot <= ROTATION_NONE
        || rot == ROTATION_0
        || rot >= ROTATION_UNDEFINED)
        return;

    src_width = *width;
    src_height = *height;
    src_pixstride = n_channels;
    src_rowstride = *rowstride;

    switch (rot)
    {
        case ROTATION_90:
        case ROTATION_90_MIRROR:
        case ROTATION_270:
        case ROTATION_270_MIRROR:
            dest_width = src_height;
            dest_height = src_width;
            break;
        case ROTATION_0_MIRROR:
        case ROTATION_180:
        case ROTATION_180_MIRROR:
            dest_width = src_width;
            dest_height = src_height;
            break;
        default:
            g_assert_not_reached ();
    }

    dest_rowstride = ROWSTRIDE_PAD (dest_width * n_channels);
    dest = g_malloc (dest_rowstride * dest_height);

    switch (rot)
    {
        case ROTATION_0_MIRROR:
            dest_pixstride = -n_channels;
            dest_trans_rowstride = dest_rowstride;
            dest_start = dest + ((dest_width - 1) * n_channels);
            break;
        case ROTATION_90:
            dest_pixstride = dest_rowstride;
            dest_trans_rowstride = -n_channels;
            dest_start = dest + ((dest_width - 1) * n_channels);
            break;
        case ROTATION_90_MIRROR:
            dest_pixstride = -dest_rowstride;
            dest_trans_rowstride = -n_channels;
            dest_start = dest + ((dest_height - 1) * dest_rowstride) + ((dest_width - 1) * n_channels);
            break;
        case ROTATION_180:
            dest_pixstride = -n_channels;
            dest_trans_rowstride = -dest_rowstride;
            dest_start = dest + ((dest_height - 1) * dest_rowstride) + ((dest_width - 1) * n_channels);
            break;
        case ROTATION_180_MIRROR:
            dest_pixstride = n_channels;
            dest_trans_rowstride = -dest_rowstride;
            dest_start = dest + ((dest_height - 1) * dest_rowstride);
            break;
        case ROTATION_270:
            dest_pixstride = -dest_rowstride;
            dest_trans_rowstride = n_channels;
            dest_start = dest + ((dest_height - 1) * dest_rowstride);
            break;
        case ROTATION_270_MIRROR:
            dest_pixstride = dest_rowstride;
            dest_trans_rowstride = n_channels;
            dest_start = dest;
            break;
        default:
            g_assert_not_reached ();
    }

    transform (*src, src_pixstride, src_rowstride,
               dest_start, dest_pixstride, dest_trans_rowstride,
               src_width, src_height, n_channels);

    g_free (*src);

    *src = dest;
    *width = dest_width;
    *height = dest_height;
    *rowstride = dest_rowstride;
}

/* ----------------------- *
 * Exif orientation reader *
 * ----------------------- */

static guint16
read_uint16 (const guchar *p, gboolean is_big_endian)
{
    return is_big_endian ?
        ((p [0] << 8) | p [1]) :
        ((p [1] << 8) | p [0]);
}

static guint32
read_uint32 (const guchar *p, gboolean is_big_endian)
{
    return is_big_endian ?
        ((p [0] << 24) | (p [1] << 16) | (p [2] << 8) | p [3]) :
        ((p [3] << 24) | (p [2] << 16) | (p [1] << 8) | p [0]);
}

static RotationType
read_orientation (JpegLoader *loader)
{
    const guchar *p0, *end;
    size_t len;
    RotationType rot = ROTATION_NONE;
    gboolean is_big_endian;
    guint n, m, o;

    p0 = loader->file_data;
    len = loader->file_data_len;
    end = p0 + len;

    /* Assume we already checked the JPEG header. Now find the Exif header. */
    p0 += 2;

    for (;;)
    {
        if (p0 + 20 > end)
            goto out;

        /* Get app type */
        if (read_uint16 (p0, TRUE) < 0xffdb)
            goto out;
        p0 += 2;

        /* Get marker length; note length field includes itself */
        n = read_uint16 (p0, TRUE);
        if (n < 2 || p0 + n > end)
            goto out;

        if (!memcmp (p0 + 2, "Exif\0\0", 6))
        {
            p0 += 8;
            break;
        }

        /* Not an Exif marker; skip it */
        p0 += n;
    }

    /* Get byte order */
    m = read_uint16 (p0, TRUE);
    if (m == 0x4949)
        is_big_endian = FALSE;
    else if (m == 0x4d4d)
        is_big_endian = TRUE;
    else
        goto out;

    /* Tag mark */
    if (read_uint16 (p0 + 2, is_big_endian) != 0x002a)
        goto out;

    /* First IFD offset */
    m = read_uint32 (p0 + 4, is_big_endian);
    if (m > 0xffff)
        goto out;
    if (p0 + m + 2 > end)
        goto out;
    if (m + 2 > n)
        goto out;

    /* Number of directory entries in this IFD */
    o = read_uint16 (p0 + m, is_big_endian);
    m += 2;

    for (;;)
    {
        guint16 tagnum;

        if (!o)
            goto out;
        if (p0 + m + 12 > end)
            goto out;
        if (m + 12 > n)
            goto out;

        tagnum = read_uint16 (p0 + m, is_big_endian);
        if (tagnum == 0x0112)
            break;

        o--;
        m += 12;
    }

    m = read_uint16 (p0 + m + 8, is_big_endian);
    if (m > 9)
        goto out;

    rot = m;

out:
    return rot;
}

/* ----------- *
 * JPEG loader *
 * ----------- */

/* --- Memory source --- */

/* libjpeg-turbo can provide jpeg_mem_src (), and usually does. However, we
 * supply our own for strict conformance with libjpeg v6b. */

static void
init_source (G_GNUC_UNUSED j_decompress_ptr cinfo)
{
}

static boolean
fill_input_buffer (j_decompress_ptr cinfo)
{
    ERREXIT (cinfo, JERR_INPUT_EMPTY);
    return TRUE;
}

static void
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
    struct jpeg_source_mgr *src = (struct jpeg_source_mgr *) cinfo->src;

    if (num_bytes < 0)
        return;

    num_bytes = MIN ((size_t) num_bytes, src->bytes_in_buffer);

    src->next_input_byte += (size_t) num_bytes;
    src->bytes_in_buffer -= (size_t) num_bytes;
}

static void
term_source (G_GNUC_UNUSED j_decompress_ptr cinfo)
{
}

static void
my_jpeg_mem_src (j_decompress_ptr cinfo, const void *buffer, long nbytes)
{
    struct jpeg_source_mgr *src;

    if (cinfo->src == NULL)
    {
        cinfo->src = (struct jpeg_source_mgr *)
            (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                        sizeof (struct jpeg_source_mgr));
    }

    src = (struct jpeg_source_mgr *) cinfo->src;
    src->init_source = init_source;
    src->fill_input_buffer = fill_input_buffer;
    src->skip_input_data = skip_input_data;
    src->resync_to_restart = jpeg_resync_to_restart;  /* Use default method */
    src->term_source = term_source;
    src->bytes_in_buffer = nbytes;
    src->next_input_byte = (JOCTET *) buffer;
}

/* --- Error handler --- */

struct my_jpeg_error_mgr
{
    struct jpeg_error_mgr jerr;
    jmp_buf setjmp_buffer;
};

static void
my_jpeg_error_exit (j_common_ptr cinfo)
{
    struct my_jpeg_error_mgr *my_jerr = (struct my_jpeg_error_mgr *) cinfo->err;

    longjmp (my_jerr->setjmp_buffer, 1);
}

/* --- Magic probe --- */

static gboolean
have_any_apptype_magic (FileMapping *mapping)
{
    guchar magic [4] = { 0xff, 0xd8, 0xff, 0x00 };
    guchar n;

    for (n = 0xe0; n <= 0xef; n++)
    {
        magic [3] = n;
        if (file_mapping_has_magic (mapping, 0, magic, 4))
            return TRUE;
    }

    magic [3] = 0xdb;
    return file_mapping_has_magic (mapping, 0, magic, 4);
}

/* --- Loader --- */

static JpegLoader *
jpeg_loader_new (void)
{
    return g_new0 (JpegLoader, 1);
}

JpegLoader *
jpeg_loader_new_from_mapping (FileMapping *mapping)
{
    guint width, height;
    guint rowstride;
    struct jpeg_decompress_struct cinfo;
    struct my_jpeg_error_mgr my_jerr;
    JpegLoader * volatile loader = NULL;
    unsigned char * volatile frame_data = NULL;
    volatile gboolean have_decompress = FALSE;
    volatile gboolean success = FALSE;

    g_return_val_if_fail (mapping != NULL, NULL);

    /* Check magic */

    if (!have_any_apptype_magic (mapping))
        goto out;

    loader = jpeg_loader_new ();
    loader->mapping = mapping;

    /* Get file data */

    loader->file_data = file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    /* Prepare to decode */

    cinfo.err = jpeg_std_error ((struct jpeg_error_mgr *) &my_jerr);     
    my_jerr.jerr.error_exit = my_jpeg_error_exit;
    if (setjmp (my_jerr.setjmp_buffer))
    {
#ifdef ENABLE_DEBUG
        static gchar jpeg_error_msg [JMSG_LENGTH_MAX];

        (*cinfo.err->format_message) ((j_common_ptr) &cinfo, jpeg_error_msg);
        g_printerr ("JPEG error: %s\n", jpeg_error_msg);
#endif
        goto out;
    }

    jpeg_create_decompress (&cinfo);
    have_decompress = TRUE;

    my_jpeg_mem_src (&cinfo, loader->file_data, loader->file_data_len);
    (void) jpeg_read_header (&cinfo, TRUE);

    cinfo.out_color_space = JCS_RGB;
    cinfo.output_components = 3;

    jpeg_start_decompress (&cinfo);

    width = cinfo.output_width;
    height = cinfo.output_height;

    if (width == 0 || width > (1 << 30)
        || height == 0 || height > (1 << 30))
        goto out;

    rowstride = ROWSTRIDE_PAD (width * BYTES_PER_PIXEL);
    frame_data = g_malloc (height * rowstride);

    /* Decoding loop */

    while (cinfo.output_scanline < cinfo.output_height)
    {
        guchar *row_data = frame_data + cinfo.output_scanline * rowstride;
        if (jpeg_read_scanlines (&cinfo, &row_data, 1) < 1)
            goto out;
    }

    /* Orientation and cleanup */

    (void) jpeg_finish_decompress (&cinfo);

    rotate_frame ((guchar **) &frame_data, &width, &height, &rowstride, 3,
                  undo_rotation (read_orientation (loader)));

    loader->frame_data = frame_data;
    loader->width = (gint) width;
    loader->height = (gint) height;
    loader->rowstride = (gint) rowstride;

    success = TRUE;

out:
    if (have_decompress)
        jpeg_destroy_decompress (&cinfo);

    if (!success)
    {
        if (frame_data)
            g_free (frame_data);

        if (loader)
        {
            g_free (loader);
            loader = NULL;
        }
    }

    return loader;
}

void
jpeg_loader_destroy (JpegLoader *loader)
{
    if (loader->mapping)
        file_mapping_destroy (loader->mapping);

    if (loader->frame_data)
        free (loader->frame_data);

    g_free (loader);
}

gboolean
jpeg_loader_get_is_animation (JpegLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return FALSE;
}

gconstpointer
jpeg_loader_get_frame_data (JpegLoader *loader, ChafaPixelType *pixel_type_out,
                            gint *width_out, gint *height_out, gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    if (pixel_type_out)
        *pixel_type_out = CHAFA_PIXEL_RGB8;
    if (width_out)
        *width_out = loader->width;
    if (height_out)
        *height_out = loader->height;
    if (rowstride_out)
        *rowstride_out = loader->rowstride;

    return loader->frame_data;
}

gint
jpeg_loader_get_frame_delay (JpegLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return 0;
}

void
jpeg_loader_goto_first_frame (JpegLoader *loader)
{
    g_return_if_fail (loader != NULL);
}

gboolean
jpeg_loader_goto_next_frame (JpegLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    return FALSE;
}
