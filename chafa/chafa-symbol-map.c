/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2021 Hans Petter Jansson
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

#include <string.h>  /* memset, memcpy */
#include <stdlib.h>  /* qsort */
#include "chafa.h"
#include "internal/chafa-private.h"
#include "internal/smolscale/smolscale.h"

#define DEBUG(x)

/* Max number of candidates to return from chafa_symbol_map_find_candidates() */
#define N_CANDIDATES_MAX 8

typedef enum
{
    SELECTOR_TAG,
    SELECTOR_RANGE
}
SelectorType;

typedef struct
{
    guint selector_type : 1;
    guint additive : 1;

    ChafaSymbolTags tags;

    /* First and last code points are inclusive */
    gunichar first_code_point;
    gunichar last_code_point;
}
Selector;

typedef struct
{
    gunichar c;
    guint64 bitmap;
}
Glyph;

/* Double-width glyphs */
typedef struct
{
    gunichar c;
    guint64 bitmap [2];
}
Glyph2;

/**
 * CHAFA_SYMBOL_WIDTH_PIXELS:
 *
 * The width of an internal symbol pixel matrix. If you are prescaling
 * input graphics, you will get the best results when scaling to a
 * multiple of this value.
 **/

/**
 * CHAFA_SYMBOL_HEIGHT_PIXELS:
 *
 * The height of an internal symbol pixel matrix. If you are prescaling
 * input graphics, you will get the best results when scaling to a
 * multiple of this value.
 **/

/**
 * ChafaSymbolTags:
 * @CHAFA_SYMBOL_TAG_NONE: Special value meaning no symbols.
 * @CHAFA_SYMBOL_TAG_SPACE: Space.
 * @CHAFA_SYMBOL_TAG_SOLID: Solid (inverse of space).
 * @CHAFA_SYMBOL_TAG_STIPPLE: Stipple symbols.
 * @CHAFA_SYMBOL_TAG_BLOCK: Block symbols.
 * @CHAFA_SYMBOL_TAG_BORDER: Border symbols.
 * @CHAFA_SYMBOL_TAG_DIAGONAL: Diagonal border symbols.
 * @CHAFA_SYMBOL_TAG_DOT: Symbols that look like isolated dots (excluding Braille).
 * @CHAFA_SYMBOL_TAG_QUAD: Quadrant block symbols.
 * @CHAFA_SYMBOL_TAG_HHALF: Horizontal half block symbols.
 * @CHAFA_SYMBOL_TAG_VHALF: Vertical half block symbols.
 * @CHAFA_SYMBOL_TAG_HALF: Joint set of horizontal and vertical halves.
 * @CHAFA_SYMBOL_TAG_INVERTED: Symbols that are the inverse of simpler symbols. When two symbols complement each other, only one will have this tag.
 * @CHAFA_SYMBOL_TAG_BRAILLE: Braille symbols.
 * @CHAFA_SYMBOL_TAG_TECHNICAL: Miscellaneous technical symbols.
 * @CHAFA_SYMBOL_TAG_GEOMETRIC: Geometric shapes.
 * @CHAFA_SYMBOL_TAG_ASCII: Printable ASCII characters.
 * @CHAFA_SYMBOL_TAG_ALPHA: Letters.
 * @CHAFA_SYMBOL_TAG_DIGIT: Digits.
 * @CHAFA_SYMBOL_TAG_ALNUM: Joint set of letters and digits.
 * @CHAFA_SYMBOL_TAG_NARROW: Characters that are one cell wide.
 * @CHAFA_SYMBOL_TAG_WIDE: Characters that are two cells wide.
 * @CHAFA_SYMBOL_TAG_AMBIGUOUS: Characters of uncertain width. Always excluded unless specifically asked for.
 * @CHAFA_SYMBOL_TAG_UGLY: Characters that are generally undesired or unlikely to render well. Always excluded unless specifically asked for.
 * @CHAFA_SYMBOL_TAG_LEGACY: Legacy computer symbols, including sextants, wedges and more.
 * @CHAFA_SYMBOL_TAG_SEXTANT: Sextant 2x3 mosaics.
 * @CHAFA_SYMBOL_TAG_WEDGE: Wedge shapes that align with sextants.
 * @CHAFA_SYMBOL_TAG_EXTRA: Symbols not in any other category.
 * @CHAFA_SYMBOL_TAG_BAD: Joint set of ugly and ambiguous characters. Always excluded unless specifically asked for.
 * @CHAFA_SYMBOL_TAG_ALL: Special value meaning all supported symbols.
 **/

/**
 * SECTION:chafa-symbol-map
 * @title: ChafaSymbolMap
 * @short_description: Describes a selection of textual symbols
 *
 * A #ChafaSymbolMap describes a selection of the supported textual symbols
 * that can be used in building a printable output string from a #ChafaCanvas.
 *
 * To create a new #ChafaSymbolMap, use chafa_symbol_map_new (). You can then
 * add symbols to it using chafa_symbol_map_add_by_tags () before copying
 * it into a #ChafaCanvasConfig using chafa_canvas_config_set_symbol_map ().
 *
 * Note that some symbols match multiple tags, so it makes sense to e.g.
 * add symbols matching #CHAFA_SYMBOL_TAG_BORDER and then removing symbols
 * matching #CHAFA_SYMBOL_TAG_DIAGONAL.
 *
 * The number of available symbols is a significant factor in the speed of
 * #ChafaCanvas. For the fastest possible operation you could use a single
 * symbol -- #CHAFA_SYMBOL_TAG_VHALF works well by itself.
 **/

/* Private */

#if 0
static gint
compare_symbols (const void *a, const void *b)
{
    const ChafaSymbol *a_sym = a;
    const ChafaSymbol *b_sym = b;

    if (a_sym->c < b_sym->c)
        return -1;
    if (a_sym->c > b_sym->c)
        return 1;
    return 0;
}
#endif

static guint8 *
bitmap_to_bytes (guint64 bitmap)
{
    guint8 *cov = g_malloc0 (CHAFA_SYMBOL_N_PIXELS);
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        cov [i] = (bitmap >> (CHAFA_SYMBOL_N_PIXELS - 1 - i)) & 1;
    }

    return cov;
}

/* Input format must always be RGBA8. old_format is just an indicator of how
 * the channel values are to be extracted. */
static void
pixels_to_coverage (const guint8 *pixels_in, ChafaPixelType old_format, guint8 *pixels_out,
                    gint n_pixels)
{
    gint i;

    if (old_format == CHAFA_PIXEL_RGB8 || old_format == CHAFA_PIXEL_BGR8)
    {
        for (i = 0; i < n_pixels; i++)
            pixels_out [i] = (pixels_in [i * 4] + pixels_in [i * 4 + 1] + pixels_in [i * 4 + 2]) / 3;
    }
    else
    {
        for (i = 0; i < n_pixels; i++)
            pixels_out [i] = pixels_in [i * 4 + 3];
    }
}

static void
sharpen_coverage (const guint8 *cov_in, guint8 *cov_out, gint width, gint height)
{
    gint k [3] [3] =
    {
        /* Sharpen + boost contrast */
        {  0, -1,  0 },
        { -1,  6, -1 },
        {  0, -1,  0 }
    };
    gint x, y;
    gint i, j;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            gint sum = 0;

            for (i = 0; i < 3; i++)
            {
                for (j = 0; j < 3; j++)
                {
                    gint a = x + i - 1, b = y + j - 1;

                    /* At edges, just clone the border pixels outwards */
                    a = CLAMP (a, 0, width - 1);
                    b = CLAMP (b, 0, height - 1);

                    sum += (gint) cov_in [a + b * width] * k [i] [j];
                }
            }

            cov_out [x + y * width] = CLAMP (sum, 0, 255);
        }
    }
}

static guint64
coverage_to_bitmap (const guint8 *cov, gint rowstride)
{
    guint64 bitmap = 0;
    gint x, y;

    for (y = 0; y < CHAFA_SYMBOL_HEIGHT_PIXELS; y++)
    {
        for (x = 0; x < CHAFA_SYMBOL_WIDTH_PIXELS; x++)
        {
            bitmap <<= 1;
            if (cov [y * rowstride + x] > 127)
                bitmap |= 1;
        }
    }

    return bitmap;
}

G_GNUC_UNUSED static void
dump_coverage (const guint8 *cov, gint width, gint height)
{
    gint i;

    for (i = 0; i < width * height; i++)
    {
        if (cov [i] > 127)
        {
            DEBUG (g_printerr ("@@"));
        }
        else
        {
            DEBUG (g_printerr ("--"));
        }

        if (!((i + 1) % width))
        {
            DEBUG (g_printerr ("\n"));
        }
    }

    DEBUG (g_printerr ("\n"));
}

static guint64
glyph_to_bitmap (gint width, gint height,
                 gint rowstride,
                 ChafaPixelType pixel_format,
                 gpointer pixels)
{
    guint8 scaled_pixels [CHAFA_SYMBOL_N_PIXELS * 4];
    guint8 cov [CHAFA_SYMBOL_N_PIXELS];
    guint8 sharpened_cov [CHAFA_SYMBOL_N_PIXELS];
    guint64 bitmap;

    /* Scale to cell dimensions */

    smol_scale_simple ((SmolPixelType) pixel_format, pixels, width, height, rowstride,
                       SMOL_PIXEL_RGBA8_PREMULTIPLIED,
                       (gpointer) scaled_pixels,
                       CHAFA_SYMBOL_WIDTH_PIXELS, CHAFA_SYMBOL_HEIGHT_PIXELS,
                       CHAFA_SYMBOL_WIDTH_PIXELS * 4);

    /* Generate coverage map */

    pixels_to_coverage (scaled_pixels, pixel_format, cov, CHAFA_SYMBOL_N_PIXELS);
    sharpen_coverage (cov, sharpened_cov, CHAFA_SYMBOL_WIDTH_PIXELS, CHAFA_SYMBOL_HEIGHT_PIXELS);
    DEBUG (dump_coverage (cov, CHAFA_SYMBOL_WIDTH_PIXELS, CHAFA_SYMBOL_HEIGHT_PIXELS));
    DEBUG (dump_coverage (sharpened_cov, CHAFA_SYMBOL_WIDTH_PIXELS, CHAFA_SYMBOL_HEIGHT_PIXELS));
    bitmap = coverage_to_bitmap (sharpened_cov, CHAFA_SYMBOL_WIDTH_PIXELS);

    return bitmap;
}

static void
glyph_to_bitmap_wide (gint width, gint height,
                      gint rowstride,
                      ChafaPixelType pixel_format,
                      gpointer pixels,
                      guint64 *left_bitmap_out,
                      guint64 *right_bitmap_out)
{
    guint8 scaled_pixels [CHAFA_SYMBOL_N_PIXELS * 2 * 4];
    guint8 cov [CHAFA_SYMBOL_N_PIXELS * 2];
    guint8 sharpened_cov [CHAFA_SYMBOL_N_PIXELS * 2];

    /* Scale to cell dimensions */

    smol_scale_simple ((SmolPixelType) pixel_format, pixels, width, height, rowstride,
                       SMOL_PIXEL_RGBA8_PREMULTIPLIED,
                       (gpointer) scaled_pixels,
                       CHAFA_SYMBOL_WIDTH_PIXELS * 2, CHAFA_SYMBOL_HEIGHT_PIXELS,
                       CHAFA_SYMBOL_WIDTH_PIXELS * 4 * 2);

    /* Generate coverage map */

    pixels_to_coverage (scaled_pixels, pixel_format, cov, CHAFA_SYMBOL_N_PIXELS * 2);
    sharpen_coverage (cov, sharpened_cov, CHAFA_SYMBOL_WIDTH_PIXELS * 2, CHAFA_SYMBOL_HEIGHT_PIXELS);
    DEBUG (dump_coverage (cov, CHAFA_SYMBOL_WIDTH_PIXELS * 2, CHAFA_SYMBOL_HEIGHT_PIXELS));
    DEBUG (dump_coverage (sharpened_cov, CHAFA_SYMBOL_WIDTH_PIXELS * 2, CHAFA_SYMBOL_HEIGHT_PIXELS));

    *left_bitmap_out = coverage_to_bitmap (sharpened_cov,
                                           CHAFA_SYMBOL_WIDTH_PIXELS * 2);
    *right_bitmap_out = coverage_to_bitmap (sharpened_cov + CHAFA_SYMBOL_WIDTH_PIXELS,
                                            CHAFA_SYMBOL_WIDTH_PIXELS * 2);
}

static gint
compare_symbols_popcount (const void *a, const void *b)
{
    const ChafaSymbol *a_sym = a;
    const ChafaSymbol *b_sym = b;

    if (a_sym->popcount < b_sym->popcount)
        return -1;
    if (a_sym->popcount > b_sym->popcount)
        return 1;
    return 0;
}

static gint
compare_symbols2_popcount (const void *a, const void *b)
{
    const ChafaSymbol2 *a_sym = a;
    const ChafaSymbol2 *b_sym = b;

    if (a_sym->sym [0].popcount + a_sym->sym [1].popcount
        < b_sym->sym [0].popcount + b_sym->sym [1].popcount)
        return -1;
    if (a_sym->sym [0].popcount + a_sym->sym [1].popcount
        > b_sym->sym [0].popcount + b_sym->sym [1].popcount)
        return 1;
    return 0;
}

static void
compile_symbols (ChafaSymbolMap *symbol_map, GHashTable *desired_symbols)
{
    GHashTableIter iter;
    gpointer key, value;
    gint i;

    for (i = 0; i < symbol_map->n_symbols; i++)
        g_free (symbol_map->symbols [i].coverage);

    g_free (symbol_map->symbols);
    g_free (symbol_map->packed_bitmaps);

    symbol_map->n_symbols = g_hash_table_size (desired_symbols);
    symbol_map->symbols = g_new (ChafaSymbol, symbol_map->n_symbols + 1);

    g_hash_table_iter_init (&iter, desired_symbols);
    i = 0;

    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        ChafaSymbol *sym = value;
        symbol_map->symbols [i] = *sym;
        symbol_map->symbols [i].coverage = g_memdup (symbol_map->symbols [i].coverage,
                                                     CHAFA_SYMBOL_N_PIXELS);
        i++;
    }

    qsort (symbol_map->symbols, symbol_map->n_symbols, sizeof (ChafaSymbol),
           compare_symbols_popcount);

    /* Clear sentinel */
    memset (&symbol_map->symbols [symbol_map->n_symbols], 0, sizeof (ChafaSymbol));

    symbol_map->packed_bitmaps = g_new (guint64, symbol_map->n_symbols);
    for (i = 0; i < symbol_map->n_symbols; i++)
        symbol_map->packed_bitmaps [i] = symbol_map->symbols [i].bitmap;
}

static void
compile_symbols_wide (ChafaSymbolMap *symbol_map, GHashTable *desired_symbols)
{
    GHashTableIter iter;
    gpointer key, value;
    gint i;

    for (i = 0; i < symbol_map->n_symbols2; i++)
    {
        g_free (symbol_map->symbols2 [i].sym [0].coverage);
        g_free (symbol_map->symbols2 [i].sym [1].coverage);
    }

    g_free (symbol_map->symbols2);

    symbol_map->n_symbols2 = g_hash_table_size (desired_symbols);
    symbol_map->symbols2 = g_new (ChafaSymbol2, symbol_map->n_symbols2 + 1);

    g_hash_table_iter_init (&iter, desired_symbols);
    i = 0;

    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        ChafaSymbol2 *sym = value;
        symbol_map->symbols2 [i] = *sym;
        symbol_map->symbols2 [i].sym [0].coverage = g_memdup (symbol_map->symbols2 [i].sym [0].coverage,
                                                              CHAFA_SYMBOL_N_PIXELS);
        symbol_map->symbols2 [i].sym [1].coverage = g_memdup (symbol_map->symbols2 [i].sym [1].coverage,
                                                              CHAFA_SYMBOL_N_PIXELS);
        i++;
    }

    qsort (symbol_map->symbols2, symbol_map->n_symbols2, sizeof (ChafaSymbol2),
           compare_symbols2_popcount);

    /* Clear sentinel */
    memset (&symbol_map->symbols2 [symbol_map->n_symbols2], 0, sizeof (ChafaSymbol2));

    symbol_map->packed_bitmaps2 = g_new (guint64, symbol_map->n_symbols2 * 2);
    for (i = 0; i < symbol_map->n_symbols2; i++)
    {
        symbol_map->packed_bitmaps2 [i * 2] = symbol_map->symbols2 [i].sym [0].bitmap;
        symbol_map->packed_bitmaps2 [i * 2 + 1] = symbol_map->symbols2 [i].sym [1].bitmap;
    }
}

static gboolean
char_is_selected (GArray *selectors, ChafaSymbolTags tags, gunichar c)
{
    ChafaSymbolTags auto_exclude_tags = CHAFA_SYMBOL_TAG_BAD;
    GUnicodeScript script;
    gboolean is_selected = FALSE;
    gint i;

    /* Always exclude characters that would mangle the output */
    if (!g_unichar_isprint (c) || g_unichar_iszerowidth (c)
        || c == '\t')
        return FALSE;

    /* We don't support RTL, so RTL characters will break the output.
     *
     * Ideally we'd exclude the R and AL bidi classes, but unfortunately we don't
     * have a convenient API available to us to determine the bidi class of a
     * character. So we just exclude a few scripts and hope for the best.
     *
     * A better implementation could extract directionality from the Unicode DB:
     *
     * https://www.unicode.org/reports/tr9/#Bidirectional_Character_Types
     * https://www.unicode.org/Public/UCD/latest/ucd/extracted/DerivedBidiClass.txt */
    script = g_unichar_get_script (c);
    if (script == G_UNICODE_SCRIPT_ARABIC
        || script == G_UNICODE_SCRIPT_HEBREW
        || script == G_UNICODE_SCRIPT_THAANA
        || script == G_UNICODE_SCRIPT_SYRIAC)
        return FALSE;

    for (i = 0; i < (gint) selectors->len; i++)
    {
        const Selector *selector = &g_array_index (selectors, Selector, i);

        switch (selector->selector_type)
        {
            case SELECTOR_TAG:
                if (tags & selector->tags)
                {
                    is_selected = selector->additive ? TRUE : FALSE;

                    /* We exclude "bad" symbols unless the user explicitly refers
                     * to them by tag. I.e. the selector string "0..fffff" will not
                     * include matches for "ugly", but "-ugly+0..fffff" will. */
                    auto_exclude_tags &= ~((guint) selector->tags);
                }
                break;

            case SELECTOR_RANGE:
                if (c >= selector->first_code_point && c <= selector->last_code_point)
                    is_selected = selector->additive ? TRUE : FALSE;
                break;
        }
    }

    if (tags & auto_exclude_tags)
        is_selected = FALSE;

    return is_selected;
}

static void
free_symbol (gpointer sym_p)
{
    ChafaSymbol *sym = sym_p;

    g_free (sym->coverage);
    g_free (sym);
}

static void
free_symbol_wide (gpointer sym_p)
{
    ChafaSymbol2 *sym = sym_p;

    g_free (sym->sym [0].coverage);
    g_free (sym->sym [1].coverage);
    g_free (sym);
}

static void
rebuild_symbols (ChafaSymbolMap *symbol_map)
{
    GHashTable *desired_syms;
    GHashTable *desired_syms_wide;
    GHashTableIter iter;
    gpointer key, value;
    gint i;

    desired_syms = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free_symbol);
    desired_syms_wide = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, free_symbol_wide);

    /* Pick built-in symbols */

    if (symbol_map->use_builtin_glyphs)
    {
        for (i = 0; chafa_symbols [i].c != 0; i++)
        {
            if (char_is_selected (symbol_map->selectors,
                                  chafa_symbols [i].sc,
                                  chafa_symbols [i].c))
            {
                ChafaSymbol *sym = g_new (ChafaSymbol, 1);

                *sym = chafa_symbols [i];
                sym->coverage = g_memdup (sym->coverage, CHAFA_SYMBOL_N_PIXELS);
                g_hash_table_replace (desired_syms,
                                      GUINT_TO_POINTER (chafa_symbols [i].c),
                                      sym);
            }
        }
    }

    /* Pick built-in symbols (wide) */

    if (symbol_map->use_builtin_glyphs)
    {
        for (i = 0; chafa_symbols2 [i].sym [0].c != 0; i++)
        {
            if (char_is_selected (symbol_map->selectors,
                                  chafa_symbols2 [i].sym [0].sc,
                                  chafa_symbols2 [i].sym [0].c))
            {
                ChafaSymbol2 *sym = g_new (ChafaSymbol2, 1);

                *sym = chafa_symbols2 [i];
                sym->sym [0].coverage = g_memdup (sym->sym [0].coverage, CHAFA_SYMBOL_N_PIXELS);
                sym->sym [1].coverage = g_memdup (sym->sym [1].coverage, CHAFA_SYMBOL_N_PIXELS);
                g_hash_table_replace (desired_syms_wide,
                                      GUINT_TO_POINTER (chafa_symbols2 [i].sym [0].c),
                                      sym);
            }
        }
    }

    /* Pick user glyph symbols */

    g_hash_table_iter_init (&iter, symbol_map->glyphs);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        Glyph *glyph = value;
        ChafaSymbolTags tags = chafa_get_tags_for_char (glyph->c);

        if (char_is_selected (symbol_map->selectors, tags, glyph->c))
        {
            ChafaSymbol *sym = g_new0 (ChafaSymbol, 1);

            sym->sc = tags;
            sym->c = glyph->c;
            sym->bitmap = glyph->bitmap;
            sym->coverage = (gchar *) bitmap_to_bytes (glyph->bitmap);
            sym->popcount = chafa_population_count_u64 (glyph->bitmap);
            sym->fg_weight = sym->popcount;
            sym->bg_weight = CHAFA_SYMBOL_N_PIXELS - sym->popcount;

            g_hash_table_replace (desired_syms, GUINT_TO_POINTER (glyph->c), sym);
        }
    }

    compile_symbols (symbol_map, desired_syms);
    g_hash_table_destroy (desired_syms);

    /* Pick user glyph symbols (wide) */

    g_hash_table_iter_init (&iter, symbol_map->glyphs2);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        Glyph2 *glyph = value;
        ChafaSymbolTags tags = chafa_get_tags_for_char (glyph->c);

        if (char_is_selected (symbol_map->selectors, tags, glyph->c))
        {
            ChafaSymbol2 *sym = g_new0 (ChafaSymbol2, 1);

            sym->sym [0].sc = tags;
            sym->sym [0].c = glyph->c;
            sym->sym [0].bitmap = glyph->bitmap [0];
            sym->sym [0].coverage = (gchar *) bitmap_to_bytes (glyph->bitmap [0]);
            sym->sym [0].popcount = chafa_population_count_u64 (glyph->bitmap [0]);
            sym->sym [0].fg_weight = sym->sym [0].popcount;
            sym->sym [0].bg_weight = CHAFA_SYMBOL_N_PIXELS - sym->sym [0].popcount;

            sym->sym [1].sc = tags;
            sym->sym [1].c = glyph->c;
            sym->sym [1].bitmap = glyph->bitmap [1];
            sym->sym [1].coverage = (gchar *) bitmap_to_bytes (glyph->bitmap [1]);
            sym->sym [1].popcount = chafa_population_count_u64 (glyph->bitmap [1]);
            sym->sym [1].fg_weight = sym->sym [1].popcount;
            sym->sym [1].bg_weight = CHAFA_SYMBOL_N_PIXELS - sym->sym [1].popcount;

            g_hash_table_replace (desired_syms_wide, GUINT_TO_POINTER (glyph->c), sym);
        }
    }

    compile_symbols_wide (symbol_map, desired_syms_wide);
    g_hash_table_destroy (desired_syms_wide);

    symbol_map->need_rebuild = FALSE;
}

static GHashTable *
copy_glyph_table (GHashTable *src)
{
    GHashTable *dest;
    GHashTableIter iter;
    gpointer key, value;

    dest = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

    g_hash_table_iter_init (&iter, src);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        g_hash_table_insert (dest, key, g_memdup (value, sizeof (Glyph)));
    }

    return dest;
}

static GHashTable *
copy_glyph2_table (GHashTable *src)
{
    GHashTable *dest;
    GHashTableIter iter;
    gpointer key, value;

    dest = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

    g_hash_table_iter_init (&iter, src);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        g_hash_table_insert (dest, key, g_memdup (value, sizeof (Glyph2)));
    }

    return dest;
}

static GArray *
copy_selector_array (GArray *src)
{
    GArray *dest;
    gint i;

    dest = g_array_new (FALSE, FALSE, sizeof (Selector));

    for (i = 0; i < (gint) src->len; i++)
    {
        const Selector *s = &g_array_index (src, Selector, i);
        g_array_append_val (dest, *s);
    }

    return dest;
}

static void
add_by_tags (ChafaSymbolMap *symbol_map, ChafaSymbolTags tags)
{
    Selector s = { 0 };

    s.selector_type = SELECTOR_TAG;
    s.additive = TRUE;
    s.tags = tags;

    g_array_append_val (symbol_map->selectors, s);
}

static void
remove_by_tags (ChafaSymbolMap *symbol_map, ChafaSymbolTags tags)
{
    Selector s = { 0 };

    s.selector_type = SELECTOR_TAG;
    s.additive = FALSE;
    s.tags = tags;

    g_array_append_val (symbol_map->selectors, s);
}

static void
add_by_range (ChafaSymbolMap *symbol_map, gunichar first, gunichar last)
{
    Selector s = { 0 };

    s.selector_type = SELECTOR_RANGE;
    s.additive = TRUE;
    s.first_code_point = first;
    s.last_code_point = last;

    g_array_append_val (symbol_map->selectors, s);
}

static void
remove_by_range (ChafaSymbolMap *symbol_map, gunichar first, gunichar last)
{
    Selector s = { 0 };

    s.selector_type = SELECTOR_RANGE;
    s.additive = FALSE;
    s.first_code_point = first;
    s.last_code_point = last;

    g_array_append_val (symbol_map->selectors, s);
}

static gboolean
parse_code_point (const gchar *str, gint len, gint *parsed_len_out, gunichar *c_out)
{
    gint i = 0;
    gunichar code = 0;
    gboolean result = FALSE;

    if (len >= 1 && (str [0] == 'u' || str [0] == 'U'))
        i++;

    if (len >= 2 && str [0] == '0' && str [1] == 'x')
        i += 2;

    for ( ; i < len; i++)
    {
        gint c = (gint) str [i];

        if (c - '0' >= 0 && c - '0' <= 9)
        {
            code *= 16;
            code += c - '0';
        }
        else if (c - 'a' >= 0 && c - 'a' <= 5)
        {
            code *= 16;
            code += c - 'a' + 10;
        }
        else if (c - 'A' >= 0 && c - 'A' <= 5)
        {
            code *= 16;
            code += c - 'A' + 10;
        }
        else
            break;

        result = TRUE;
    }

    *parsed_len_out = i;
    *c_out = code;
    return result;
}

typedef struct
{
    const gchar *name;
    ChafaSymbolTags sc;
}
SymMapping;

static gboolean
parse_symbol_tag (const gchar *name, gint len, SelectorType *sel_type_out,
                  ChafaSymbolTags *sc_out, gunichar *first_out, gunichar *last_out,
                  GError **error)
{
    const SymMapping map [] =
    {
        { "all", CHAFA_SYMBOL_TAG_ALL },
        { "none", CHAFA_SYMBOL_TAG_NONE },
        { "space", CHAFA_SYMBOL_TAG_SPACE },
        { "solid", CHAFA_SYMBOL_TAG_SOLID },
        { "stipple", CHAFA_SYMBOL_TAG_STIPPLE },
        { "block", CHAFA_SYMBOL_TAG_BLOCK },
        { "border", CHAFA_SYMBOL_TAG_BORDER },
        { "diagonal", CHAFA_SYMBOL_TAG_DIAGONAL },
        { "dot", CHAFA_SYMBOL_TAG_DOT },
        { "quad", CHAFA_SYMBOL_TAG_QUAD },
        { "half", CHAFA_SYMBOL_TAG_HALF },
        { "hhalf", CHAFA_SYMBOL_TAG_HHALF },
        { "vhalf", CHAFA_SYMBOL_TAG_VHALF },
        { "inverted", CHAFA_SYMBOL_TAG_INVERTED },
        { "braille", CHAFA_SYMBOL_TAG_BRAILLE },
        { "sextant", CHAFA_SYMBOL_TAG_SEXTANT },
        { "wedge", CHAFA_SYMBOL_TAG_WEDGE },
        { "technical", CHAFA_SYMBOL_TAG_TECHNICAL },
        { "geometric", CHAFA_SYMBOL_TAG_GEOMETRIC },
        { "ascii", CHAFA_SYMBOL_TAG_ASCII },
        { "alpha", CHAFA_SYMBOL_TAG_ALPHA },
        { "digit", CHAFA_SYMBOL_TAG_DIGIT },
        { "narrow", CHAFA_SYMBOL_TAG_NARROW },
        { "wide", CHAFA_SYMBOL_TAG_WIDE },
        { "ambiguous", CHAFA_SYMBOL_TAG_AMBIGUOUS },
        { "ugly", CHAFA_SYMBOL_TAG_UGLY },
        { "extra", CHAFA_SYMBOL_TAG_EXTRA },
        { "alnum", CHAFA_SYMBOL_TAG_ALNUM },
        { "bad", CHAFA_SYMBOL_TAG_BAD },
        { "legacy", CHAFA_SYMBOL_TAG_LEGACY },

        { NULL, 0 }
    };
    gint parsed_len;
    gint i;

    /* Tag? */

    for (i = 0; map [i].name; i++)
    {
        if (!g_ascii_strncasecmp (map [i].name, name, len))
        {
            *sc_out = map [i].sc;
            *sel_type_out = SELECTOR_TAG;
            return TRUE;
        }
    }

    /* Range? */

    if (!parse_code_point (name, len, &parsed_len, first_out))
        goto err;

    if (len - parsed_len > 0)
    {
        gint parsed_last_len;

        if (len - parsed_len < 3
            || name [parsed_len] != '.' || name [parsed_len + 1] != '.'
            || !parse_code_point (name + parsed_len + 2, len - parsed_len - 2,
                                  &parsed_last_len, last_out)
            || parsed_len + 2 + parsed_last_len != len)
            goto err;
    }
    else
    {
        *last_out = *first_out;
    }

    *sel_type_out = SELECTOR_RANGE;
    return TRUE;

err:
    /* Bad input */
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                 "Unrecognized symbol tag '%.*s'.",
                 len, name);
    return FALSE;
}

static gboolean
parse_selectors (ChafaSymbolMap *symbol_map, const gchar *selectors, GError **error)
{
    const gchar *p0 = selectors;
    gboolean is_add = FALSE, is_remove = FALSE;
    gboolean result = FALSE;

    while (*p0)
    {
        SelectorType sel_type;
        ChafaSymbolTags sc;
        gunichar first, last;
        gint n;

        p0 += strspn (p0, " ,");
        if (!*p0)
            break;

        if (*p0 == '-')
        {
            is_add = FALSE;
            is_remove = TRUE;
            p0++;
        }
        else if (*p0 == '+')
        {
            is_add = TRUE;
            is_remove = FALSE;
            p0++;
        }

        p0 += strspn (p0, " ");
        if (!*p0)
            break;

        n = strspn (p0, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.");
        if (!n)
        {
            g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                         "Syntax error in symbol tag selectors.");
            goto out;
        }

        if (!parse_symbol_tag (p0, n, &sel_type, &sc, &first, &last, error))
            goto out;

        p0 += n;

        if (!is_add && !is_remove)
        {
            g_array_set_size (symbol_map->selectors, 0);
            is_add = TRUE;
        }

        if (sel_type == SELECTOR_TAG)
        {
            if (is_add)
                add_by_tags (symbol_map, sc);
            else if (is_remove)
                remove_by_tags (symbol_map, sc);
        }
        else
        {
            if (is_add)
                add_by_range (symbol_map, first, last);
            else if (is_remove)
                remove_by_range (symbol_map, first, last);
        }
    }

    symbol_map->need_rebuild = TRUE;

    result = TRUE;

out:
    return result;
}

void
chafa_symbol_map_init (ChafaSymbolMap *symbol_map)
{
    g_return_if_fail (symbol_map != NULL);

    /* We need the global symbol table */
    chafa_init ();

    memset (symbol_map, 0, sizeof (*symbol_map));
    symbol_map->refs = 1;
    symbol_map->use_builtin_glyphs = TRUE;
    symbol_map->glyphs = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
    symbol_map->glyphs2 = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
    symbol_map->selectors = g_array_new (FALSE, FALSE, sizeof (Selector));
}

void
chafa_symbol_map_deinit (ChafaSymbolMap *symbol_map)
{
    gint i;

    g_return_if_fail (symbol_map != NULL);

    for (i = 0; i < symbol_map->n_symbols; i++)
        g_free (symbol_map->symbols [i].coverage);

    g_hash_table_destroy (symbol_map->glyphs);
    g_hash_table_destroy (symbol_map->glyphs2);
    g_array_free (symbol_map->selectors, TRUE);
    g_free (symbol_map->symbols);
    g_free (symbol_map->symbols2);
    g_free (symbol_map->packed_bitmaps);
    g_free (symbol_map->packed_bitmaps2);
}

void
chafa_symbol_map_copy_contents (ChafaSymbolMap *dest, const ChafaSymbolMap *src)
{
    g_return_if_fail (dest != NULL);
    g_return_if_fail (src != NULL);

    memcpy (dest, src, sizeof (*dest));

    dest->glyphs = copy_glyph_table (dest->glyphs);
    dest->glyphs2 = copy_glyph2_table (dest->glyphs2);
    dest->selectors = copy_selector_array (dest->selectors);
    dest->symbols = NULL;
    dest->symbols2 = NULL;
    dest->packed_bitmaps = NULL;
    dest->packed_bitmaps2 = NULL;
    dest->need_rebuild = TRUE;
    dest->refs = 1;
}

void
chafa_symbol_map_prepare (ChafaSymbolMap *symbol_map)
{
    if (!symbol_map->need_rebuild)
        return;

    rebuild_symbols (symbol_map);
}

gboolean
chafa_symbol_map_has_symbol (const ChafaSymbolMap *symbol_map, gunichar symbol)
{
    gint i;

    g_return_val_if_fail (symbol_map != NULL, FALSE);

    /* FIXME: Use gunichars as keys in hash table instead */

    for (i = 0; i < symbol_map->n_symbols; i++)
    {
        const ChafaSymbol *sym = &symbol_map->symbols [i];

        if (sym->c == symbol)
            return TRUE;
    }

    for (i = 0; i < symbol_map->n_symbols2; i++)
    {
        const ChafaSymbol2 *sym = &symbol_map->symbols2 [i];

        if (sym->sym [0].c == symbol)
            return TRUE;
    }

    return FALSE;
}

/* Only call this when you know the candidate should be inserted */
static void
insert_candidate (ChafaCandidate *candidates, const ChafaCandidate *new_cand)
{
    gint i;

    i = N_CANDIDATES_MAX - 1;

    while (i)
    {
        i--;

        if (new_cand->hamming_distance >= candidates [i].hamming_distance)
        {
            memmove (candidates + i + 2, candidates + i + 1, (N_CANDIDATES_MAX - 2 - i) * sizeof (ChafaCandidate));
            candidates [i + 1] = *new_cand;
            return;
        }
    }

    memmove (candidates + 1, candidates, (N_CANDIDATES_MAX - 1) * sizeof (ChafaCandidate));
    candidates [0] = *new_cand;
}

void
chafa_symbol_map_find_candidates (const ChafaSymbolMap *symbol_map, guint64 bitmap,
                                  gboolean do_inverse, ChafaCandidate *candidates_out, gint *n_candidates_inout)
{
    ChafaCandidate candidates [N_CANDIDATES_MAX] =
    {
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE }
    };
    gint *ham_dist;
    gint i;

    g_return_if_fail (symbol_map != NULL);

    ham_dist = g_alloca (sizeof (gint) * (symbol_map->n_symbols + 1));

    chafa_hamming_distance_vu64 (bitmap, symbol_map->packed_bitmaps, ham_dist, symbol_map->n_symbols);

    if (do_inverse)
    {
        for (i = 0; i < symbol_map->n_symbols; i++)
        {
            ChafaCandidate cand;
            gint hd = ham_dist [i];

            if (hd < candidates [N_CANDIDATES_MAX - 1].hamming_distance)
            {
                cand.symbol_index = i;
                cand.hamming_distance = hd;
                cand.is_inverted = FALSE;
                insert_candidate (candidates, &cand);
            }

            hd = 64 - hd;

            if (hd < candidates [N_CANDIDATES_MAX - 1].hamming_distance)
            {
                cand.symbol_index = i;
                cand.hamming_distance = hd;
                cand.is_inverted = TRUE;
                insert_candidate (candidates, &cand);
            }
        }
    }
    else
    {
        for (i = 0; i < symbol_map->n_symbols; i++)
        {
            ChafaCandidate cand;
            gint hd = ham_dist [i];

            if (hd < candidates [N_CANDIDATES_MAX - 1].hamming_distance)
            {
                cand.symbol_index = i;
                cand.hamming_distance = hd;
                cand.is_inverted = FALSE;
                insert_candidate (candidates, &cand);
            }
        }
    }

    for (i = 0; i < N_CANDIDATES_MAX; i++)
    {
         if (candidates [i].hamming_distance > 64)
             break;
    }

    i = *n_candidates_inout = MIN (i, *n_candidates_inout);
    memcpy (candidates_out, candidates, i * sizeof (ChafaCandidate));
}

void
chafa_symbol_map_find_wide_candidates (const ChafaSymbolMap *symbol_map, const guint64 *bitmaps,
                                       gboolean do_inverse, ChafaCandidate *candidates_out, gint *n_candidates_inout)
{
    ChafaCandidate candidates [N_CANDIDATES_MAX] =
    {
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE }
    };
    gint *ham_dist;
    gint i;

    g_return_if_fail (symbol_map != NULL);

    ham_dist = g_alloca (sizeof (gint) * (symbol_map->n_symbols2 + 1));

    chafa_hamming_distance_2_vu64 (bitmaps, symbol_map->packed_bitmaps2, ham_dist, symbol_map->n_symbols2);

    if (do_inverse)
    {
        for (i = 0; i < symbol_map->n_symbols2; i++)
        {
            ChafaCandidate cand;
            gint hd = ham_dist [i];

            if (hd < candidates [N_CANDIDATES_MAX - 1].hamming_distance)
            {
                cand.symbol_index = i;
                cand.hamming_distance = hd;
                cand.is_inverted = FALSE;
                insert_candidate (candidates, &cand);
            }

            hd = 128 - hd;

            if (hd < candidates [N_CANDIDATES_MAX - 1].hamming_distance)
            {
                cand.symbol_index = i;
                cand.hamming_distance = hd;
                cand.is_inverted = TRUE;
                insert_candidate (candidates, &cand);
            }
        }
    }
    else
    {
        for (i = 0; i < symbol_map->n_symbols2; i++)
        {
            ChafaCandidate cand;
            gint hd = ham_dist [i];

            if (hd < candidates [N_CANDIDATES_MAX - 1].hamming_distance)
            {
                cand.symbol_index = i;
                cand.hamming_distance = hd;
                cand.is_inverted = FALSE;
                insert_candidate (candidates, &cand);
            }
        }
    }

    for (i = 0; i < N_CANDIDATES_MAX; i++)
    {
         if (candidates [i].hamming_distance > 128)
             break;
    }

    i = *n_candidates_inout = MIN (i, *n_candidates_inout);
    memcpy (candidates_out, candidates, i * sizeof (ChafaCandidate));
}

/* Assumes symbols are sorted by ascending popcount */
static gint
find_closest_popcount (const ChafaSymbolMap *symbol_map, gint popcount)
{
    gint i, j;

    g_assert (symbol_map->n_symbols > 0);

    i = 0;
    j = symbol_map->n_symbols - 1;

    while (i < j)
    {
        gint k = (i + j + 1) / 2;

        if (popcount < symbol_map->symbols [k].popcount)
            j = k - 1;
        else if (popcount >= symbol_map->symbols [k].popcount)
            i = k;
        else
            i = j = k;
    }

    /* If we didn't find the exact popcount, the i+1'th element may be
     * a closer match. */

    if (i < symbol_map->n_symbols - 1
        && (abs (popcount - symbol_map->symbols [i + 1].popcount)
            < abs (popcount - symbol_map->symbols [i].popcount)))
    {
        i++;
    }

    return i;
}

/* Always returns zero or one candidates. We may want to do more in the future */
void
chafa_symbol_map_find_fill_candidates (const ChafaSymbolMap *symbol_map, gint popcount,
                                       gboolean do_inverse, ChafaCandidate *candidates_out, gint *n_candidates_inout)
{
    ChafaCandidate candidates [N_CANDIDATES_MAX] =
    {
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE },
        { 0, 65, FALSE }
    };
    gint sym, distance;
    gint i;

    g_return_if_fail (symbol_map != NULL);

    if (!*n_candidates_inout)
        return;

    if (symbol_map->n_symbols == 0)
    {
        *n_candidates_inout = 0;
        return;
    }

    sym = find_closest_popcount (symbol_map, popcount);
    candidates [0].symbol_index = sym;
    candidates [0].hamming_distance = abs (popcount - symbol_map->symbols [sym].popcount);
    candidates [0].is_inverted = FALSE;

    if (do_inverse && candidates [0].hamming_distance != 0)
    {
        sym = find_closest_popcount (symbol_map, 64 - popcount);
        distance = abs (64 - popcount - symbol_map->symbols [sym].popcount);

        if (distance < candidates [0].hamming_distance)
        {
            candidates [0].symbol_index = sym;
            candidates [0].hamming_distance = distance;
            candidates [0].is_inverted = TRUE;
        }
    }

    for (i = 0; i < N_CANDIDATES_MAX; i++)
    {
         if (candidates [i].hamming_distance > 64)
             break;
    }

    i = *n_candidates_inout = MIN (i, *n_candidates_inout);
    memcpy (candidates_out, candidates, i * sizeof (ChafaCandidate));
}

/* Assumes symbols are sorted by ascending popcount */
static gint
find_closest_popcount_wide (const ChafaSymbolMap *symbol_map, gint popcount)
{
    gint i, j;

    g_assert (symbol_map->n_symbols2 > 0);

    i = 0;
    j = symbol_map->n_symbols2 - 1;

    while (i < j)
    {
        gint k = (i + j + 1) / 2;

        if (popcount < symbol_map->symbols2 [k].sym [0].popcount
            + symbol_map->symbols2 [k].sym [1].popcount)
            j = k - 1;
        else if (popcount >= symbol_map->symbols2 [k].sym [0].popcount
            + symbol_map->symbols2 [k].sym [1].popcount)
            i = k;
        else
            i = j = k;
    }

    /* If we didn't find the exact popcount, the i+1'th element may be
     * a closer match. */

    if (i < symbol_map->n_symbols2 - 1
        && (abs (popcount - (symbol_map->symbols2 [i + 1].sym [0].popcount
                             + symbol_map->symbols2 [i + 1].sym [1].popcount))
            < abs (popcount - (symbol_map->symbols2 [i].sym [0].popcount
                               + symbol_map->symbols2 [i].sym [1].popcount))))
    {
        i++;
    }

    return i;
}

/* Always returns zero or one candidates. We may want to do more in the future */
void
chafa_symbol_map_find_wide_fill_candidates (const ChafaSymbolMap *symbol_map, gint popcount,
                                            gboolean do_inverse, ChafaCandidate *candidates_out, gint *n_candidates_inout)
{
    ChafaCandidate candidates [N_CANDIDATES_MAX] =
    {
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE },
        { 0, 129, FALSE }
    };
    gint sym, distance;
    gint i;

    g_return_if_fail (symbol_map != NULL);

    if (!*n_candidates_inout)
        return;

    if (symbol_map->n_symbols2 == 0)
    {
        *n_candidates_inout = 0;
        return;
    }

    sym = find_closest_popcount_wide (symbol_map, popcount);
    candidates [0].symbol_index = sym;
    candidates [0].hamming_distance = abs (popcount - (symbol_map->symbols2 [sym].sym [0].popcount
                                                       + symbol_map->symbols2 [sym].sym [1].popcount));
    candidates [0].is_inverted = FALSE;

    if (do_inverse && candidates [0].hamming_distance != 0)
    {
        sym = find_closest_popcount (symbol_map, 128 - popcount);
        distance = abs (128 - popcount - (symbol_map->symbols2 [sym].sym [0].popcount
                                          + symbol_map->symbols2 [sym].sym [1].popcount));

        if (distance < candidates [0].hamming_distance)
        {
            candidates [0].symbol_index = sym;
            candidates [0].hamming_distance = distance;
            candidates [0].is_inverted = TRUE;
        }
    }

    for (i = 0; i < N_CANDIDATES_MAX; i++)
    {
         if (candidates [i].hamming_distance > 128)
             break;
    }

    i = *n_candidates_inout = MIN (i, *n_candidates_inout);
    memcpy (candidates_out, candidates, i * sizeof (ChafaCandidate));
}

/* Public */

/**
 * chafa_symbol_map_new:
 *
 * Creates a new #ChafaSymbolMap representing a set of Unicode symbols. The
 * symbol map starts out empty.
 *
 * Returns: The new symbol map
 **/
ChafaSymbolMap *
chafa_symbol_map_new (void)
{
    ChafaSymbolMap *symbol_map;

    symbol_map = g_new (ChafaSymbolMap, 1);
    chafa_symbol_map_init (symbol_map);
    return symbol_map;
}

/**
 * chafa_symbol_map_copy:
 * @symbol_map: A #ChafaSymbolMap to copy.
 *
 * Creates a new #ChafaSymbolMap that's a copy of @symbol_map.
 *
 * Returns: The new #ChafaSymbolMap
 **/
ChafaSymbolMap *
chafa_symbol_map_copy (const ChafaSymbolMap *symbol_map)
{
    ChafaSymbolMap *new_symbol_map;

    new_symbol_map = g_new (ChafaSymbolMap, 1);
    chafa_symbol_map_copy_contents (new_symbol_map, symbol_map);
    return new_symbol_map;
}

/**
 * chafa_symbol_map_ref:
 * @symbol_map: Symbol map to add a reference to
 *
 * Adds a reference to @symbol_map.
 **/
void
chafa_symbol_map_ref (ChafaSymbolMap *symbol_map)
{
    gint refs;

    g_return_if_fail (symbol_map != NULL);
    refs = g_atomic_int_get (&symbol_map->refs);
    g_return_if_fail (refs > 0);

    g_atomic_int_inc (&symbol_map->refs);
}

/**
 * chafa_symbol_map_unref:
 * @symbol_map: Symbol map to remove a reference from
 *
 * Removes a reference from @symbol_map. When remaining references drops to
 * zero, the symbol map is freed and can no longer be used.
 **/
void
chafa_symbol_map_unref (ChafaSymbolMap *symbol_map)
{
    gint refs;

    g_return_if_fail (symbol_map != NULL);
    refs = g_atomic_int_get (&symbol_map->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&symbol_map->refs))
    {
        chafa_symbol_map_deinit (symbol_map);
        g_free (symbol_map);
    }
}

/**
 * chafa_symbol_map_add_by_tags:
 * @symbol_map: Symbol map to add symbols to
 * @tags: Selector tags for symbols to add
 *
 * Adds symbols matching the set of @tags to @symbol_map.
 **/
void
chafa_symbol_map_add_by_tags (ChafaSymbolMap *symbol_map, ChafaSymbolTags tags)
{
    g_return_if_fail (symbol_map != NULL);
    g_return_if_fail (symbol_map->refs > 0);

    add_by_tags (symbol_map, tags);

    symbol_map->need_rebuild = TRUE;
}

/**
 * chafa_symbol_map_remove_by_tags:
 * @symbol_map: Symbol map to remove symbols from
 * @tags: Selector tags for symbols to remove
 *
 * Removes symbols matching the set of @tags from @symbol_map.
 **/
void
chafa_symbol_map_remove_by_tags (ChafaSymbolMap *symbol_map, ChafaSymbolTags tags)
{
    g_return_if_fail (symbol_map != NULL);
    g_return_if_fail (symbol_map->refs > 0);

    remove_by_tags (symbol_map, tags);

    symbol_map->need_rebuild = TRUE;
}

/**
 * chafa_symbol_map_add_by_range:
 * @symbol_map: Symbol map to add symbols to
 * @first: First code point to add, inclusive
 * @last: Last code point to add, inclusive
 *
 * Adds symbols in the code point range starting with @first
 * and ending with @last to @symbol_map.
 *
 * Since: 1.4
 **/
void
chafa_symbol_map_add_by_range (ChafaSymbolMap *symbol_map, gunichar first, gunichar last)
{
    g_return_if_fail (symbol_map != NULL);
    g_return_if_fail (symbol_map->refs > 0);

    add_by_range (symbol_map, first, last);

    symbol_map->need_rebuild = TRUE;
}

/**
 * chafa_symbol_map_remove_by_range:
 * @symbol_map: Symbol map to remove symbols from
 * @first: First code point to remove, inclusive
 * @last: Last code point to remove, inclusive
 *
 * Removes symbols in the code point range starting with @first
 * and ending with @last from @symbol_map.
 *
 * Since: 1.4
 **/
void
chafa_symbol_map_remove_by_range (ChafaSymbolMap *symbol_map, gunichar first, gunichar last)
{
    g_return_if_fail (symbol_map != NULL);
    g_return_if_fail (symbol_map->refs > 0);

    remove_by_range (symbol_map, first, last);

    symbol_map->need_rebuild = TRUE;
}

/**
 * chafa_symbol_map_apply_selectors:
 * @symbol_map: Symbol map to apply selection to
 * @selectors: A string specifying selections
 * @error: Return location for an error, or %NULL
 *
 * Parses a string consisting of symbol tags separated by [+-,] and
 * applies the pattern to @symbol_map. If the string begins with + or -,
 * it's understood to be relative to the current set in @symbol_map,
 * otherwise the map is cleared first.
 *
 * The symbol tags are string versions of #ChafaSymbolTags, i.e.
 * [all, none, space, solid, stipple, block, border, diagonal, dot,
 * quad, half, hhalf, vhalf, braille, technical, geometric, ascii,
 * extra].
 *
 * Examples: "block,border" sets map to contain symbols matching either
 * of those tags. "+block,border-dot,stipple" adds block and border
 * symbols then removes dot and stipple symbols.
 *
 * If there is a parse error, none of the changes are applied.
 *
 * Returns: %TRUE on success, %FALSE if there was a parse error
 **/
gboolean
chafa_symbol_map_apply_selectors (ChafaSymbolMap *symbol_map, const gchar *selectors, GError **error)
{
    g_return_val_if_fail (symbol_map != NULL, FALSE);
    g_return_val_if_fail (symbol_map->refs > 0, FALSE);

    return parse_selectors (symbol_map, selectors, error);
}

/* --- Glyphs --- */

/**
 * chafa_symbol_map_get_allow_builtin_glyphs:
 * @symbol_map: A symbol map
 *
 * Queries whether a symbol map is allowed to use built-in glyphs for
 * symbol selection. This can be turned off if you want to use your
 * own glyphs exclusively (see chafa_symbol_map_add_glyph()).
 *
 * Defaults to %TRUE.
 *
 * Returns: %TRUE if built-in glyphs are allowed
 *
 * Since: 1.4
 **/
gboolean
chafa_symbol_map_get_allow_builtin_glyphs (ChafaSymbolMap *symbol_map)
{
    g_return_val_if_fail (symbol_map != NULL, FALSE);

    return symbol_map->use_builtin_glyphs;
}

/**
 * chafa_symbol_map_set_allow_builtin_glyphs:
 * @symbol_map: A symbol map
 * @use_builtin_glyphs: A boolean indicating whether to use built-in glyphs
 *
 * Controls whether a symbol map is allowed to use built-in glyphs for
 * symbol selection. This can be turned off if you want to use your
 * own glyphs exclusively (see chafa_symbol_map_add_glyph()).
 *
 * Defaults to %TRUE.
 *
 * Since: 1.4
 **/
void
chafa_symbol_map_set_allow_builtin_glyphs (ChafaSymbolMap *symbol_map,
                                           gboolean use_builtin_glyphs)
{
    g_return_if_fail (symbol_map != NULL);

    /* Avoid unnecessary rebuild */
    if (symbol_map->use_builtin_glyphs == use_builtin_glyphs)
        return;

    symbol_map->use_builtin_glyphs = use_builtin_glyphs;
    symbol_map->need_rebuild = TRUE;
}

/**
 * chafa_symbol_map_add_glyph:
 * @symbol_map: A symbol map
 * @code_point: The Unicode code point for this glyph
 * @pixel_format: Glyph pixel format of @pixels
 * @pixels: The glyph data
 * @width: Width of glyph, in pixels
 * @height: Height of glyph, in pixels
 * @rowstride: Offset from start of one row to the next, in bytes
 *
 * Assigns a rendered glyph to a Unicode code point. This tells Chafa what the
 * glyph looks like so the corresponding symbol can be used appropriately in
 * output.
 *
 * Assigned glyphs override built-in glyphs and any earlier glyph that may
 * have been assigned to the same code point.
 *
 * If the input is in a format with an alpha channel, the alpha channel will
 * be used for the shape. If not, an average of the color channels will be used.
 *
 * Since: 1.4
 **/
void
chafa_symbol_map_add_glyph (ChafaSymbolMap *symbol_map,
                            gunichar code_point,
                            ChafaPixelType pixel_format,
                            gpointer pixels,
                            gint width, gint height,
                            gint rowstride)
{
    g_return_if_fail (symbol_map != NULL);

    if (g_unichar_iswide (code_point))
    {
        Glyph2 *glyph2;

        glyph2 = g_new (Glyph2, 1);
        glyph2->c = code_point;
        glyph_to_bitmap_wide (width, height, rowstride, pixel_format, pixels,
                              &glyph2->bitmap [0], &glyph2->bitmap [1]);
        g_hash_table_insert (symbol_map->glyphs2, GUINT_TO_POINTER (code_point), glyph2);
    }
    else
    {
        Glyph *glyph;

        glyph = g_new (Glyph, 1);
        glyph->c = code_point;
        glyph->bitmap = glyph_to_bitmap (width, height, rowstride, pixel_format, pixels);
        g_hash_table_insert (symbol_map->glyphs, GUINT_TO_POINTER (code_point), glyph);
    }

    symbol_map->need_rebuild = TRUE;
}
