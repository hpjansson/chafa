/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018 Hans Petter Jansson
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
#include "chafa/chafa.h"
#include "chafa/chafa-private.h"

/* Max number of candidates to return from chafa_symbol_map_find_candidates() */
#define N_CANDIDATES_MAX 8

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

static void
rebuild_symbols (ChafaSymbolMap *symbol_map)
{
    gint i;

    g_free (symbol_map->symbols);
    g_free (symbol_map->packed_bitmaps);

    symbol_map->n_symbols = symbol_map->desired_symbols ? g_hash_table_size (symbol_map->desired_symbols) : 0;
    symbol_map->symbols = g_new (ChafaSymbol, symbol_map->n_symbols + 1);

    if (symbol_map->desired_symbols)
    {
        GHashTableIter iter;
        gpointer key, value;
        gint i;

        g_hash_table_iter_init (&iter, symbol_map->desired_symbols);
        i = 0;

        while (g_hash_table_iter_next (&iter, &key, &value))
        {
            gint src_index = GPOINTER_TO_INT (key);
            symbol_map->symbols [i++] = chafa_symbols [src_index];
        }

        qsort (symbol_map->symbols, symbol_map->n_symbols, sizeof (ChafaSymbol),
               compare_symbols_popcount);
    }

    /* Clear sentinel */
    memset (&symbol_map->symbols [symbol_map->n_symbols], 0, sizeof (ChafaSymbol));

    symbol_map->packed_bitmaps = g_new (guint64, symbol_map->n_symbols);
    for (i = 0; i < symbol_map->n_symbols; i++)
        symbol_map->packed_bitmaps [i] = symbol_map->symbols [i].bitmap;

    symbol_map->need_rebuild = FALSE;
}

static GHashTable *
copy_hash_table (GHashTable *src)
{
    GHashTable *dest;
    GHashTableIter iter;
    gpointer key, value;

    dest = g_hash_table_new (g_direct_hash, g_direct_equal);

    g_hash_table_iter_init (&iter, src);
    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        g_hash_table_insert (dest, key, value);
    }

    return dest;
}

static void
add_by_tags (GHashTable *sym_ht, ChafaSymbolTags tags)
{
    gint i;

    for (i = 0; chafa_symbols [i].c != 0; i++)
    {
        if (chafa_symbols [i].sc & tags)
            g_hash_table_add (sym_ht, GINT_TO_POINTER (i));
    }
}

static void
remove_by_tags (GHashTable *sym_ht, ChafaSymbolTags tags)
{
    gint i;

    for (i = 0; chafa_symbols [i].c != 0; i++)
    {
        if (chafa_symbols [i].sc & tags)
            g_hash_table_remove (sym_ht, GINT_TO_POINTER (i));
    }
}

typedef struct
{
    const gchar *name;
    ChafaSymbolTags sc;
}
SymMapping;

static gboolean
parse_symbol_tag (const gchar *name, gint len, ChafaSymbolTags *sc_out, GError **error)
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
        { "technical", CHAFA_SYMBOL_TAG_TECHNICAL },
        { "geometric", CHAFA_SYMBOL_TAG_GEOMETRIC },
        { "ascii", CHAFA_SYMBOL_TAG_ASCII },
        { NULL, 0 }
    };
    gint i;

    for (i = 0; map [i].name; i++)
    {
        if (!g_ascii_strncasecmp (map [i].name, name, len))
        {
            *sc_out = map [i].sc;
            return TRUE;
        }
    }

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
    GHashTable *new_syms = NULL;
    gboolean result = FALSE;

    while (*p0)
    {
        ChafaSymbolTags sc;
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

        n = strspn (p0, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
        if (!n)
        {
            g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                         "Syntax error in symbol tag selectors.");
            goto out;
        }

        if (!parse_symbol_tag (p0, n, &sc, error))
            goto out;

        p0 += n;

        if (is_add)
        {
            if (!new_syms)
                new_syms = copy_hash_table (symbol_map->desired_symbols);
            add_by_tags (new_syms, sc);
        }
        else if (is_remove)
        {
            if (!new_syms)
                new_syms = copy_hash_table (symbol_map->desired_symbols);
            remove_by_tags (new_syms, sc);
        }
        else
        {
            if (new_syms)
                g_hash_table_unref (new_syms);
            new_syms = g_hash_table_new (g_direct_hash, g_direct_equal);
            add_by_tags (new_syms, sc);
            is_add = TRUE;
        }
    }

    if (symbol_map->desired_symbols)
        g_hash_table_unref (symbol_map->desired_symbols);
    symbol_map->desired_symbols = new_syms;
    new_syms = NULL;

    symbol_map->need_rebuild = TRUE;

    result = TRUE;

out:
    if (new_syms)
        g_hash_table_unref (new_syms);
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
}

void
chafa_symbol_map_deinit (ChafaSymbolMap *symbol_map)
{
    g_return_if_fail (symbol_map != NULL);

    if (symbol_map->desired_symbols)
        g_hash_table_unref (symbol_map->desired_symbols);

    g_free (symbol_map->symbols);
    g_free (symbol_map->packed_bitmaps);
}

void
chafa_symbol_map_copy_contents (ChafaSymbolMap *dest, const ChafaSymbolMap *src)
{
    g_return_if_fail (dest != NULL);
    g_return_if_fail (src != NULL);

    memcpy (dest, src, sizeof (*dest));

    if (dest->desired_symbols)
        dest->desired_symbols = copy_hash_table (dest->desired_symbols);

    dest->symbols = NULL;
    dest->packed_bitmaps = NULL;
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

/* FIXME: Use gunichars as keys in hash table instead, or use binary search here */
gboolean
chafa_symbol_map_has_symbol (const ChafaSymbolMap *symbol_map, gunichar symbol)
{
    gint i;

    g_return_val_if_fail (symbol_map != NULL, FALSE);

    for (i = 0; i < symbol_map->n_symbols; i++)
    {
        const ChafaSymbol *sym = &symbol_map->symbols [i];

        if (sym->c == symbol)
            return TRUE;
        if (sym->c > symbol)
            break;
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
    gint ham_dist [CHAFA_N_SYMBOLS_MAX];
    gint i;

    g_return_if_fail (symbol_map != NULL);

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

    if (!symbol_map->desired_symbols)
        symbol_map->desired_symbols = g_hash_table_new (g_direct_hash, g_direct_equal);

    add_by_tags (symbol_map->desired_symbols, tags);

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

    if (!symbol_map->desired_symbols)
        symbol_map->desired_symbols = g_hash_table_new (g_direct_hash, g_direct_equal);

    remove_by_tags (symbol_map->desired_symbols, tags);

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
 * quad, half, hhalf, vhalf, braille, technical, geometric, ascii].
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
