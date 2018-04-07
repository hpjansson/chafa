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

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include "chafa/chafa.h"
#include "chafa/chafa-private.h"

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
 * @CHAFA_SYMBOL_TAG_DOT: Symbols that look like isolated dots.
 * @CHAFA_SYMBOL_TAG_QUAD: Quadrant block symbols.
 * @CHAFA_SYMBOL_TAG_HHALF: Horizontal half block symbols.
 * @CHAFA_SYMBOL_TAG_VHALF: Vertical half block symbols.
 * @CHAFA_SYMBOL_TAG_HALF: Joint set of horizontal and vertical halves.
 * @CHAFA_SYMBOL_TAG_INVERTED: Symbols that are the inverse of simpler symbols. When two symbols complement each other, only one will have this tag.
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

static void
rebuild_symbols (ChafaSymbolMap *symbol_map)
{
    
    GHashTableIter iter;
    gpointer key, value;
    gint i;

    g_free (symbol_map->symbols);

    symbol_map->n_symbols = g_hash_table_size (symbol_map->desired_symbols);
    symbol_map->symbols = g_new (ChafaSymbol, symbol_map->n_symbols + 1);

    g_hash_table_iter_init (&iter, symbol_map->desired_symbols);
    i = 0;

    while (g_hash_table_iter_next (&iter, &key, &value))
    {
        gint src_index = GPOINTER_TO_INT (key);
        symbol_map->symbols [i++] = chafa_symbols [src_index];
    }

    qsort (symbol_map->symbols, symbol_map->n_symbols, sizeof (ChafaSymbol),
           compare_symbols);
    memset (&symbol_map->symbols [symbol_map->n_symbols], 0, sizeof (ChafaSymbol));

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
    dest->need_rebuild = TRUE;
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
 * chafa_symbol_map_ref:
 * @symbol_map: Symbol map to add a reference to
 *
 * Adds a reference to @symbol_map.
 **/
void
chafa_symbol_map_ref (ChafaSymbolMap *symbol_map)
{
    g_return_if_fail (symbol_map != NULL);
    g_return_if_fail (symbol_map->refs > 0);

    symbol_map->refs++;
}

/**
 * chafa_symbol_map_unref:
 * @symbol_map: Symbol map to remove a reference from
 *
 * Removes a reference from @symbol_map. When remaining references frops to
 * zero, the symbol map is freed and can no longer be used.
 **/
void
chafa_symbol_map_unref (ChafaSymbolMap *symbol_map)
{
    g_return_if_fail (symbol_map != NULL);
    g_return_if_fail (symbol_map->refs > 0);

    if (--symbol_map->refs == 0)
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
    gint i;

    g_return_if_fail (symbol_map != NULL);
    g_return_if_fail (symbol_map->refs > 0);

    if (!symbol_map->desired_symbols)
        symbol_map->desired_symbols = g_hash_table_new (g_direct_hash, g_direct_equal);

    for (i = 0; chafa_symbols [i].c != 0; i++)
    {
        if (chafa_symbols [i].sc & tags)
            g_hash_table_add (symbol_map->desired_symbols, GINT_TO_POINTER (i));
    }

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
    gint i;

    g_return_if_fail (symbol_map != NULL);
    g_return_if_fail (symbol_map->refs > 0);

    if (!symbol_map->desired_symbols)
        symbol_map->desired_symbols = g_hash_table_new (g_direct_hash, g_direct_equal);

    for (i = 0; chafa_symbols [i].c != 0; i++)
    {
        if (chafa_symbols [i].sc & tags)
            g_hash_table_remove (symbol_map->desired_symbols, GINT_TO_POINTER (i));
    }

    symbol_map->need_rebuild = TRUE;
}
