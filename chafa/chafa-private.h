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

#ifndef __CHAFA_PRIVATE_H__
#define __CHAFA_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

/* Colors and color spaces */

#define CHAFA_PALETTE_INDEX_TRANSPARENT 256
#define CHAFA_PALETTE_INDEX_FG 257
#define CHAFA_PALETTE_INDEX_BG 258

/* Color space agnostic, using fixed point */
typedef struct
{
    gint16 ch [4];
}
ChafaColor;

typedef struct
{
    ChafaColor col;
}
ChafaPixel;

typedef struct
{
    ChafaColor col [CHAFA_COLOR_SPACE_MAX];
}
ChafaPaletteColor;

/* Character symbols and symbol classes */

#define CHAFA_SYMBOL_N_PIXELS (CHAFA_SYMBOL_WIDTH_PIXELS * CHAFA_SYMBOL_HEIGHT_PIXELS)

typedef struct
{
    ChafaSymbolTags sc;
    gunichar c;
    gchar *coverage;
    gint fg_weight, bg_weight;
    gboolean have_mixed;
    guint64 bitmap;
}
ChafaSymbol;

struct ChafaSymbolMap
{
    gint refs;

    guint need_rebuild : 1;
    GHashTable *desired_symbols;
    ChafaSymbol *symbols;
    gint n_symbols;
};

/* Canvas config */

struct ChafaCanvasConfig
{
    gint refs;

    gint width, height;
    ChafaCanvasMode canvas_mode;
    ChafaColorSpace color_space;
    guint32 fg_color_packed_rgb;
    guint32 bg_color_packed_rgb;
    gint alpha_threshold;  /* 0-255. 255 = no alpha in output */
    gfloat work_factor;
    ChafaSymbolMap symbol_map;
};

/* Canvas */

typedef struct ChafaCanvasCell ChafaCanvasCell;

/* Library functions */

extern ChafaSymbol *chafa_symbols;

void chafa_init_palette (void);
void chafa_init_symbols (void);

void chafa_init (void);
gboolean chafa_have_mmx (void) G_GNUC_PURE;
gboolean chafa_have_sse41 (void) G_GNUC_PURE;
gboolean chafa_have_popcnt (void) G_GNUC_PURE;

void chafa_symbol_map_init (ChafaSymbolMap *symbol_map);
void chafa_symbol_map_deinit (ChafaSymbolMap *symbol_map);
void chafa_symbol_map_copy_contents (ChafaSymbolMap *dest, const ChafaSymbolMap *src);
void chafa_symbol_map_prepare (ChafaSymbolMap *symbol_map);
gboolean chafa_symbol_map_has_symbol (const ChafaSymbolMap *symbol_map, gunichar symbol);

void chafa_canvas_config_init (ChafaCanvasConfig *canvas_config);
void chafa_canvas_config_deinit (ChafaCanvasConfig *canvas_config);
void chafa_canvas_config_copy_contents (ChafaCanvasConfig *dest, const ChafaCanvasConfig *src);

guint32 chafa_pack_color (const ChafaColor *color) G_GNUC_PURE;
void chafa_unpack_color (guint32 packed, ChafaColor *color_out);

#define chafa_color_add(d, s) \
G_STMT_START { \
  (d)->ch [0] += (s)->ch [0]; (d)->ch [1] += (s)->ch [1]; (d)->ch [2] += (s)->ch [2]; (d)->ch [3] += (s)->ch [3]; \
} G_STMT_END

#define chafa_color_diff_fast(col_a, col_b, color_space) \
(((col_b)->ch [0] - (col_a)->ch [0]) * ((col_b)->ch [0] - (col_a)->ch [0]) \
  + ((col_b)->ch [1] - (col_a)->ch [1]) * ((col_b)->ch [1] - (col_a)->ch [1]) \
  + ((col_b)->ch [2] - (col_a)->ch [2]) * ((col_b)->ch [2] - (col_a)->ch [2]))

/* Required to get alpha right */
gint chafa_color_diff_slow (const ChafaColor *col_a, const ChafaColor *col_b, ChafaColorSpace color_space) G_GNUC_PURE;

void chafa_color_div_scalar (ChafaColor *color, gint scalar);

void chafa_color_rgb_to_din99d (const ChafaColor *rgb, ChafaColor *din99);

/* Ratio is in the range 0-1000 */
void chafa_color_mix (ChafaColor *out, const ChafaColor *a, const ChafaColor *b, gint ratio);

/* Takes values 0-255 for r, g, b and returns a universal palette index 0-255 */
gint chafa_pick_color_256 (const ChafaColor *color, ChafaColorSpace color_space) G_GNUC_PURE;

/* Takes values 0-255 for r, g, b and returns a universal palette index 16-255 */
gint chafa_pick_color_240 (const ChafaColor *color, ChafaColorSpace color_space) G_GNUC_PURE;

/* Takes values 0-255 for r, g, b and returns a universal palette index 0-15 */
gint chafa_pick_color_16 (const ChafaColor *color, ChafaColorSpace color_space) G_GNUC_PURE;

/* Takes values 0-255 for r, g, b and returns CHAFA_PALETTE_INDEX_FG or CHAFA_PALETTE_INDEX_BG */
gint chafa_pick_color_fgbg (const ChafaColor *color, ChafaColorSpace color_space,
                            const ChafaColor *fg_color, const ChafaColor *bg_color) G_GNUC_PURE;

const ChafaColor *chafa_get_palette_color_256 (guint index, ChafaColorSpace color_space) G_GNUC_CONST;

#ifdef HAVE_MMX_INTRINSICS
void calc_colors_mmx (const ChafaPixel *pixels, ChafaColor *cols, const guint8 *cov);
void leave_mmx (void);
#endif

#ifdef HAVE_SSE41_INTRINSICS
gint calc_error_sse41 (const ChafaPixel *pixels, const ChafaColor *cols, const guint8 *cov) G_GNUC_PURE;
#endif

#ifdef HAVE_POPCNT_INTRINSICS
gint chafa_pop_count_u64_builtin (guint64 v) G_GNUC_PURE;
#endif

G_END_DECLS

#endif /* __CHAFA_PRIVATE_H__ */
