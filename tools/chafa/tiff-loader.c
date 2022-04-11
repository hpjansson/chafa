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

#include <tiffio.h>

#include <chafa.h>
#include "tiff-loader.h"

/* ----------------------- *
 * Global macros and types *
 * ----------------------- */

#define BYTES_PER_PIXEL 4

struct TiffLoader
{
    FileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    gpointer frame_data;
    gint width, height;
    ChafaPixelType pixel_type;

    toff_t file_pos;
};

/* ----------- *
 * TIFF loader *
 * ----------- */

/* --- Memory source --- */

static tsize_t
my_tiff_read (thandle_t obj, tdata_t buffer, tsize_t size)
{
    TiffLoader *loader = (TiffLoader *) obj;

    size = CLAMP (size, 0, (tsize_t) loader->file_data_len - (tsize_t) loader->file_pos);
    memcpy (buffer, loader->file_data + loader->file_pos, size);
    loader->file_pos += size;

    return size;
}

static tsize_t
my_tiff_write (G_GNUC_UNUSED thandle_t obj, G_GNUC_UNUSED tdata_t buffer, G_GNUC_UNUSED tsize_t size)
{
    return 0;
}

static int
my_tiff_close (G_GNUC_UNUSED thandle_t obj)
{
    return 0;
}

static toff_t
my_tiff_seek (thandle_t obj, toff_t pos, int whence)
{
    TiffLoader *loader = (TiffLoader *) obj;

    if (whence == SEEK_SET)
    {
        loader->file_pos = pos;
    }
    else if (whence == SEEK_CUR)
    {
        /* Since toff_t is unsigned, we can't seek backwards */
        loader->file_pos += pos;
    }
    else /* whence == SEEK_END */
    {
        /* Since toff_t is unsigned, this is all we can do */
        loader->file_pos = loader->file_data_len;
    }

    loader->file_pos = MIN (loader->file_pos, loader->file_data_len);
    return loader->file_pos;
}

static toff_t
my_tiff_size (thandle_t obj)
{
    TiffLoader *loader = (TiffLoader *) obj;

    return loader->file_data_len;
}

static int
my_tiff_map (thandle_t obj, void **base, toff_t *len)
{
    TiffLoader *loader = (TiffLoader *) obj;

    /* When the TIFFMap delegate is non-NULL, libtiff will use it preferentially.
     *
     * Our map is read-only, while base points to non-const. Fingers crossed
     * libtiff doesn't actually try to write to it during read operations. */

    *base = (void *) loader->file_data;
    *len = loader->file_data_len;
    return 0;
}

static void
my_tiff_unmap (thandle_t, void *, toff_t)
{
}

/* --- Error handlers --- */

static void
my_tiff_error_handler (G_GNUC_UNUSED const char *module, G_GNUC_UNUSED const char *format, G_GNUC_UNUSED va_list ap)
{
}

static void
my_tiff_warning_handler (G_GNUC_UNUSED const char *module, G_GNUC_UNUSED const char *format, G_GNUC_UNUSED va_list ap)
{
}

/* --- Loader --- */

static TiffLoader *
tiff_loader_new (void)
{
    return g_new0 (TiffLoader, 1);
}

TiffLoader *
tiff_loader_new_from_mapping (FileMapping *mapping)
{
    TiffLoader *loader = NULL;
    gboolean success = FALSE;
    uint8_t *frame_data = NULL;
    TIFF *tiff = NULL;
    gint samples_per_pixel = 4;
    uint32_t width, height;

    g_return_val_if_fail (mapping != NULL, NULL);

    if (!((file_mapping_has_magic (mapping, 0, "II", 2)
           && file_mapping_has_magic (mapping, 2, "\x2a\x00", 2))
          || (file_mapping_has_magic (mapping, 0, "MM", 2)
              && file_mapping_has_magic (mapping, 2, "\x00\x2a", 2))))
        goto out;

    loader = tiff_loader_new ();
    loader->mapping = mapping;

    /* Get file data */

    loader->file_data = file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    /* Prepare to decode */

    TIFFSetErrorHandler (my_tiff_error_handler);
    TIFFSetWarningHandler (my_tiff_warning_handler);

    tiff = TIFFClientOpen ("Memory", "r", (thandle_t) loader,
                           my_tiff_read, my_tiff_write, my_tiff_seek, my_tiff_close,
                           my_tiff_size, my_tiff_map, my_tiff_unmap);
    if (!tiff)
        goto out;

    if (!TIFFGetField (tiff, TIFFTAG_IMAGEWIDTH, &width))
        goto out;
    if (!TIFFGetField (tiff, TIFFTAG_IMAGELENGTH, &height))
        goto out;

    if (width < 1 || width > (1 << 30)
        || height < 1 || height > (1 << 30))
        goto out;

    /* An opaque image with unassociated alpha set to 0xff is equivalent to
     * premultiplied alpha. This will speed up resampling later on.
     *
     * For an opaque image, samples_per_pixel will typically be 1 or 3. Other
     * values may indicate there's an alpha channel -- in those cases, we look
     * for an EXTRASAMPLES field, and if it doesn't explicitly specify
     * premultiplied alpha, we fail safe to unassociated alpha. */

    loader->pixel_type = CHAFA_PIXEL_RGBA8_PREMULTIPLIED;

    TIFFGetField (tiff, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    if (samples_per_pixel == 2 || samples_per_pixel >= 4)
    {
        const uint16_t *extra_samples;
        uint16_t n_extra_samples;

        TIFFGetField (tiff, TIFFTAG_EXTRASAMPLES, &n_extra_samples, &extra_samples);
        if (n_extra_samples < 1 || extra_samples [0] != EXTRASAMPLE_ASSOCALPHA)
            loader->pixel_type = CHAFA_PIXEL_RGBA8_UNASSOCIATED;
    }

    frame_data = _TIFFmalloc (width * height * BYTES_PER_PIXEL);
    if (!frame_data)
        goto out;

    /* Decode and rotate the image */

    if (!TIFFReadRGBAImageOriented (tiff, width, height, (uint32_t *) frame_data, ORIENTATION_TOPLEFT, 0))
        goto out;

    /* Finish up */

    loader->width = width;
    loader->height = height;
    loader->frame_data = frame_data;

    success = TRUE;

out:
    if (tiff)
        TIFFClose (tiff);

    if (!success)
    {
        if (frame_data)
            _TIFFfree (frame_data);

        if (loader)
        {
            g_free (loader);
            loader = NULL;
        }
    }

    return loader;
}

void
tiff_loader_destroy (TiffLoader *loader)
{
    if (loader->mapping)
        file_mapping_destroy (loader->mapping);

    if (loader->frame_data)
        _TIFFfree (loader->frame_data);

    g_free (loader);
}

gboolean
tiff_loader_get_is_animation (TiffLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return FALSE;
}

gconstpointer *
tiff_loader_get_frame_data (TiffLoader *loader, ChafaPixelType *pixel_type_out,
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
tiff_loader_get_frame_delay (TiffLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    return 0;
}

void
tiff_loader_goto_first_frame (TiffLoader *loader)
{
    g_return_if_fail (loader != NULL);
}

gboolean
tiff_loader_goto_next_frame (TiffLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    return FALSE;
}
