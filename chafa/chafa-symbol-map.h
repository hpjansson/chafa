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

#ifndef __CHAFA_SYMBOL_MAP_H__
#define __CHAFA_SYMBOL_MAP_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

G_BEGIN_DECLS

#define CHAFA_SYMBOL_WIDTH_PIXELS 8
#define CHAFA_SYMBOL_HEIGHT_PIXELS 8

typedef enum
{
    CHAFA_SYMBOL_TAG_NONE        = 0,

    CHAFA_SYMBOL_TAG_SPACE       = (1 <<  0),
    CHAFA_SYMBOL_TAG_SOLID       = (1 <<  1),
    CHAFA_SYMBOL_TAG_STIPPLE     = (1 <<  2),
    CHAFA_SYMBOL_TAG_BLOCK       = (1 <<  3),
    CHAFA_SYMBOL_TAG_BORDER      = (1 <<  4),
    CHAFA_SYMBOL_TAG_DIAGONAL    = (1 <<  5),
    CHAFA_SYMBOL_TAG_DOT         = (1 <<  6),
    CHAFA_SYMBOL_TAG_QUAD        = (1 <<  7),
    CHAFA_SYMBOL_TAG_HHALF       = (1 <<  8),
    CHAFA_SYMBOL_TAG_VHALF       = (1 <<  9),
    CHAFA_SYMBOL_TAG_HALF        = ((CHAFA_SYMBOL_TAG_HHALF) | (CHAFA_SYMBOL_TAG_VHALF)),
    CHAFA_SYMBOL_TAG_INVERTED    = (1 << 10),
    CHAFA_SYMBOL_TAG_BRAILLE     = (1 << 11),
    CHAFA_SYMBOL_TAG_TECHNICAL   = (1 << 12),
    CHAFA_SYMBOL_TAG_GEOMETRIC   = (1 << 13),
    CHAFA_SYMBOL_TAG_ASCII       = (1 << 14),
    CHAFA_SYMBOL_TAG_ALPHA       = (1 << 15),
    CHAFA_SYMBOL_TAG_DIGIT       = (1 << 16),
    CHAFA_SYMBOL_TAG_ALNUM       = CHAFA_SYMBOL_TAG_ALPHA | CHAFA_SYMBOL_TAG_DIGIT,
    CHAFA_SYMBOL_TAG_NARROW      = (1 << 17),
    CHAFA_SYMBOL_TAG_WIDE        = (1 << 18),
    CHAFA_SYMBOL_TAG_AMBIGUOUS   = (1 << 19),
    CHAFA_SYMBOL_TAG_UGLY        = (1 << 20),
    CHAFA_SYMBOL_TAG_LEGACY      = (1 << 21),
    CHAFA_SYMBOL_TAG_SEXTANT     = (1 << 22),
    CHAFA_SYMBOL_TAG_WEDGE       = (1 << 23),
    CHAFA_SYMBOL_TAG_LATIN       = (1 << 24),
    CHAFA_SYMBOL_TAG_EXTRA       = (1 << 30),
    CHAFA_SYMBOL_TAG_BAD         = CHAFA_SYMBOL_TAG_AMBIGUOUS | CHAFA_SYMBOL_TAG_UGLY,
    CHAFA_SYMBOL_TAG_ALL         = ~(CHAFA_SYMBOL_TAG_EXTRA | CHAFA_SYMBOL_TAG_BAD)
}
ChafaSymbolTags;

typedef struct ChafaSymbolMap ChafaSymbolMap;

CHAFA_AVAILABLE_IN_ALL
ChafaSymbolMap *chafa_symbol_map_new (void);
CHAFA_AVAILABLE_IN_ALL
ChafaSymbolMap *chafa_symbol_map_copy (const ChafaSymbolMap *symbol_map);
CHAFA_AVAILABLE_IN_ALL
void chafa_symbol_map_ref (ChafaSymbolMap *symbol_map);
CHAFA_AVAILABLE_IN_ALL
void chafa_symbol_map_unref (ChafaSymbolMap *symbol_map);

/* --- Selectors --- */

CHAFA_AVAILABLE_IN_ALL
void chafa_symbol_map_add_by_tags (ChafaSymbolMap *symbol_map, ChafaSymbolTags tags);
CHAFA_AVAILABLE_IN_ALL
void chafa_symbol_map_remove_by_tags (ChafaSymbolMap *symbol_map, ChafaSymbolTags tags);

CHAFA_AVAILABLE_IN_1_4
void chafa_symbol_map_add_by_range (ChafaSymbolMap *symbol_map,
                                    gunichar first, gunichar last);
CHAFA_AVAILABLE_IN_1_4
void chafa_symbol_map_remove_by_range (ChafaSymbolMap *symbol_map,
                                       gunichar first, gunichar last);

CHAFA_AVAILABLE_IN_ALL
gboolean chafa_symbol_map_apply_selectors (ChafaSymbolMap *symbol_map,
                                           const gchar *selectors, GError **error);

/* --- Glyphs --- */

CHAFA_AVAILABLE_IN_1_4
gboolean chafa_symbol_map_get_allow_builtin_glyphs (ChafaSymbolMap *symbol_map);
CHAFA_AVAILABLE_IN_1_4
void chafa_symbol_map_set_allow_builtin_glyphs (ChafaSymbolMap *symbol_map,
                                                gboolean use_builtin_glyphs);

CHAFA_AVAILABLE_IN_1_4
void chafa_symbol_map_add_glyph (ChafaSymbolMap *symbol_map,
                                 gunichar code_point,
                                 ChafaPixelType pixel_format,
                                 gpointer pixels,
                                 gint width, gint height,
                                 gint rowstride);

CHAFA_AVAILABLE_IN_1_12
gboolean chafa_symbol_map_get_glyph (ChafaSymbolMap *symbol_map,
                                     gunichar code_point,
                                     ChafaPixelType pixel_format,
                                     gpointer *pixels_out,
                                     gint *width_out, gint *height_out,
                                     gint *rowstride_out);

G_END_DECLS

#endif /* __CHAFA_SYMBOL_MAP_H__ */
