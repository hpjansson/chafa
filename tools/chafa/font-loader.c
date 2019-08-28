/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019 Hans Petter Jansson
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

#include <ft2build.h>
#include FT_FREETYPE_H

#include <chafa.h>

#include "font-loader.h"

#define DEBUG(x)

#define REQ_WIDTH_DEFAULT 15
#define REQ_HEIGHT_DEFAULT 8

struct FontLoader
{
    /* General / I/O */
    FileMapping *mapping;
    const guint8 *file_data;
    size_t file_data_len;
    FT_Library ft_lib;
    FT_Face ft_face;

    /* Cell size that provides a good fit for font, pre-scaled */
    gint font_width;
    gint font_height;

    /* Baseline offset, vertical from top */
    gint baseline_ofs;

    /* Iterator */
    FT_ULong glyph_charcode;
    gint n_glyphs_read;
};

#define SMALL_HISTOGRAM_N_BINS 256

typedef struct
{
    gint count [SMALL_HISTOGRAM_N_BINS];
    gint first_bin;
    gint n_values;
}
SmallHistogram;

static void
small_histogram_init (SmallHistogram *hist)
{
    memset (hist, 0, sizeof (*hist));
    hist->first_bin = - (SMALL_HISTOGRAM_N_BINS / 2);
}

static void
small_histogram_add (SmallHistogram *hist, gint value)
{
    gint bin_index;

    bin_index = value - hist->first_bin;
    if (bin_index < 0 || bin_index >= SMALL_HISTOGRAM_N_BINS)
        return;

    hist->count [bin_index]++;
    hist->n_values++;
}

static gint
small_histogram_get_quantile (SmallHistogram *hist, gint dividend, gint divisor)
{
    gint i;
    gint n = 0;

    g_return_val_if_fail (dividend <= divisor, 0);

    for (i = 0; i < SMALL_HISTOGRAM_N_BINS; i++)
    {
        n += hist->count [i];
        if (n >= (hist->n_values * dividend) / divisor)
            break;
    }

    return hist->first_bin + i;
}

static void
small_histogram_get_range (SmallHistogram *hist, gint *min_out, gint *max_out)
{
    gint min, max;

    min = small_histogram_get_quantile (hist, 1, 8);
    max = small_histogram_get_quantile (hist, 7, 8);

    if (min_out)
        *min_out = min;
    if (max_out)
        *max_out = max;
}

static FontLoader *
font_loader_new (void)
{
    return g_new0 (FontLoader, 1);
}

static guint
get_bitmap_bit (const FT_Bitmap *bm, const guint8 *a, gint x, gint y, gint rowstride)
{
    if (x < 0 || x >= (gint) bm->width || y < 0 || y >= (gint) bm->rows)
        return 0;

    /* MSB first */
    return (a [y * rowstride + (x / 8)] >> (7 - (x % 8))) & 1;
}

static void
measure_glyphs (FontLoader *loader, gint *width_out, gint *height_out, gint *baseline_out)
{
    FT_ULong glyph_charcode;
    FT_UInt glyph_index;
    FT_GlyphSlot slot;
    SmallHistogram x_adv_hist;
    SmallHistogram asc_hist;
    SmallHistogram desc_hist;
    gint asc_max, desc_max;

    small_histogram_init (&x_adv_hist);
    small_histogram_init (&asc_hist);
    small_histogram_init (&desc_hist);

    glyph_charcode = FT_Get_First_Char (loader->ft_face, &glyph_index);
    while (glyph_index != 0)
    {
        /* FIXME: No need to render? */
        if (FT_Load_Glyph (loader->ft_face, glyph_index, FT_LOAD_RENDER | FT_LOAD_MONOCHROME | FT_LOAD_TARGET_MONO))
            continue;

        slot = loader->ft_face->glyph;

        small_histogram_add (&x_adv_hist, slot->advance.x / 64 > 0
                             ? slot->advance.x / 64 : slot->bitmap_left + slot->bitmap.width);
        small_histogram_add (&asc_hist, slot->bitmap_top);
        small_histogram_add (&desc_hist, (gint) slot->bitmap.rows - (gint) slot->bitmap_top);

        glyph_charcode = FT_Get_Next_Char (loader->ft_face, glyph_charcode, &glyph_index);
    }

    small_histogram_get_range (&x_adv_hist, NULL, width_out);
    small_histogram_get_range (&asc_hist, NULL, &asc_max);
    small_histogram_get_range (&desc_hist, NULL, &desc_max);

    if (height_out)
        *height_out = asc_max + desc_max;
    if (baseline_out)
        *baseline_out = asc_max;
}

static gboolean
find_best_pixel_size_scalable (FontLoader *loader)
{
    gboolean success = FALSE;
    gint req_width = REQ_WIDTH_DEFAULT;
    gint req_height = REQ_HEIGHT_DEFAULT;
    gint width = 0, height = 0, baseline;
    gint width_chg = 0, height_chg = 0;

    while ((width != CHAFA_SYMBOL_WIDTH_PIXELS && width_chg != 3)
           || (height != CHAFA_SYMBOL_HEIGHT_PIXELS && height_chg != 3))
    {
        if (FT_Set_Pixel_Sizes (loader->ft_face, req_width, req_height))
            goto out;

        measure_glyphs (loader, &width, &height, &baseline);

        if (width < CHAFA_SYMBOL_WIDTH_PIXELS)
        { req_width++; width_chg |= 1; }
        if (width > CHAFA_SYMBOL_WIDTH_PIXELS)
        { req_width--; width_chg |= 2; }

        if (height < CHAFA_SYMBOL_HEIGHT_PIXELS)
        { req_height++; height_chg |= 1; }
        if (height > CHAFA_SYMBOL_HEIGHT_PIXELS)
        { req_height--; height_chg |= 2; }
    }

    while (height < CHAFA_SYMBOL_HEIGHT_PIXELS)
    {
        req_height++;
        if (FT_Set_Pixel_Sizes (loader->ft_face, req_width, req_height))
            goto out;

        measure_glyphs (loader, &width, &height, &baseline);
    }

    loader->font_width = width;
    loader->font_height = height;
    loader->baseline_ofs = baseline;
    success = TRUE;

out:
    return success;
}

static gboolean
find_best_pixel_size_fixed (FontLoader *loader)
{
    gboolean success = FALSE;
    const FT_Bitmap_Size *avsz = loader->ft_face->available_sizes;
    gint best_width = 0, best_height = 0, best_baseline = 0;
    gint i;

    if (!loader->ft_face->available_sizes)
        goto out;

    for (i = 0; i < loader->ft_face->num_fixed_sizes; i++)
    {
        gint width, height, baseline;

        if (FT_Set_Pixel_Sizes (loader->ft_face, avsz [i].width, avsz [i].height))
            continue;

        measure_glyphs (loader, &width, &height, &baseline);

        /* Prefer strikes bigger than and as close as possible to actual target size */
        if (((best_width < CHAFA_SYMBOL_WIDTH_PIXELS || best_height < CHAFA_SYMBOL_HEIGHT_PIXELS)
             && (width >= best_width && height >= best_height))
            || ((best_width > CHAFA_SYMBOL_WIDTH_PIXELS || best_height > CHAFA_SYMBOL_HEIGHT_PIXELS)
                && (width >= CHAFA_SYMBOL_WIDTH_PIXELS && height >= CHAFA_SYMBOL_HEIGHT_PIXELS)
                && (width < best_width || height < best_height)))
        {
            best_width = width;
            best_height = height;
            best_baseline = baseline;
        }
    }

    if (best_width == 0 || best_height == 0)
        goto out;

    if (FT_Set_Pixel_Sizes (loader->ft_face, best_width, best_height))
        goto out;

    loader->font_width = best_width;
    loader->font_height = best_height;
    loader->baseline_ofs = best_baseline;

    success = TRUE;

out:
    return success;
}

FontLoader *
font_loader_new_from_mapping (FileMapping *mapping)
{
    FontLoader *loader = NULL;
    gboolean success = FALSE;

    g_return_val_if_fail (mapping != NULL, NULL);

    loader = font_loader_new ();
    loader->mapping = mapping;

    loader->file_data = file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        goto out;

    if (FT_Init_FreeType (&loader->ft_lib) != 0)
        goto out;

    if (FT_New_Memory_Face (loader->ft_lib,
                            loader->file_data,
                            loader->file_data_len,
                            0, /* face index */
                            &loader->ft_face))
        goto out;

    if (!find_best_pixel_size_scalable (loader)
        && !find_best_pixel_size_fixed (loader))
        goto out;

    success = TRUE;

out:
    if (!success)
    {
        if (loader)
        {
            font_loader_destroy (loader);
            loader = NULL;
        }
    }

    return loader;
}

void
font_loader_destroy (FontLoader *loader)
{
    if (loader->mapping)
        file_mapping_destroy (loader->mapping);

    if (loader->ft_face)
        FT_Done_Face (loader->ft_face);

    if (loader->ft_lib)
        FT_Done_FreeType (loader->ft_lib);

    g_free (loader);
}

static void
generate_glyph (const FontLoader *loader, const FT_GlyphSlot slot,
                gpointer *glyph_out, gint *width_out, gint *height_out)
{
    const FT_Bitmap *bm = &slot->bitmap;
    guint8 *glyph_data;
    gint i, j;
    const guint8 val [2] = { 0x00, 0xff };

    glyph_data = g_malloc (loader->font_width * loader->font_height * 4);

    for (j = 0; j < loader->font_height; j++)
    {
        for (i = 0; i < loader->font_width; i++)
        {
            guint b;

            b = get_bitmap_bit (bm, bm->buffer,
                                i - (gint) slot->bitmap_left,
                                j - (loader->font_height - (gint) slot->bitmap_top) + (loader->font_height - loader->baseline_ofs),
                                bm->pitch);

            if (b)
            {
                DEBUG (g_printerr ("XX"));
            }
            else
            {
                DEBUG (g_printerr (".."));
            }

            glyph_data [(j * loader->font_width + i) * 4] = val [b];
            glyph_data [(j * loader->font_width + i) * 4 + 1] = val [b];
            glyph_data [(j * loader->font_width + i) * 4 + 2] = val [b];
            glyph_data [(j * loader->font_width + i) * 4 + 3] = val [b];
        }

        DEBUG (g_printerr ("\n"));
    }

    DEBUG (g_printerr ("\n"));

    *glyph_out = glyph_data;
    *width_out = loader->font_width;
    *height_out = loader->font_height;
}

gboolean
font_loader_get_next_glyph (FontLoader *loader, gunichar *char_out,
                            gpointer *glyph_out, gint *width_out, gint *height_out)
{
    FT_GlyphSlot slot;
    gboolean success = FALSE;
    FT_UInt glyph_index = 0;

    slot = loader->ft_face->glyph;

    if (loader->n_glyphs_read == 0)
    {
        loader->glyph_charcode = FT_Get_First_Char (loader->ft_face, &glyph_index);
    }
    else
    {
        loader->glyph_charcode = FT_Get_Next_Char (loader->ft_face, loader->glyph_charcode, &glyph_index);
    }

    if (!glyph_index)
        goto out;

    loader->n_glyphs_read++;

    if (FT_Load_Glyph (loader->ft_face, glyph_index, FT_LOAD_RENDER | FT_LOAD_MONOCHROME | FT_LOAD_TARGET_MONO))
        goto out;

    generate_glyph (loader, slot, glyph_out, width_out, height_out);

    *char_out = loader->glyph_charcode;
    success = TRUE;

out:
    return success;
}
