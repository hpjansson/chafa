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

#ifndef __CHAFA_PRIVATE_H__
#define __CHAFA_PRIVATE_H__

#include <glib.h>
#include "internal/chafa-bitfield.h"
#include "internal/chafa-color-hash.h"
#include "internal/chafa-dither.h"
#include "internal/chafa-indexed-image.h"
#include "internal/chafa-iterm2-canvas.h"
#include "internal/chafa-kitty-canvas.h"
#include "internal/chafa-palette.h"
#include "internal/chafa-sixel-canvas.h"

G_BEGIN_DECLS

/* Character symbols and symbol classes */

#define CHAFA_N_SYMBOLS_MAX 1024  /* For static temp arrays */
#define CHAFA_SYMBOL_N_PIXELS (CHAFA_SYMBOL_WIDTH_PIXELS * CHAFA_SYMBOL_HEIGHT_PIXELS)

typedef struct
{
    ChafaSymbolTags sc;
    gunichar c;
    gchar *coverage;
    gint fg_weight, bg_weight;
    guint64 bitmap;
    gint popcount;
}
ChafaSymbol;

/* Double-width symbol */
typedef struct
{
    ChafaSymbol sym [2];
}
ChafaSymbol2;

struct ChafaSymbolMap
{
    gint refs;

    guint need_rebuild : 1;
    guint use_builtin_glyphs : 1;

    GHashTable *glyphs;
    GHashTable *glyphs2;  /* Wide glyphs with left/right bitmaps */
    GArray *selectors;

    /* Remaining fields are populated by chafa_symbol_map_prepare () */

    /* Narrow symbols */
    ChafaSymbol *symbols;
    gint n_symbols;
    guint64 *packed_bitmaps;

    /* Wide symbols */
    ChafaSymbol2 *symbols2;
    gint n_symbols2;
    guint64 *packed_bitmaps2;
};

/* Symbol selection candidate */

typedef struct
{
    gint16 symbol_index;
    guint8 hamming_distance;
    guint8 is_inverted;
}
ChafaCandidate;

/* Canvas config */

struct ChafaCanvasConfig
{
    gint refs;

    gint width, height;
    gint cell_width, cell_height;
    ChafaCanvasMode canvas_mode;
    ChafaColorSpace color_space;
    ChafaDitherMode dither_mode;
    ChafaColorExtractor color_extractor;
    ChafaPixelMode pixel_mode;
    gint dither_grain_width, dither_grain_height;
    gfloat dither_intensity;
    guint32 fg_color_packed_rgb;
    guint32 bg_color_packed_rgb;
    gint alpha_threshold;  /* 0-255. 255 = no alpha in output */
    gfloat work_factor;
    ChafaSymbolMap symbol_map;
    ChafaSymbolMap fill_symbol_map;
    guint preprocessing_enabled : 1;
    guint fg_only_enabled : 1;
    ChafaOptimizations optimizations;
};

/* Canvas */

typedef struct ChafaCanvasCell ChafaCanvasCell;

/* Library functions */

extern ChafaSymbol *chafa_symbols;
extern ChafaSymbol2 *chafa_symbols2;

void chafa_init_palette (void);
void chafa_init_symbols (void);
ChafaSymbolTags chafa_get_tags_for_char (gunichar c);

void chafa_init (void);
gboolean chafa_have_mmx (void) G_GNUC_PURE;
gboolean chafa_have_sse41 (void) G_GNUC_PURE;
gboolean chafa_have_popcnt (void) G_GNUC_PURE;

void chafa_symbol_map_init (ChafaSymbolMap *symbol_map);
void chafa_symbol_map_deinit (ChafaSymbolMap *symbol_map);
void chafa_symbol_map_copy_contents (ChafaSymbolMap *dest, const ChafaSymbolMap *src);
void chafa_symbol_map_prepare (ChafaSymbolMap *symbol_map);
gboolean chafa_symbol_map_has_symbol (const ChafaSymbolMap *symbol_map, gunichar symbol);
void chafa_symbol_map_find_candidates (const ChafaSymbolMap *symbol_map,
                                       guint64 bitmap,
                                       gboolean do_inverse,
                                       ChafaCandidate *candidates_out,
                                       gint *n_candidates_inout);
void chafa_symbol_map_find_fill_candidates (const ChafaSymbolMap *symbol_map,
                                            gint popcount,
                                            gboolean do_inverse,
                                            ChafaCandidate *candidates_out,
                                            gint *n_candidates_inout);
void chafa_symbol_map_find_wide_candidates (const ChafaSymbolMap *symbol_map,
                                            const guint64 *bitmaps,
                                            gboolean do_inverse,
                                            ChafaCandidate *candidates_out,
                                            gint *n_candidates_inout);
void chafa_symbol_map_find_wide_fill_candidates (const ChafaSymbolMap *symbol_map,
                                                 gint popcount,
                                                 gboolean do_inverse,
                                                 ChafaCandidate *candidates_out,
                                                 gint *n_candidates_inout);

void chafa_canvas_config_init (ChafaCanvasConfig *canvas_config);
void chafa_canvas_config_deinit (ChafaCanvasConfig *canvas_config);
void chafa_canvas_config_copy_contents (ChafaCanvasConfig *dest, const ChafaCanvasConfig *src);

gint *chafa_gen_bayer_matrix (gint matrix_size, gdouble magnitude);

/* Math stuff */

#ifdef HAVE_MMX_INTRINSICS
void calc_colors_mmx (const ChafaPixel *pixels, ChafaColorAccum *accums_out, const guint8 *cov);
void chafa_leave_mmx (void);
#else
# define chafa_leave_mmx()
#endif

#ifdef HAVE_SSE41_INTRINSICS
gint calc_error_sse41 (const ChafaPixel *pixels, const ChafaColorPair *color_pair, const guint8 *cov) G_GNUC_PURE;
#endif

#if defined(HAVE_POPCNT64_INTRINSICS) || defined(HAVE_POPCNT32_INTRINSICS)
#define HAVE_POPCNT_INTRINSICS
#endif

#ifdef HAVE_POPCNT_INTRINSICS
gint chafa_pop_count_u64_builtin (guint64 v) G_GNUC_PURE;
void chafa_pop_count_vu64_builtin (const guint64 *vv, gint *vc, gint n);
void chafa_hamming_distance_vu64_builtin (guint64 a, const guint64 *vb, gint *vc, gint n);
void chafa_hamming_distance_2_vu64_builtin (const guint64 *a, const guint64 *vb, gint *vc, gint n);
#endif

/* Inline functions */

static inline guint64 chafa_slow_pop_count (guint64 v) G_GNUC_UNUSED;
static inline gint chafa_population_count_u64 (guint64 v) G_GNUC_UNUSED;
static inline void chafa_population_count_vu64 (const guint64 *vv, gint *vc, gint n) G_GNUC_UNUSED;

static inline guint64
chafa_slow_pop_count (guint64 v)
{
    /* Generic population count from
     * http://www.graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
     *
     * Peter Kankowski has more hacks, including better SIMD versions, at
     * https://www.strchr.com/crc32_popcnt */

    v = v - ((v >> 1) & (guint64) ~(guint64) 0 / 3);
    v = (v & (guint64) ~(guint64) 0 / 15 * 3) + ((v >> 2) & (guint64) ~(guint64) 0 / 15 * 3);
    v = (v + (v >> 4)) & (guint64) ~(guint64) 0 / 255 * 15;
    return (guint64) (v * ((guint64) ~(guint64) 0 / 255)) >> (sizeof (guint64) - 1) * 8;
}

static inline gint
chafa_population_count_u64 (guint64 v)
{
#ifdef HAVE_POPCNT_INTRINSICS
    if (chafa_have_popcnt ())
        return chafa_pop_count_u64_builtin (v);
#endif

    return chafa_slow_pop_count (v);
}

static inline void
chafa_population_count_vu64 (const guint64 *vv, gint *vc, gint n)
{
#ifdef HAVE_POPCNT_INTRINSICS
    if (chafa_have_popcnt ())
    {
        chafa_pop_count_vu64_builtin (vv, vc, n);
        return;
    }
#endif

    while (n--)
        *(vc++) = chafa_slow_pop_count (*(vv++));
}

static inline void
chafa_hamming_distance_vu64 (guint64 a, const guint64 *vb, gint *vc, gint n)
{
#ifdef HAVE_POPCNT_INTRINSICS
    if (chafa_have_popcnt ())
    {
        chafa_hamming_distance_vu64_builtin (a, vb, vc, n);
        return;
    }
#endif

    while (n--)
        *(vc++) = chafa_slow_pop_count (a ^ *(vb++));
}

static inline void
chafa_hamming_distance_2_vu64 (const guint64 *a, const guint64 *vb, gint *vc, gint n)
{
#ifdef HAVE_POPCNT_INTRINSICS
    if (chafa_have_popcnt ())
    {
        chafa_hamming_distance_2_vu64_builtin (a, vb, vc, n);
        return;
    }
#endif

    while (n--)
    {
        *(vc++) = chafa_slow_pop_count (a [0] ^ vb [0])
            + chafa_slow_pop_count (a [1] ^ vb [1]);
        vb += 2;
    }
}

G_END_DECLS

#endif /* __CHAFA_PRIVATE_H__ */
