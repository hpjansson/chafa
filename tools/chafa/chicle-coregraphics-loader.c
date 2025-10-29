/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2025 Hans Petter Jansson
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

#ifdef HAVE_COREGRAPHICS

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <glib.h>
#include <chafa.h>

#include "chicle-coregraphics-loader.h"
#include "chicle-util.h"

#define BYTES_PER_PIXEL 4
#define IMAGE_BUFFER_SIZE_MAX (0xffffffffU >> 2)
#define ROWSTRIDE_ALIGN 16

#define PAD_TO_N(p, n) (((p) + ((n) - 1)) & ~((unsigned) (n) - 1))
#define ROWSTRIDE_PAD(rowstride) (PAD_TO_N ((rowstride), (ROWSTRIDE_ALIGN)))
#define DEFAULT_FRAME_DELAY_MS 100

#if G_BYTE_ORDER == G_BIG_ENDIAN
# define BITMAP_INFO (kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Big)
# define PIXEL_TYPE CHAFA_PIXEL_ARGB8_PREMULTIPLIED
#else
# define BITMAP_INFO (kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little)
# define PIXEL_TYPE CHAFA_PIXEL_BGRA8_PREMULTIPLIED
#endif

struct _ChicleCoreGraphicsLoader
{
    ChicleFileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    CFDataRef cf_data;
    CGImageSourceRef image_source;

    guint frame_index;
    guint frame_count;
    guint is_animation : 1;
    guint frame_ready : 1;

    guchar *frame_data;
    gint width;
    gint height;
    gint rowstride;
    gint frame_delay_ms;
};

static ChicleCoreGraphicsLoader *chicle_coregraphics_loader_new (void);
static gboolean ensure_frame_ready (ChicleCoreGraphicsLoader *loader);
static gboolean decode_frame (ChicleCoreGraphicsLoader *loader);
static ChicleRotationType extract_orientation (CFDictionaryRef properties);
static gdouble extract_frame_delay (CFDictionaryRef properties);
static gdouble get_delay_from_dictionary (CFDictionaryRef dict,
                                          CFStringRef unclamped_key,
                                          CFStringRef clamped_key);

static ChicleCoreGraphicsLoader *
chicle_coregraphics_loader_new (void)
{
    return g_new0 (ChicleCoreGraphicsLoader, 1);
}

ChicleCoreGraphicsLoader *
chicle_coregraphics_loader_new_from_mapping (ChicleFileMapping *mapping,
                                             G_GNUC_UNUSED gint target_width,
                                             G_GNUC_UNUSED gint target_height)
{
    ChicleCoreGraphicsLoader *loader = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail (mapping != NULL, NULL);

    loader = chicle_coregraphics_loader_new ();
    loader->mapping = mapping;

    loader->file_data = chicle_file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    loader->cf_data = CFDataCreateWithBytesNoCopy (kCFAllocatorDefault,
                                                   loader->file_data,
                                                   (CFIndex) loader->file_data_len,
                                                   kCFAllocatorNull);
    if (!loader->cf_data)
        goto out;

    loader->image_source = CGImageSourceCreateWithData (loader->cf_data, NULL);
    if (!loader->image_source)
        goto out;

    loader->frame_count = (guint) CGImageSourceGetCount (loader->image_source);
    if (loader->frame_count < 1)
        goto out;

    if (loader->frame_count > 1)
        loader->is_animation = TRUE;

    success = TRUE;

out:
    if (!success)
    {
        if (loader)
        {
            if (loader->image_source)
                CFRelease (loader->image_source);
            loader->image_source = NULL;

            if (loader->cf_data)
                CFRelease (loader->cf_data);
            loader->cf_data = NULL;

            loader->mapping = NULL;  /* Ownership stays with caller */
            g_free (loader);
            loader = NULL;
        }
    }

    return loader;
}

void
chicle_coregraphics_loader_destroy (ChicleCoreGraphicsLoader *loader)
{
    if (!loader)
        return;

    if (loader->mapping)
        chicle_file_mapping_destroy (loader->mapping);

    if (loader->image_source)
        CFRelease (loader->image_source);

    if (loader->cf_data)
        CFRelease (loader->cf_data);

    g_free (loader->frame_data);

    g_free (loader);
}

static gboolean
ensure_frame_ready (ChicleCoreGraphicsLoader *loader)
{
    if (!loader->frame_ready)
    {
        if (!decode_frame (loader))
            return FALSE;
    }

    return TRUE;
}

static gboolean
decode_frame (ChicleCoreGraphicsLoader *loader)
{
    CGColorSpaceRef color_space = NULL;
    CGContextRef context = NULL;
    CGImageRef image = NULL;
    CFDictionaryRef properties = NULL;
    gpointer pixel_ptr;
    guchar *pixels = NULL;
    size_t width, height, rowstride;
    guint width_u, height_u, rowstride_u;
    gdouble delay_seconds;
    gboolean success = FALSE;

    image = CGImageSourceCreateImageAtIndex (loader->image_source, loader->frame_index, NULL);
    if (!image)
        goto out;

    width = CGImageGetWidth (image);
    height = CGImageGetHeight (image);

    if (width < 1 || width >= (1u << 28)
        || height < 1 || height >= (1u << 28))
        goto out;

    rowstride = ROWSTRIDE_PAD (width * BYTES_PER_PIXEL);
    if (height > 0 && ((guint64) rowstride * (guint64) height) > IMAGE_BUFFER_SIZE_MAX)
        goto out;

    pixels = g_malloc ((gsize) rowstride * (gsize) height);

    color_space = CGColorSpaceCreateDeviceRGB ();
    if (!color_space)
        goto out;

    context = CGBitmapContextCreate (pixels,
                                     width,
                                     height,
                                     8,
                                     rowstride,
                                     color_space,
                                     BITMAP_INFO);
    if (!context)
        goto out;

    CGContextDrawImage (context, CGRectMake (0, 0, width, height), image);

    properties = CGImageSourceCopyPropertiesAtIndex (loader->image_source,
                                                     loader->frame_index,
                                                     NULL);

    pixel_ptr = pixels;
    width_u = (guint) width;
    height_u = (guint) height;
    rowstride_u = (guint) rowstride;

    chicle_rotate_image (&pixel_ptr,
                         &width_u,
                         &height_u,
                         &rowstride_u,
                         BYTES_PER_PIXEL,
                         chicle_invert_rotation (extract_orientation (properties)));

    pixels = pixel_ptr;

    delay_seconds = extract_frame_delay (properties);

    g_free (loader->frame_data);
    loader->frame_data = pixels;
    loader->width = (gint) width_u;
    loader->height = (gint) height_u;
    loader->rowstride = (gint) rowstride_u;
    loader->frame_delay_ms = 0;

    if (delay_seconds > 0.0)
    {
        gdouble delay_ms = delay_seconds * 1000.0;

        if (delay_ms > (gdouble) G_MAXINT)
            loader->frame_delay_ms = G_MAXINT;
        else
            loader->frame_delay_ms = (gint) (delay_ms + 0.5);
    }
    else if (loader->is_animation)
    {
        loader->frame_delay_ms = DEFAULT_FRAME_DELAY_MS;
    }

    loader->frame_ready = TRUE;
    pixels = NULL;  /* Ownership moved to loader */

    success = TRUE;

out:
    if (!success && pixels)
        g_free (pixels);

    if (properties)
        CFRelease (properties);
    if (context)
        CGContextRelease (context);
    if (color_space)
        CGColorSpaceRelease (color_space);
    if (image)
        CGImageRelease (image);

    if (!success)
    {
        loader->frame_ready = FALSE;
        loader->frame_delay_ms = 0;
    }

    return success;
}

gboolean
chicle_coregraphics_loader_get_is_animation (ChicleCoreGraphicsLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    return loader->is_animation ? TRUE : FALSE;
}

gconstpointer
chicle_coregraphics_loader_get_frame_data (ChicleCoreGraphicsLoader *loader,
                                           ChafaPixelType *pixel_type_out,
                                           gint *width_out,
                                           gint *height_out,
                                           gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);

    if (!ensure_frame_ready (loader))
        return NULL;

    if (pixel_type_out)
        *pixel_type_out = PIXEL_TYPE;
    if (width_out)
        *width_out = loader->width;
    if (height_out)
        *height_out = loader->height;
    if (rowstride_out)
        *rowstride_out = loader->rowstride;

    return loader->frame_data;
}

gint
chicle_coregraphics_loader_get_frame_delay (ChicleCoreGraphicsLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);

    if (!ensure_frame_ready (loader))
        return 0;

    return loader->frame_delay_ms;
}

void
chicle_coregraphics_loader_goto_first_frame (ChicleCoreGraphicsLoader *loader)
{
    g_return_if_fail (loader != NULL);

    loader->frame_index = 0;
    loader->frame_ready = FALSE;
    loader->frame_delay_ms = 0;

    if (loader->frame_data)
    {
        g_free (loader->frame_data);
        loader->frame_data = NULL;
    }
}

gboolean
chicle_coregraphics_loader_goto_next_frame (ChicleCoreGraphicsLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);

    if (loader->frame_index + 1 >= loader->frame_count)
        return FALSE;

    loader->frame_index++;
    loader->frame_ready = FALSE;
    loader->frame_delay_ms = 0;

    if (loader->frame_data)
    {
        g_free (loader->frame_data);
        loader->frame_data = NULL;
    }

    return TRUE;
}

static ChicleRotationType
extract_orientation (CFDictionaryRef properties)
{
    ChicleRotationType rotation = CHICLE_ROTATION_NONE;

    if (properties)
    {
        const void *value = CFDictionaryGetValue (properties, kCGImagePropertyOrientation);

        if (value && CFGetTypeID (value) == CFNumberGetTypeID ())
        {
            gint orientation = 0;

            if (CFNumberGetValue ((CFNumberRef) value, kCFNumberIntType, &orientation)
                && orientation > CHICLE_ROTATION_NONE
                && orientation < CHICLE_ROTATION_UNDEFINED)
            {
                rotation = (ChicleRotationType) orientation;
            }
        }
    }

    return rotation;
}

static gdouble
get_delay_from_dictionary (CFDictionaryRef dict,
                           CFStringRef unclamped_key,
                           CFStringRef clamped_key)
{
    gdouble delay = 0.0;
    const void *value;

    if (!dict)
        return 0.0;

    value = CFDictionaryGetValue (dict, unclamped_key);
    if (value && CFGetTypeID (value) == CFNumberGetTypeID ())
    {
        if (CFNumberGetValue ((CFNumberRef) value, kCFNumberDoubleType, &delay) && delay > 0.0)
            return delay;
    }

    value = CFDictionaryGetValue (dict, clamped_key);
    if (value && CFGetTypeID (value) == CFNumberGetTypeID ())
    {
        if (CFNumberGetValue ((CFNumberRef) value, kCFNumberDoubleType, &delay) && delay > 0.0)
            return delay;
    }

    return 0.0;
}

static gdouble
extract_frame_delay (CFDictionaryRef properties)
{
    gdouble delay = 0.0;

    if (!properties)
        return 0.0;

    const void *dict_value = CFDictionaryGetValue (properties, kCGImagePropertyGIFDictionary);
    if (dict_value && CFGetTypeID (dict_value) == CFDictionaryGetTypeID ())
    {
        delay = get_delay_from_dictionary ((CFDictionaryRef) dict_value,
                                           kCGImagePropertyGIFUnclampedDelayTime,
                                           kCGImagePropertyGIFDelayTime);
        if (delay > 0.0)
            return delay;
    }

    return 0.0;
}

#endif /* HAVE_COREGRAPHICS */
