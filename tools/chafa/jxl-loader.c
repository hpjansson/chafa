/* -*- Mode: C; tab-width: 4; ent su: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2024 oupson
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

#include "chafa.h"
#include "file-mapping.h"
#include "glib.h"
#include <jxl/codestream_header.h>
#include <jxl/decode.h>
#include <jxl/resizable_parallel_runner.h>
#include <stdio.h>

#include "jxl-loader.h"

#define BUFFER_SIZE (1024)

GList *jxl_get_frames (JxlDecoder *dec, JxlParallelRunner *runner, FileMapping *mapping);

void jxl_cleanup_frame_list (GList *frame_list);

struct JxlLoader
{
    GList *frames;
    int frame_count;
    int index;
};

typedef struct JxlFrame
{
    uint8_t *buffer;
    int width;
    int height;
    gboolean have_alpha;
    gboolean is_premul;
    int frame_duration;
} JxlFrame;

static JxlLoader *
jxl_loader_new (void)
{
    return g_new0 (JxlLoader, 1);
}

GList *
jxl_get_frames (JxlDecoder *dec, JxlParallelRunner *runner, FileMapping *mapping)
{
    if (JXL_DEC_SUCCESS
        != JxlDecoderSetParallelRunner (dec, JxlResizableParallelRunner, runner))
    {
        return NULL;
    }

    if (JXL_DEC_SUCCESS
        != JxlDecoderSubscribeEvents (dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE
                                               | JXL_DEC_FRAME))
    {
        return NULL;
    }

    JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };

    JxlDecoderStatus decode_status = JXL_DEC_NEED_MORE_INPUT;
    uint8_t buffer [BUFFER_SIZE];

    gpointer image_buffer = NULL;
    GList *frame_list = NULL;
    JxlBasicInfo info;
    JxlFrameHeader frame_header;

    int read = 0;

    while (JXL_DEC_ERROR != decode_status)
    {
        if (JXL_DEC_NEED_MORE_INPUT == decode_status)
        {
            JxlDecoderReleaseInput (dec);
            const gsize nbr = file_mapping_read (mapping, buffer, read, BUFFER_SIZE);
            read += nbr;
            if (nbr > 0)
            {
                if (JXL_DEC_SUCCESS != JxlDecoderSetInput (dec, buffer, nbr))
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
        else if (JXL_DEC_BASIC_INFO == decode_status)
        {
            if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo (dec, &info))
            {
                break;
            }
            if (!info.alpha_bits)
            {
                format.num_channels = 3;
            }
            image_buffer = g_malloc (info.xsize * info.ysize * format.num_channels);
        }
        else if (JXL_DEC_NEED_IMAGE_OUT_BUFFER == decode_status)
        {
            if (JXL_DEC_SUCCESS
                != JxlDecoderSetImageOutBuffer (dec, &format, image_buffer,
                                                info.xsize * info.ysize
                                                    * format.num_channels))
            {
                break;
            }
        }
        else if (JXL_DEC_FRAME == decode_status)
        {
            if (JXL_DEC_SUCCESS != JxlDecoderGetFrameHeader (dec, &frame_header))
            {
                break;
            }
        }
        else if (JXL_DEC_FULL_IMAGE == decode_status)
        {
            const uint32_t num
                = info.animation.tps_numerator == 0 ? 1 : info.animation.tps_numerator;
            JxlFrame *frame = g_new (JxlFrame, 1);
            frame->buffer = image_buffer;
            frame->width = info.xsize;
            frame->height = info.ysize;
            frame->have_alpha = format.num_channels == 4;
            frame->is_premul = info.alpha_premultiplied;
            frame->frame_duration
                = frame_header.duration * 1000 * info.animation.tps_denominator / num;
            frame_list = g_list_prepend (frame_list, frame);

            image_buffer = g_malloc (info.xsize * info.ysize * format.num_channels);
        }
        else if (JXL_DEC_SUCCESS == decode_status)
        {
            frame_list = g_list_reverse (frame_list);
            return frame_list;
        }

        decode_status = JxlDecoderProcessInput (dec);
    }

    // Decoding failed
    if (image_buffer != NULL)
    {
        g_free (image_buffer);
    }

    if (frame_list != NULL)
    {
        jxl_cleanup_frame_list (frame_list);
        g_list_free (frame_list);
    }

    return NULL;
}

JxlLoader *
jxl_loader_new_from_mapping (FileMapping *mapping)
{
    JxlLoader *loader = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail (mapping != NULL, NULL);

    if (!file_mapping_has_magic (mapping, 0, "\xFF\x0A", 2)
        && !file_mapping_has_magic (
            mapping, 0, "\x00\x00\x00\x0C\x4A\x58\x4C\x20\x0D\x0A\x87\x0A", 12))
    {
        return NULL;
    }

    loader = jxl_loader_new ();

    JxlDecoder *decoder = JxlDecoderCreate (NULL);
    JxlParallelRunner *runner = JxlResizableParallelRunnerCreate (NULL);

    GList *frames = jxl_get_frames (decoder, runner, mapping);

    JxlDecoderDestroy (decoder);
    JxlResizableParallelRunnerDestroy (runner);

    success = frames != NULL;

    if (success)
    {
        loader->frames = frames;
        loader->frame_count = g_list_length (frames);
        loader->index = 0;
    }
    else
    {
        if (loader)
        {
            g_free (loader);
            loader = NULL;
        }
    }

    return loader;
};

void
jxl_cleanup_frame_list (GList *frame_list)
{
    GList *frame = frame_list;

    while (frame != NULL)
    {
        if (frame->data != NULL)
        {
            JxlFrame *jxl_frame = frame->data;
            g_free (jxl_frame->buffer);
            g_free (jxl_frame);
            frame->data = NULL;
        }
        frame = frame->next;
    }
}

void
jxl_loader_destroy (JxlLoader *loader)
{
    GList *list = loader->frames;
    jxl_cleanup_frame_list (list);
    g_list_free (loader->frames);
    g_free (loader);
}

gboolean
jxl_loader_get_is_animation (JxlLoader *loader)
{
    return loader->frame_count > 1;
}

gconstpointer
jxl_loader_get_frame_data (JxlLoader *loader, ChafaPixelType *pixel_type_out,
                           gint *width_out, gint *height_out, gint *rowstride_out)
{
    g_return_val_if_fail (loader != NULL, NULL);
    const JxlFrame *frame = g_list_nth_data (loader->frames, loader->index);

    int num_channels;
    if (frame->have_alpha)
    {
        num_channels = 4;
        *pixel_type_out = frame->is_premul ? CHAFA_PIXEL_RGBA8_PREMULTIPLIED :
                                             CHAFA_PIXEL_RGBA8_UNASSOCIATED;
    }
    else
    {
        num_channels = 3;
        *pixel_type_out = CHAFA_PIXEL_RGB8;
    }

    *width_out = frame->width;
    *height_out = frame->height;
    *rowstride_out = frame->width * num_channels;

    return frame->buffer;
}

gint
jxl_loader_get_frame_delay (JxlLoader *loader)
{
    g_return_val_if_fail (loader != NULL, 0);
    return ((JxlFrame *) g_list_nth_data (loader->frames, loader->index))->frame_duration;
}

void
jxl_loader_goto_first_frame (JxlLoader *loader)
{
    g_return_if_fail (loader != NULL);
    loader->index = 0;
}

gboolean
jxl_loader_goto_next_frame (JxlLoader *loader)
{
    g_return_val_if_fail (loader != NULL, FALSE);
    loader->index = loader->index + 1;
    return loader->index < loader->frame_count;
}
