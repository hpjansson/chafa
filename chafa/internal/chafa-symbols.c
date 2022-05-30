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

#include <string.h>  /* memset */
#include "chafa.h"
#include "internal/chafa-private.h"

/* Standard C doesn't require that "s"[0] be considered a compile-time constant.
 * Modern compilers support it as an extension, but gcc < 8.1 does not. That's a
 * bit too recent, enough to make our tests fail. Therefore we disable it for now.
 *
 * See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69960
 *
 * Another option is to use binary literals, but that is itself an extension,
 * and the symbol outlines would be less legible that way. */
#undef CHAFA_USE_CONSTANT_STRING_EXPR

#if CHAFA_USE_CONSTANT_STRING_EXPR

/* Fancy macros that turn our ASCII symbol outlines into compact bitmaps */
#define CHAFA_FOLD_BYTE_TO_BIT(x) ((((x) >> 0) | ((x) >> 1) | ((x) >> 2) | ((x) >> 3) | \
                                    ((x) >> 4) | ((x) >> 5) | ((x) >> 6) | ((x) >> 7)) & 1)
#define CHAFA_OUTLINE_CHAR_TO_BIT(c) ((guint64) CHAFA_FOLD_BYTE_TO_BIT ((c) ^ 0x20))
#define CHAFA_OUTLINE_8_CHARS_TO_BITS(s, i) \
    ((CHAFA_OUTLINE_CHAR_TO_BIT (s [i + 0]) << 7) | (CHAFA_OUTLINE_CHAR_TO_BIT (s [i + 1]) << 6) | \
     (CHAFA_OUTLINE_CHAR_TO_BIT (s [i + 2]) << 5) | (CHAFA_OUTLINE_CHAR_TO_BIT (s [i + 3]) << 4) | \
     (CHAFA_OUTLINE_CHAR_TO_BIT (s [i + 4]) << 3) | (CHAFA_OUTLINE_CHAR_TO_BIT (s [i + 5]) << 2) | \
     (CHAFA_OUTLINE_CHAR_TO_BIT (s [i + 6]) << 1) | (CHAFA_OUTLINE_CHAR_TO_BIT (s [i + 7]) << 0))
#define CHAFA_OUTLINE_TO_BITMAP_8X8(s) { \
    ((CHAFA_OUTLINE_8_CHARS_TO_BITS (s,  0) << 56) | (CHAFA_OUTLINE_8_CHARS_TO_BITS (s,  8) << 48) | \
     (CHAFA_OUTLINE_8_CHARS_TO_BITS (s, 16) << 40) | (CHAFA_OUTLINE_8_CHARS_TO_BITS (s, 24) << 32) | \
     (CHAFA_OUTLINE_8_CHARS_TO_BITS (s, 32) << 24) | (CHAFA_OUTLINE_8_CHARS_TO_BITS (s, 40) << 16) | \
     (CHAFA_OUTLINE_8_CHARS_TO_BITS (s, 48) <<  8) | (CHAFA_OUTLINE_8_CHARS_TO_BITS (s, 56) <<  0)), 0 }
#define CHAFA_SYMBOL_OUTLINE_8X8(x) CHAFA_OUTLINE_TO_BITMAP_8X8(x)

#else

#define CHAFA_SYMBOL_OUTLINE_8X8(x) x
#define CHAFA_SYMBOL_OUTLINE_16X8(x) x

#endif

typedef struct
{
    gunichar first, last;
}
UnicharRange;

typedef struct
{
    ChafaSymbolTags sc;
    gunichar c;

#if CHAFA_USE_CONSTANT_STRING_EXPR
    /* Each 64-bit integer represents an 8x8 bitmap, scanning left-to-right
     * and top-to-bottom, stored in host byte order.
     *
     * Narrow symbols use bitmap [0], with bitmap [1] set to zero. Wide
     * symbols are implemented as two narrow symbols side-by-side, with
     * the leftmost in [0] and rightmost in [1]. */
    guint64 bitmap [2];
#else
    gchar *outline;
#endif
}
ChafaSymbolDef;

ChafaSymbol *chafa_symbols;
ChafaSymbol2 *chafa_symbols2;
static gboolean symbols_initialized;

/* Ranges we treat as ambiguous-width in addition to the ones defined by
 * GLib. For instance: VTE, although spacing correctly, has many glyphs
 * extending well outside their cells resulting in ugly overlapping. */
static const UnicharRange ambiguous_ranges [] =
{
    {  0x00ad,  0x00ad },  /* Soft hyphen */
    {  0x2196,  0x21ff },  /* Arrows (most) */

    {  0x222c,  0x2237 },  /* Mathematical ops (some) */
    {  0x2245,  0x2269 },  /* Mathematical ops (some) */
    {  0x226d,  0x2279 },  /* Mathematical ops (some) */
    {  0x2295,  0x22af },  /* Mathematical ops (some) */
    {  0x22bf,  0x22bf },  /* Mathematical ops (some) */
    {  0x22c8,  0x22ff },  /* Mathematical ops (some) */

    {  0x2300,  0x23ff },  /* Technical */
    {  0x2460,  0x24ff },  /* Enclosed alphanumerics */
    {  0x25a0,  0x25ff },  /* Geometric */
    {  0x2700,  0x27bf },  /* Dingbats */
    {  0x27c0,  0x27e5 },  /* Miscellaneous mathematical symbols A (most) */
    {  0x27f0,  0x27ff },  /* Supplemental arrows A */
    {  0x2900,  0x297f },  /* Supplemental arrows B */
    {  0x2980,  0x29ff },  /* Miscellaneous mathematical symbols B */
    {  0x2b00,  0x2bff },  /* Miscellaneous symbols and arrows */
    { 0x1f100, 0x1f1ff },  /* Enclosed alphanumeric supplement */

    { 0, 0 }
};

/* Emojis of various kinds; usually multicolored. We have no control over
 * the foreground colors of these, and they may render poorly for other
 * reasons (e.g. too wide). */
static const UnicharRange emoji_ranges [] =
{
    {  0x2600,  0x26ff },  /* Miscellaneous symbols */
    { 0x1f000, 0x1fb3b },  /* Emojis first part */
    { 0x1fbcb, 0x1ffff },  /* Emojis second part, the gap is legacy computing */

    /* This symbol usually prints fine, but we don't want it randomly
     * popping up in our output anyway. So we add it to the "ugly" category,
     * which is excluded from "all". */
    {  0x534d,  0x534d },

    { 0, 0 }
};

static const UnicharRange meta_ranges [] =
{
    /* Arabic tatweel -- RTL but it's a modifier and not formally part
     * of a script, so can't simply be excluded on that basis in
     * ChafaSymbolMap::char_is_selected() */
    {  0x0640, 0x0640 },

    /* Ideographic description characters. These convert poorly to our
     * internal format. */
    {  0x2ff0, 0x2fff },

    { 0, 0 }
};

static const ChafaSymbolDef symbol_defs [] =
{
#include "chafa-symbols-ascii.h"
#include "chafa-symbols-latin.h"
#include "chafa-symbols-block.h"
#include "chafa-symbols-kana.h"
#include "chafa-symbols-misc-narrow.h"
    {
#if CHAFA_USE_CONSTANT_STRING_EXPR
        0, 0, { 0, 0 }
#else
        0, 0, NULL
#endif
    }
};

/* ranges must be terminated by zero first, last */
static gboolean
unichar_is_in_ranges (gunichar c, const UnicharRange *ranges)
{
    for ( ; ranges->first != 0 || ranges->last != 0; ranges++)
    {
        g_assert (ranges->first <= ranges->last);

        if (c >= ranges->first && c <= ranges->last)
            return TRUE;
    }

    return FALSE;
}

static void
calc_weights (ChafaSymbol *sym)
{
    gint i;

    sym->fg_weight = 0;
    sym->bg_weight = 0;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        guchar p = sym->coverage [i];

        sym->fg_weight += p;
        sym->bg_weight += 1 - p;
    }
}

static void
outline_to_coverage (const gchar *outline, gchar *coverage_out, gint rowstride)
{
    gchar xlate [256];
    gint x, y;

    xlate [' '] = 0;
    xlate ['X'] = 1;

    for (y = 0; y < CHAFA_SYMBOL_HEIGHT_PIXELS; y++)
    {
        for (x = 0; x < CHAFA_SYMBOL_WIDTH_PIXELS; x++)
        {
            guchar p = (guchar) outline [y * rowstride + x];
            coverage_out [y * CHAFA_SYMBOL_WIDTH_PIXELS + x] = xlate [p];
        }
    }
}

static guint64
coverage_to_bitmap (const gchar *cov, gint rowstride)
{
    guint64 bitmap = 0;
    gint x, y;

    for (y = 0; y < CHAFA_SYMBOL_HEIGHT_PIXELS; y++)
    {
        for (x = 0; x < CHAFA_SYMBOL_WIDTH_PIXELS; x++)
        {
            bitmap <<= 1;
            if (cov [y * rowstride + x])
                bitmap |= 1;
        }
    }

    return bitmap;
}

static void
bitmap_to_coverage (guint64 bitmap, gchar *cov_out)
{
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        cov_out [i] = (bitmap >> (63 - i)) & 1;
    }
}

static void
gen_braille_sym (gchar *cov, guint8 val)
{
    memset (cov, 0, CHAFA_SYMBOL_N_PIXELS);

    cov [1] = cov [2] = (val & 1);
    cov [5] = cov [6] = ((val >> 3) & 1);
    cov += CHAFA_SYMBOL_WIDTH_PIXELS * 2;

    cov [1] = cov [2] = ((val >> 1) & 1);
    cov [5] = cov [6] = ((val >> 4) & 1);
    cov += CHAFA_SYMBOL_WIDTH_PIXELS * 2;

    cov [1] = cov [2] = ((val >> 2) & 1);
    cov [5] = cov [6] = ((val >> 5) & 1);
    cov += CHAFA_SYMBOL_WIDTH_PIXELS * 2;

    cov [1] = cov [2] = ((val >> 6) & 1);
    cov [5] = cov [6] = ((val >> 7) & 1);
}

static int
generate_braille_syms (ChafaSymbol *syms, gint first_ofs)
{
    gunichar c;
    gint i = first_ofs;

    /* Braille 2x4 range */

    c = 0x2800;

    for (i = first_ofs; c < 0x2900; c++, i++)
    {
        ChafaSymbol *sym = &syms [i];

        sym->sc = CHAFA_SYMBOL_TAG_BRAILLE;
        sym->c = c;
        sym->coverage = g_malloc (CHAFA_SYMBOL_N_PIXELS);

        gen_braille_sym (sym->coverage, c - 0x2800);
        calc_weights (&syms [i]);
        syms [i].bitmap = coverage_to_bitmap (syms [i].coverage, CHAFA_SYMBOL_WIDTH_PIXELS);
        syms [i].popcount = chafa_population_count_u64 (syms [i].bitmap);
    }
    return i;
}

static void
gen_sextant_sym (gchar *cov, guint8 val)
{
    gint x, y;

    memset (cov, 0, CHAFA_SYMBOL_N_PIXELS);

    for (y = 0; y < 3; y++)
    {
        for (x = 0; x < 2; x++)
        {
            gint bit = y * 2 + x;

            if (val & (1 << bit))
            {
                gint u, v;

                for (v = 0; v < 3; v++)
                {
                    for (u = 0; u < 4; u++)
                    {
                        gint row = y * 3 + v;
                        if (row > 3)
                            row--;

                        cov [(row * 8) + x * 4 + u] = 1;
                    }
                }
            }
        }
    }
}

static int
generate_sextant_syms (ChafaSymbol *syms, gint first_ofs)
{
    gunichar c;
    gint i = first_ofs;

    /* Teletext sextant/2x3 mosaic range */

    c = 0x1fb00;

    for (i = first_ofs; c < 0x1fb3b; c++, i++)
    {
        ChafaSymbol *sym = &syms [i];
        gint bitmap;

        sym->sc = CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_SEXTANT;
        sym->c = c;
        sym->coverage = g_malloc (CHAFA_SYMBOL_N_PIXELS);

        bitmap = c - 0x1fb00 + 1;
        if (bitmap > 20) bitmap++;
        if (bitmap > 41) bitmap++;

        gen_sextant_sym (sym->coverage, bitmap);
        calc_weights (&syms [i]);
        syms [i].bitmap = coverage_to_bitmap (syms [i].coverage, CHAFA_SYMBOL_WIDTH_PIXELS);
        syms [i].popcount = chafa_population_count_u64 (syms [i].bitmap);
    }

    return i;
}

static ChafaSymbolTags
get_default_tags_for_char (gunichar c)
{
    ChafaSymbolTags tags = CHAFA_SYMBOL_TAG_NONE;

    if (g_unichar_iswide (c))
        tags |= CHAFA_SYMBOL_TAG_WIDE;
    else if (g_unichar_iswide_cjk (c))
        tags |= CHAFA_SYMBOL_TAG_AMBIGUOUS;

    if (g_unichar_ismark (c)
        || g_unichar_iszerowidth (c)
        || unichar_is_in_ranges (c, ambiguous_ranges))
        tags |= CHAFA_SYMBOL_TAG_AMBIGUOUS;

    if (unichar_is_in_ranges (c, emoji_ranges)
        || unichar_is_in_ranges (c, meta_ranges))
        tags |= CHAFA_SYMBOL_TAG_UGLY;

    if (c <= 0x7f)
        tags |= CHAFA_SYMBOL_TAG_ASCII;
    else if (c >= 0x2300 && c <= 0x23ff)
        tags |= CHAFA_SYMBOL_TAG_TECHNICAL;
    else if (c >= 0x25a0 && c <= 0x25ff)
        tags |= CHAFA_SYMBOL_TAG_GEOMETRIC;
    else if (c >= 0x2800 && c <= 0x28ff)
        tags |= CHAFA_SYMBOL_TAG_BRAILLE;
    else if (c >= 0x1fb00 && c <= 0x1fb3b)
        tags |= CHAFA_SYMBOL_TAG_SEXTANT;

    if (g_unichar_isalpha (c))
        tags |= CHAFA_SYMBOL_TAG_ALPHA;
    if (g_unichar_isdigit (c))
        tags |= CHAFA_SYMBOL_TAG_DIGIT;

    if (!(tags & CHAFA_SYMBOL_TAG_WIDE))
        tags |= CHAFA_SYMBOL_TAG_NARROW;

    return tags;
}

static void
def_to_symbol (const ChafaSymbolDef *def, ChafaSymbol *sym, gint x_ofs, gint rowstride)
{
    sym->c = def->c;

    /* FIXME: g_unichar_iswide_cjk() will erroneously mark many of our
     * builtin symbols as ambiguous. Find a better way to deal with it. */
    sym->sc = def->sc | (get_default_tags_for_char (def->c) & ~CHAFA_SYMBOL_TAG_AMBIGUOUS);

    sym->coverage = g_malloc (CHAFA_SYMBOL_N_PIXELS);
    outline_to_coverage (def->outline + x_ofs, sym->coverage, rowstride);

    sym->bitmap = coverage_to_bitmap (sym->coverage, CHAFA_SYMBOL_WIDTH_PIXELS);
    sym->popcount = chafa_population_count_u64 (sym->bitmap);

    calc_weights (sym);
}

static ChafaSymbol *
init_symbol_array (const ChafaSymbolDef *defs)
{
    ChafaSymbol *syms;
    gint i, j;

    syms = g_new0 (ChafaSymbol, CHAFA_N_SYMBOLS_MAX);

    for (i = 0, j = 0; defs [i].c; i++)
    {
        gint outline_len;

        outline_len = strlen (defs [i].outline);
        g_assert (outline_len == CHAFA_SYMBOL_N_PIXELS
                  || outline_len == CHAFA_SYMBOL_N_PIXELS * 2);

        if (outline_len != CHAFA_SYMBOL_N_PIXELS
            || g_unichar_iswide (defs [i].c))
            continue;

        def_to_symbol (&defs [i], &syms [j], 0, CHAFA_SYMBOL_WIDTH_PIXELS);
        j++;
    }

    j = generate_braille_syms (syms, j);
    generate_sextant_syms (syms, j);
    return syms;
}

static ChafaSymbol2 *
init_symbol_array_wide (const ChafaSymbolDef *defs)
{
    ChafaSymbol2 *syms;
    gint i, j;

    syms = g_new0 (ChafaSymbol2, CHAFA_N_SYMBOLS_MAX);

    for (i = 0, j = 0; defs [i].c; i++)
    {
        gint outline_len;

        outline_len = strlen (defs [i].outline);
        g_assert (outline_len == CHAFA_SYMBOL_N_PIXELS
                  || outline_len == CHAFA_SYMBOL_N_PIXELS * 2);

        if (outline_len != CHAFA_SYMBOL_N_PIXELS * 2
            || !g_unichar_iswide (defs [i].c))
            continue;

        def_to_symbol (&defs [i], &syms [j].sym [0],
                       0, CHAFA_SYMBOL_WIDTH_PIXELS * 2);
        def_to_symbol (&defs [i], &syms [j].sym [1],
                       CHAFA_SYMBOL_WIDTH_PIXELS, CHAFA_SYMBOL_WIDTH_PIXELS * 2);
        j++;
    }

    return syms;
}

void
chafa_init_symbols (void)
{
    if (symbols_initialized)
        return;

    chafa_symbols = init_symbol_array (symbol_defs);
    chafa_symbols2 = init_symbol_array_wide (symbol_defs);

    symbols_initialized = TRUE;
}

ChafaSymbolTags
chafa_get_tags_for_char (gunichar c)
{
    gint i;

    for (i = 0; symbol_defs [i].c; i++)
    {
        const ChafaSymbolDef *def = &symbol_defs [i];

        if (def->c == c)
            return def->sc | (get_default_tags_for_char (def->c) & ~CHAFA_SYMBOL_TAG_AMBIGUOUS);
    }

    return get_default_tags_for_char (c);
}
