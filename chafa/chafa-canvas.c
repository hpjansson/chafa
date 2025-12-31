/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2025 Hans Petter Jansson
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

#include <math.h>
#include <string.h>
#include <glib.h>
#include "chafa.h"
#include "internal/chafa-batch.h"
#include "internal/chafa-canvas-internal.h"
#include "internal/chafa-canvas-printer.h"
#include "internal/chafa-private.h"
#include "internal/chafa-pixops.h"
#include "internal/chafa-symbol-renderer.h"
#include "internal/chafa-work-cell.h"
#include "internal/smolscale/smolscale.h"

/**
 * SECTION:chafa-canvas
 * @title: ChafaCanvas
 * @short_description: A canvas that renders to text
 *
 * A #ChafaCanvas is a canvas that can render its contents as text strings.
 *
 * To create a new #ChafaCanvas, use chafa_canvas_new (). If you want to
 * specify any parameters, like the geometry, color space and so on, you
 * must create a #ChafaCanvasConfig first.
 *
 * You can draw an image to the canvas using chafa_canvas_draw_all_pixels ()
 * and create an ANSI text (or sixel) representation of the canvas' current
 * contents using chafa_canvas_build_ansi ().
 **/

/* Max candidates to consider in pick_symbol_and_colors_fast(). This is also
 * limited by a similar constant in chafa-symbol-map.c */
#define N_CANDIDATES_MAX 8

/* Dithering */
#define DITHER_BASE_INTENSITY_FGBG 1.0
#define DITHER_BASE_INTENSITY_8C   0.5
#define DITHER_BASE_INTENSITY_16C  0.25
#define DITHER_BASE_INTENSITY_256C 0.1

static ChafaColor
threshold_alpha (ChafaColor col, gint alpha_threshold)
{
    col.ch [3] = col.ch [3] < alpha_threshold ? 0 : 255;
    return col;
}

static gint
color_to_rgb (const ChafaCanvas *canvas, ChafaColor col)
{
    col = threshold_alpha (col, canvas->config.alpha_threshold);
    if (col.ch [3] == 0)
        return -1;

    return ((gint) col.ch [0] << 16) | ((gint) col.ch [1] << 8) | (gint) col.ch [2];
}

static ChafaColor
packed_rgb_to_color (gint rgb)
{
    ChafaColor col;

    if (rgb < 0)
    {
        col.ch [0] = 0x80;
        col.ch [1] = 0x80;
        col.ch [2] = 0x80;
        col.ch [3] = 0x00;
    }
    else
    {
        col.ch [0] = (rgb >> 16) & 0xff;
        col.ch [1] = (rgb >> 8) & 0xff;
        col.ch [2] = rgb & 0xff;
        col.ch [3] = 0xff;
    }

    return col;
}

static gint
packed_rgba_to_rgb (const ChafaCanvas *canvas, guint32 rgba)
{
    ChafaColor col;

    chafa_unpack_color (rgba, &col);
    return color_to_rgb (canvas, col);
}

static guint32
packed_rgb_to_rgba (gint rgb)
{
    ChafaColor col;

    col = packed_rgb_to_color (rgb);
    return chafa_pack_color (&col);
}

static gint16
packed_rgb_to_index (const ChafaPalette *palette, ChafaColorSpace cs, gint rgb)
{
    ChafaColorCandidates ccand;
    ChafaColor col;

    if (rgb < 0)
        return CHAFA_PALETTE_INDEX_TRANSPARENT;

    col = packed_rgb_to_color (rgb);
    chafa_palette_lookup_nearest (palette, cs, &col, &ccand);
    return ccand.index [0];
}

static const ChafaColor *
get_palette_color_with_color_space (ChafaPalette *palette, gint index, ChafaColorSpace cs)
{
    return chafa_palette_get_color (palette, cs, index);
}

static void
differentiate_channel (guint8 *dest_channel, guint8 reference_channel, gint min_diff)
{
    gint diff;

    diff = (gint) *dest_channel - (gint) reference_channel;

    if (diff >= -min_diff && diff <= 0)
        *dest_channel = MAX ((gint) reference_channel - min_diff, 0);
    else if (diff <= min_diff && diff >= 0)
        *dest_channel = MIN ((gint) reference_channel + min_diff, 255);
}

static void
update_display_colors (ChafaCanvas *canvas)
{
    ChafaColor fg_col;
    ChafaColor bg_col;

    chafa_unpack_color (canvas->config.fg_color_packed_rgb, &fg_col);
    chafa_unpack_color (canvas->config.bg_color_packed_rgb, &bg_col);

    if (canvas->config.color_space == CHAFA_COLOR_SPACE_DIN99D)
    {
        chafa_color_rgb_to_din99d (&fg_col, &canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG]);
        chafa_color_rgb_to_din99d (&bg_col, &canvas->default_colors.colors [CHAFA_COLOR_PAIR_BG]);
    }
    else
    {
        canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG] = fg_col;
        canvas->default_colors.colors [CHAFA_COLOR_PAIR_BG] = bg_col;
    }

    canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG].ch [3] = 0xff;
    canvas->default_colors.colors [CHAFA_COLOR_PAIR_BG].ch [3] = 0x00;

    /* When holding the BG, we need to compare against a consistent
     * foreground color for symbol selection by outline. 50% gray
     * yields acceptable results as a stand-in average of all possible
     * colors. The BG color can't be too similar, so push it away a
     * little if needed. This should work with both bright and dark
     * background colors, and the background color doesn't have to
     * be precise.
     *
     * We don't need to do this for monochrome modes, as they use the
     * FG/BG colors directly. */

    if (canvas->extract_colors && canvas->config.fg_only_enabled)
    {
        gint i;

        chafa_unpack_color (0xff7f7f7f,
                            &canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG]);

        for (i = 0; i < 3; i++)
        {
            differentiate_channel (&canvas->default_colors.colors [CHAFA_COLOR_PAIR_BG].ch [i],
                                   canvas->default_colors.colors [CHAFA_COLOR_PAIR_FG].ch [i],
                                   5);
        }
    }
}

static void
maybe_clear (ChafaCanvas *canvas)
{
    gint i;

    if (!canvas->needs_clear)
        return;

    for (i = 0; i < canvas->config.width * canvas->config.height; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];

        memset (cell, 0, sizeof (*cell));
        cell->c = ' ';
    }
}

static void
setup_palette (ChafaCanvas *canvas)
{
    ChafaPaletteType fg_pal_type = CHAFA_PALETTE_TYPE_DYNAMIC_256;
    ChafaPaletteType bg_pal_type = CHAFA_PALETTE_TYPE_DYNAMIC_256;
    ChafaColor fg_col;
    ChafaColor bg_col;

    chafa_unpack_color (canvas->config.fg_color_packed_rgb, &fg_col);
    chafa_unpack_color (canvas->config.bg_color_packed_rgb, &bg_col);

    fg_col.ch [3] = 0xff;
    bg_col.ch [3] = 0x00;

    /* The repetition here kind of sucks, but it'll get better once the
     * palette refactoring is done and subtypes go away. */

    switch (chafa_canvas_config_get_canvas_mode (&canvas->config))
    {
        case CHAFA_CANVAS_MODE_TRUECOLOR:
            fg_pal_type = CHAFA_PALETTE_TYPE_DYNAMIC_256;
            bg_pal_type = CHAFA_PALETTE_TYPE_DYNAMIC_256;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_256:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_256;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_256;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_240:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_240;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_240;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_16:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_16;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_16;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_16_8:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_16;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_8;
            break;

        case CHAFA_CANVAS_MODE_INDEXED_8:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_8;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_8;
            break;

        case CHAFA_CANVAS_MODE_FGBG_BGFG:
        case CHAFA_CANVAS_MODE_FGBG:
            fg_pal_type = CHAFA_PALETTE_TYPE_FIXED_FGBG;
            bg_pal_type = CHAFA_PALETTE_TYPE_FIXED_FGBG;
            break;

        case CHAFA_CANVAS_MODE_MAX:
            g_assert_not_reached ();
    }

    chafa_palette_init (&canvas->fg_palette, fg_pal_type);

    chafa_palette_set_color (&canvas->fg_palette, CHAFA_PALETTE_INDEX_FG, &fg_col);
    chafa_palette_set_color (&canvas->fg_palette, CHAFA_PALETTE_INDEX_BG, &bg_col);

    chafa_palette_set_alpha_threshold (&canvas->fg_palette, canvas->config.alpha_threshold);
    chafa_palette_set_transparent_index (&canvas->fg_palette, CHAFA_PALETTE_INDEX_TRANSPARENT);

    chafa_palette_init (&canvas->bg_palette, bg_pal_type);

    chafa_palette_set_color (&canvas->bg_palette, CHAFA_PALETTE_INDEX_FG, &fg_col);
    chafa_palette_set_color (&canvas->bg_palette, CHAFA_PALETTE_INDEX_BG, &bg_col);

    chafa_palette_set_alpha_threshold (&canvas->bg_palette, canvas->config.alpha_threshold);
    chafa_palette_set_transparent_index (&canvas->bg_palette, CHAFA_PALETTE_INDEX_TRANSPARENT);
}

static gunichar
find_best_blank_char (ChafaCanvas *canvas)
{
    ChafaCandidate candidates [N_CANDIDATES_MAX];
    gint n_candidates;
    gunichar best_char = 0x20;

    /* Try space (0x20) first */
    if (chafa_symbol_map_has_symbol (&canvas->config.symbol_map, 0x20)
        || chafa_symbol_map_has_symbol (&canvas->config.fill_symbol_map, 0x20))
        return 0x20;

    n_candidates = N_CANDIDATES_MAX;
    chafa_symbol_map_find_fill_candidates (&canvas->config.fill_symbol_map,
                                           0,
                                           FALSE,
                                           candidates,
                                           &n_candidates);
    if (n_candidates > 0)
    {
        best_char = canvas->config.fill_symbol_map.symbols [candidates [0].symbol_index].c;
    }
    else
    {
        n_candidates = N_CANDIDATES_MAX;
        chafa_symbol_map_find_candidates (&canvas->config.symbol_map,
                                          0,
                                          FALSE,
                                          candidates,
                                          &n_candidates);
        if (n_candidates > 0)
        {
            best_char = canvas->config.symbol_map.symbols [candidates [0].symbol_index].c;
        }
    }

    return best_char;
}

static gunichar
find_best_solid_char (ChafaCanvas *canvas)
{
    ChafaCandidate candidates [N_CANDIDATES_MAX];
    gint n_candidates;
    gunichar best_char = 0;

    /* Try solid block (0x2588) first */
    if (chafa_symbol_map_has_symbol (&canvas->config.symbol_map, 0x2588)
        || chafa_symbol_map_has_symbol (&canvas->config.fill_symbol_map, 0x2588))
        return 0x2588;

    n_candidates = N_CANDIDATES_MAX;
    chafa_symbol_map_find_fill_candidates (&canvas->config.fill_symbol_map,
                                           64,
                                           FALSE,
                                           candidates,
                                           &n_candidates);
    if (n_candidates > 0 && candidates [0].hamming_distance <= 32)
    {
        best_char = canvas->config.fill_symbol_map.symbols [candidates [0].symbol_index].c;
    }
    else
    {
        n_candidates = N_CANDIDATES_MAX;
        chafa_symbol_map_find_candidates (&canvas->config.symbol_map,
                                          0xffffffffffffffff,
                                          FALSE,
                                          candidates,
                                          &n_candidates);
        if (n_candidates > 0 && candidates [0].hamming_distance <= 32)
        {
            best_char = canvas->config.symbol_map.symbols [candidates [0].symbol_index].c;
        }
    }

    return best_char;
}

static void
destroy_pixel_canvas (ChafaCanvas *canvas)
{
    if (canvas->pixel_canvas)
    {
        if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
            chafa_sixel_canvas_destroy (canvas->pixel_canvas);
        else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_KITTY)
            chafa_kitty_canvas_destroy (canvas->pixel_canvas);
        else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_ITERM2)
            chafa_iterm2_canvas_destroy (canvas->pixel_canvas);

        canvas->pixel_canvas = NULL;
    }
}

static void
draw_all_pixels (ChafaCanvas *canvas, ChafaPixelType src_pixel_type,
                 const guint8 *src_pixels,
                 gint src_width, gint src_height, gint src_rowstride)
{
    ChafaColor bg_color;
    ChafaAlign halign = CHAFA_ALIGN_START, valign = CHAFA_ALIGN_START;
    ChafaTuck tuck = CHAFA_TUCK_STRETCH;

    if (src_width == 0 || src_height == 0)
        return;

    if (canvas->placement)
    {
        halign = chafa_placement_get_halign (canvas->placement);
        valign = chafa_placement_get_valign (canvas->placement);
        tuck = chafa_placement_get_tuck (canvas->placement);
    }

    if (canvas->pixels)
    {
        g_free (canvas->pixels);
        canvas->pixels = NULL;
    }

    destroy_pixel_canvas (canvas);

    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_KITTY
        || canvas->config.pixel_mode == CHAFA_PIXEL_MODE_ITERM2)
    {
        chafa_unpack_color (canvas->config.bg_color_packed_rgb, &bg_color);
        bg_color.ch [3] = canvas->config.alpha_threshold < 1 ? 0x00 : 0xff;
    }

    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        /* Symbol mode */

        canvas->pixel_canvas = chafa_symbol_renderer_new (canvas,
                                                          0,
                                                          0,
                                                          canvas->config.width,
                                                          canvas->config.height);
        chafa_symbol_renderer_draw_all_pixels (canvas->pixel_canvas,
                                               src_pixel_type,
                                               src_pixels,
                                               src_width, src_height,
                                               src_rowstride,
                                               halign, valign,
                                               tuck,
                                               canvas->config.work_factor);
    }
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
    {
        /* Sixel mode */

        canvas->fg_palette.alpha_threshold = canvas->config.alpha_threshold;
        canvas->pixel_canvas = chafa_sixel_canvas_new (canvas->width_pixels,
                                                       canvas->height_pixels,
                                                       canvas->config.color_space,
                                                       &canvas->fg_palette,
                                                       &canvas->dither);
        chafa_sixel_canvas_draw_all_pixels (canvas->pixel_canvas,
                                            src_pixel_type,
                                            src_pixels,
                                            src_width, src_height,
                                            src_rowstride,
                                            halign, valign,
                                            tuck,
                                            canvas->config.work_factor);
    }
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_KITTY)
    {
        /* Kitty mode */

        canvas->fg_palette.alpha_threshold = canvas->config.alpha_threshold;
        canvas->pixel_canvas = chafa_kitty_canvas_new (canvas->width_pixels,
                                                       canvas->height_pixels);

        if (canvas->pixel_canvas)
            chafa_kitty_canvas_draw_all_pixels (canvas->pixel_canvas,
                                                src_pixel_type,
                                                src_pixels,
                                                src_width, src_height,
                                                src_rowstride,
                                                bg_color,
                                                halign, valign,
                                                tuck);
    }
    else  /* if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_ITERM2) */
    {
        /* iTerm2 mode */

        canvas->fg_palette.alpha_threshold = canvas->config.alpha_threshold;
        canvas->pixel_canvas = chafa_iterm2_canvas_new (canvas->width_pixels,
                                                        canvas->height_pixels);

        if (canvas->pixel_canvas)
            chafa_iterm2_canvas_draw_all_pixels (canvas->pixel_canvas,
                                                 src_pixel_type,
                                                 src_pixels,
                                                 src_width, src_height,
                                                 src_rowstride,
                                                 bg_color,
                                                 halign, valign,
                                                 tuck);
    }
}

/**
 * chafa_canvas_new:
 * @config: Configuration to use or %NULL for hardcoded defaults
 *
 * Creates a new canvas with the specified configuration. The
 * canvas makes a private copy of the configuration, so it will
 * not be affected by subsequent changes.
 *
 * Returns: The new canvas
 **/
ChafaCanvas *
chafa_canvas_new (const ChafaCanvasConfig *config)
{
    ChafaCanvas *canvas;
    gfloat dither_intensity = 1.0f;

    if (config)
    {
        g_return_val_if_fail (config->width > 0, NULL);
        g_return_val_if_fail (config->height > 0, NULL);
    }

    chafa_init ();

    canvas = g_new0 (ChafaCanvas, 1);

    if (config)
        chafa_canvas_config_copy_contents (&canvas->config, config);
    else
        chafa_canvas_config_init (&canvas->config);

    canvas->refs = 1;

    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        /* ANSI art */
        canvas->width_pixels = canvas->config.width * CHAFA_SYMBOL_WIDTH_PIXELS;
        canvas->height_pixels = canvas->config.height * CHAFA_SYMBOL_HEIGHT_PIXELS;
    }
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SIXELS)
    {
        /* Sixels */
        canvas->width_pixels = canvas->config.width * canvas->config.cell_width;
        canvas->height_pixels = canvas->config.height * canvas->config.cell_height;
    }
    else  /* CHAFA_PIXEL_MODE_KITTY or CHAFA_PIXEL_MODE_ITERM2 */
    {
        canvas->width_pixels = canvas->config.width * canvas->config.cell_width;
        canvas->height_pixels = canvas->config.height * canvas->config.cell_height;
    }

    canvas->pixels = NULL;
    canvas->cells = g_new (ChafaCanvasCell, canvas->config.width * canvas->config.height);
    canvas->work_factor_int = canvas->config.work_factor * 10 + 0.5f;
    canvas->needs_clear = TRUE;
    canvas->have_alpha = FALSE;
    canvas->placement = NULL;

    canvas->consider_inverted = !(canvas->config.fg_only_enabled
                                  || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG);

    canvas->extract_colors = !(canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG
                               || canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG);

    if (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_FGBG)
        canvas->config.fg_only_enabled = TRUE;

    canvas->use_quantized_error =
        (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_INDEXED_16_8
         && !canvas->config.fg_only_enabled);

    chafa_symbol_map_prepare (&canvas->config.symbol_map);
    chafa_symbol_map_prepare (&canvas->config.fill_symbol_map);

    canvas->blank_char = find_best_blank_char (canvas);
    canvas->solid_char = find_best_solid_char (canvas);

    /* In truecolor mode we don't support any fancy color spaces for now, since
     * we'd have to convert back to RGB space when emitting control codes, and
     * the code for that has yet to be written. In palette modes we just use
     * the palette mappings.
     *
     * There is also no reason to dither in truecolor mode, _unless_ we're
     * producing sixels, which quantize to a dynamic palette. */
    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_KITTY
        || canvas->config.pixel_mode == CHAFA_PIXEL_MODE_ITERM2
        || (canvas->config.canvas_mode == CHAFA_CANVAS_MODE_TRUECOLOR
            && canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS))
    {
        canvas->config.color_space = CHAFA_COLOR_SPACE_RGB;
        canvas->config.dither_mode = CHAFA_DITHER_MODE_NONE;
    }

    if (canvas->config.dither_mode == CHAFA_DITHER_MODE_ORDERED)
    {
        switch (canvas->config.canvas_mode)
        {
            case CHAFA_CANVAS_MODE_TRUECOLOR:
            case CHAFA_CANVAS_MODE_INDEXED_256:
            case CHAFA_CANVAS_MODE_INDEXED_240:
                dither_intensity = DITHER_BASE_INTENSITY_256C;
                break;
            case CHAFA_CANVAS_MODE_INDEXED_16:
            case CHAFA_CANVAS_MODE_INDEXED_16_8:
                dither_intensity = DITHER_BASE_INTENSITY_16C;
                break;
            case CHAFA_CANVAS_MODE_INDEXED_8:
                dither_intensity = DITHER_BASE_INTENSITY_8C;
                break;
            case CHAFA_CANVAS_MODE_FGBG:
            case CHAFA_CANVAS_MODE_FGBG_BGFG:
                dither_intensity = DITHER_BASE_INTENSITY_FGBG;
                break;
            default:
                g_assert_not_reached ();
                break;
        }
    }

    chafa_dither_init (&canvas->dither, canvas->config.dither_mode,
                       dither_intensity * canvas->config.dither_intensity,
                       canvas->config.dither_grain_width,
                       canvas->config.dither_grain_height);

    update_display_colors (canvas);
    setup_palette (canvas);

    return canvas;
}

/**
 * chafa_canvas_new_similar:
 * @orig: Canvas to copy configuration from
 *
 * Creates a new canvas configured similarly to @orig.
 *
 * Returns: The new canvas
 **/
ChafaCanvas *
chafa_canvas_new_similar (ChafaCanvas *orig)
{
    ChafaCanvas *canvas;

    g_return_val_if_fail (orig != NULL, NULL);

    canvas = g_new (ChafaCanvas, 1);
    memcpy (canvas, orig, sizeof (*canvas));
    canvas->refs = 1;

    chafa_canvas_config_copy_contents (&canvas->config, &orig->config);

    canvas->pixels = NULL;
    canvas->cells = g_new (ChafaCanvasCell, canvas->config.width * canvas->config.height);
    canvas->needs_clear = TRUE;

    chafa_dither_copy (&orig->dither, &canvas->dither);

    canvas->placement = NULL;

    return canvas;
}

/**
 * chafa_canvas_ref:
 * @canvas: Canvas to add a reference to
 *
 * Adds a reference to @canvas.
 **/
void
chafa_canvas_ref (ChafaCanvas *canvas)
{
    gint refs;

    g_return_if_fail (canvas != NULL);
    refs = g_atomic_int_get (&canvas->refs);
    g_return_if_fail (refs > 0);

    g_atomic_int_inc (&canvas->refs);
}

/**
 * chafa_canvas_unref:
 * @canvas: Canvas to remove a reference from
 *
 * Removes a reference from @canvas. When remaining references drops to
 * zero, the canvas is freed and can no longer be used.
 **/
void
chafa_canvas_unref (ChafaCanvas *canvas)
{
    gint refs;

    g_return_if_fail (canvas != NULL);
    refs = g_atomic_int_get (&canvas->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&canvas->refs))
    {
        if (canvas->placement)
            chafa_placement_unref (canvas->placement);
        chafa_canvas_config_deinit (&canvas->config);
        destroy_pixel_canvas (canvas);
        chafa_dither_deinit (&canvas->dither);
        chafa_palette_deinit (&canvas->fg_palette);
        chafa_palette_deinit (&canvas->bg_palette);
        g_free (canvas->pixels);
        g_free (canvas->cells);
        g_free (canvas);
    }
}

/**
 * chafa_canvas_peek_config:
 * @canvas: Canvas whose configuration to inspect
 *
 * Returns a pointer to the configuration belonging to @canvas.
 * This can be inspected using the #ChafaCanvasConfig getter
 * functions, but not changed.
 *
 * Returns: A pointer to the canvas' immutable configuration
 **/
const ChafaCanvasConfig *
chafa_canvas_peek_config (ChafaCanvas *canvas)
{
    g_return_val_if_fail (canvas != NULL, NULL);
    g_return_val_if_fail (canvas->refs > 0, NULL);

    return &canvas->config;
}

/**
 * chafa_canvas_set_placement:
 * @canvas: Canvas to place the placement on
 * @placement: Placement to place
 *
 * Places @placement on @canvas, replacing the latter's content. The placement
 * will cover the entire canvas.
 *
 * The canvas will keep a reference to the placement until it is replaced or the
 * canvas itself is freed.
 *
 * Since: 1.14
 */
void
chafa_canvas_set_placement (ChafaCanvas *canvas, ChafaPlacement *placement)
{
    ChafaImage *image;
    ChafaFrame *frame;

    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);

    chafa_placement_ref (placement);
    if (canvas->placement)
        chafa_placement_unref (canvas->placement);
    canvas->placement = placement;

    image = placement->image;
    g_assert (image != NULL);

    frame = image->frame;
    if (!frame)
        return;

    draw_all_pixels (canvas, frame->pixel_type, frame->data,
                     frame->width, frame->height, frame->rowstride);
}

/**
 * chafa_canvas_draw_all_pixels:
 * @canvas: Canvas whose pixel data to replace
 * @src_pixel_type: Pixel format of @src_pixels
 * @src_pixels: Pointer to the start of source pixel memory
 * @src_width: Width in pixels of source pixel data
 * @src_height: Height in pixels of source pixel data
 * @src_rowstride: Number of bytes between the start of each pixel row
 *
 * Replaces pixel data of @canvas with a copy of that found at @src_pixels,
 * which must be in one of the formats supported by #ChafaPixelType.
 *
 * Since: 1.2
 **/
void
chafa_canvas_draw_all_pixels (ChafaCanvas *canvas, ChafaPixelType src_pixel_type,
                              const guint8 *src_pixels,
                              gint src_width, gint src_height, gint src_rowstride)
{
    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (src_pixel_type < CHAFA_PIXEL_MAX);
    g_return_if_fail (src_pixels != NULL);
    g_return_if_fail (src_width >= 0);
    g_return_if_fail (src_height >= 0);

    draw_all_pixels (canvas, src_pixel_type, src_pixels,
                     src_width, src_height, src_rowstride);
}

/**
 * chafa_canvas_set_contents_rgba8:
 * @canvas: Canvas whose pixel data to replace
 * @src_pixels: Pointer to the start of source pixel memory
 * @src_width: Width in pixels of source pixel data
 * @src_height: Height in pixels of source pixel data
 * @src_rowstride: Number of bytes between the start of each pixel row
 *
 * Replaces pixel data of @canvas with a copy of that found at @src_pixels.
 * The source data must be in packed 8-bits-per-channel RGBA format. The
 * alpha value is expressed as opacity (0xff is opaque) and is not
 * premultiplied.
 *
 * Deprecated: 1.2: Use chafa_canvas_draw_all_pixels() instead.
 **/
void
chafa_canvas_set_contents_rgba8 (ChafaCanvas *canvas, const guint8 *src_pixels,
                                 gint src_width, gint src_height, gint src_rowstride)
{
    draw_all_pixels (canvas, CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                     src_pixels, src_width, src_height, src_rowstride);
}

/**
 * chafa_canvas_build_ansi:
 * @canvas: The canvas to generate an ANSI character representation of
 *
 * Builds a UTF-8 string of ANSI sequences and symbols representing
 * the canvas' current contents. This can e.g. be printed to a terminal.
 * The exact choice of escape sequences and symbols, dimensions, etc. is
 * determined by the configuration assigned to @canvas on its creation.
 *
 * All output lines except for the last one will end in a newline.
 *
 * Returns: A UTF-8 string of ANSI sequences and symbols
 *
 * Deprecated: 1.6: Use chafa_canvas_print() instead.
 **/
GString *
chafa_canvas_build_ansi (ChafaCanvas *canvas)
{
    g_return_val_if_fail (canvas != NULL, NULL);
    g_return_val_if_fail (canvas->refs > 0, NULL);

    return chafa_canvas_print (canvas, NULL);
}

/**
 * chafa_canvas_print:
 * @canvas: The canvas to generate a printable representation of
 * @term_info: Terminal to format for, or %NULL for fallback
 *
 * Builds a UTF-8 string of terminal control sequences and symbols
 * representing the canvas' current contents. This can be printed
 * to a terminal. The exact choice of escape sequences and symbols,
 * dimensions, etc. is determined by the configuration assigned to
 * @canvas on its creation.
 *
 * All output lines except for the last one will end in a newline.
 *
 * Returns: A UTF-8 string of terminal control sequences and symbols
 *
 * Since: 1.6
 **/
GString *
chafa_canvas_print (ChafaCanvas *canvas, ChafaTermInfo *term_info)
{
    GString *str;

    g_return_val_if_fail (canvas != NULL, NULL);
    g_return_val_if_fail (canvas->refs > 0, NULL);

    if (term_info)
        chafa_term_info_ref (term_info);
    else
        term_info = chafa_term_db_get_fallback_info (chafa_term_db_get_default ());

    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        maybe_clear (canvas);
        str = chafa_canvas_print_symbols (canvas, term_info);
    }
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SIXELS
             && chafa_term_info_get_seq (term_info, CHAFA_TERM_SEQ_BEGIN_SIXELS)
             && canvas->pixel_canvas)
    {
        /* Sixel mode */

        str = g_string_new ("");
        chafa_sixel_canvas_build_ansi (canvas->pixel_canvas, term_info, str,
                                       canvas->config.passthrough);
    }
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_KITTY
             && chafa_term_info_get_seq (term_info, CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1)
             && canvas->pixel_canvas)
    {
        /* Kitty mode */

        str = g_string_new ("");
        chafa_kitty_canvas_build_ansi (canvas->pixel_canvas, term_info, str,
                                       canvas->config.width, canvas->config.height,
                                       canvas->placement ? canvas->placement->id : -1,
                                       canvas->config.passthrough);
    }
    else if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_ITERM2
             && canvas->pixel_canvas)
    {
        /* iTerm2 mode */

        str = g_string_new ("");
        chafa_iterm2_canvas_build_ansi (canvas->pixel_canvas, term_info, str,
                                        canvas->config.width, canvas->config.height);
    }
    else
    {
        str = g_string_new ("");
    }

    chafa_term_info_unref (term_info);
    return str;
}

/**
 * chafa_canvas_print_rows:
 * @canvas: The canvas to generate a printable representation of
 * @term_info: Terminal to format for, or %NULL for fallback
 * @array_out: Pointer to storage for resulting array pointer
 * @array_len_out: Pointer to storage for array's element count, or %NULL
 *
 * Builds an array of UTF-8 strings made up of terminal control sequences
 * and symbols representing the canvas' current contents. These can be
 * printed to a terminal. The exact choice of escape sequences and symbols,
 * dimensions, etc. is determined by the configuration assigned to
 * @canvas on its creation.
 *
 * The array will be %NULL-terminated. The element count does not include
 * the terminator.
 *
 * When the canvas' pixel mode is %CHAFA_PIXEL_MODE_SYMBOLS, each element
 * will hold the contents of exactly one symbol row. There will be no row
 * separators, newlines or control sequences to reposition the cursor between
 * rows. Row positioning is left to the caller.
 *
 * In other pixel modes, there may be one or more strings, but the splitting
 * criteria should not be relied on. They must be printed in sequence, exactly
 * as they appear.
 *
 * Since: 1.14
 **/
void
chafa_canvas_print_rows (ChafaCanvas *canvas, ChafaTermInfo *term_info,
                         GString ***array_out, gint *array_len_out)
{
    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (array_out != NULL);

    if (term_info)
        chafa_term_info_ref (term_info);
    else
        term_info = chafa_term_db_get_fallback_info (chafa_term_db_get_default ());

    if (canvas->config.pixel_mode == CHAFA_PIXEL_MODE_SYMBOLS)
    {
        maybe_clear (canvas);
        chafa_canvas_print_symbol_rows (canvas, term_info, array_out, array_len_out);
    }
    else
    {
        GString *gs = chafa_canvas_print (canvas, term_info);

        *array_out = g_new (GString *, 2);
        (*array_out) [0] = gs;
        (*array_out) [1] = NULL;
        if (array_len_out)
            *array_len_out = 1;
    }

    chafa_term_info_unref (term_info);
}

/**
 * chafa_canvas_print_rows_strv:
 * @canvas: The canvas to generate a printable representation of
 * @term_info: Terminal to format for, or %NULL for fallback
 *
 * Builds an array of UTF-8 strings made up of terminal control sequences
 * and symbols representing the canvas' current contents. These can be
 * printed to a terminal. The exact choice of escape sequences and symbols,
 * dimensions, etc. is determined by the configuration assigned to
 * @canvas on its creation.
 *
 * The array will be %NULL-terminated and can be freed with g_strfreev().
 *
 * When the canvas' pixel mode is %CHAFA_PIXEL_MODE_SYMBOLS, each element
 * will hold the contents of exactly one symbol row. There will be no row
 * separators, newlines or control sequences to reposition the cursor between
 * rows. Row positioning is left to the caller.
 *
 * In other pixel modes, there may be one or more strings, but the splitting
 * criteria should not be relied on. They must be printed in sequence, exactly
 * as they appear.
 *
 * Returns: A %NULL-terminated array of string pointers
 *
 * Since: 1.14
 **/
gchar **
chafa_canvas_print_rows_strv (ChafaCanvas *canvas, ChafaTermInfo *term_info)
{
    GString **gsa;
    gint gsa_len;
    gchar **strv;
    gint i;

    g_return_val_if_fail (canvas != NULL, NULL);
    g_return_val_if_fail (canvas->refs > 0, NULL);

    chafa_canvas_print_rows (canvas, term_info, &gsa, &gsa_len);
    strv = g_new (gchar *, gsa_len + 1);

    for (i = 0; i < gsa_len; i++)
    {
        /* FIXME: Switch to g_string_free_and_steal() in 2025 */
        strv [i] = g_string_free (gsa [i], FALSE);
    }

    strv [gsa_len] = NULL;
    g_free (gsa);
    return strv;
}

/**
 * chafa_canvas_get_char_at:
 * @canvas: The canvas to inspect
 * @x: Column of character cell to inspect
 * @y: Row of character cell to inspect
 *
 * Returns the character at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, the leftmost cell will contain the character
 * and the rightmost cell will contain 0.
 *
 * Returns: The character at (x, y)
 *
 * Since: 1.8
 **/
gunichar
chafa_canvas_get_char_at (ChafaCanvas *canvas, gint x, gint y)
{
    g_return_val_if_fail (canvas != NULL, 0);
    g_return_val_if_fail (canvas->refs > 0, 0);
    g_return_val_if_fail (x >= 0 && x < canvas->config.width, 0);
    g_return_val_if_fail (y >= 0 && y < canvas->config.height, 0);

    return canvas->cells [y * canvas->config.width + x].c;
}

/**
 * chafa_canvas_set_char_at:
 * @canvas: The canvas to manipulate
 * @x: Column of character cell to manipulate
 * @y: Row of character cell to manipulate
 * @c: The character value to store
 *
 * Sets the character at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, the leftmost cell must contain the character
 * and the cell to the right of it will automatically be set to 0.
 *
 * If the character is a nonprintable or zero-width, no change will be
 * made.
 *
 * Returns: The number of cells output (0, 1 or 2)
 *
 * Since: 1.8
 **/
gint
chafa_canvas_set_char_at (ChafaCanvas *canvas, gint x, gint y, gunichar c)
{
    ChafaCanvasCell *cell;
    gint cwidth = 1;

    g_return_val_if_fail (canvas != NULL, 0);
    g_return_val_if_fail (canvas->refs > 0, 0);
    g_return_val_if_fail (x >= 0 && x < canvas->config.width, 0);
    g_return_val_if_fail (y >= 0 && y < canvas->config.height, 0);

    if (!g_unichar_isprint (c) || g_unichar_iszerowidth (c))
        return 0;

    if (g_unichar_iswide (c))
        cwidth = 2;

    if (x + cwidth > canvas->config.width)
        return 0;

    cell = &canvas->cells [y * canvas->config.width + x];
    cell [0].c = c;

    if (cwidth == 2)
    {
        cell [1].c = 0;
        cell [1].fg_color = cell [0].fg_color;
        cell [1].bg_color = cell [0].bg_color;
    }

    /* If we're overwriting the rightmost half of a wide character,
     * clear its leftmost half */

    if (x > 0)
    {
        if (cell [-1].c != 0
            && g_unichar_iswide (cell [-1].c))
            cell [-1].c = canvas->blank_char;
    }

    return cwidth;
}

/**
 * chafa_canvas_get_colors_at:
 * @canvas: The canvas to inspect
 * @x: Column of character cell to inspect
 * @y: Row of character cell to inspect
 * @fg_out: Storage for foreground color
 * @bg_out: Storage for background color
 *
 * Gets the colors at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, both cells will contain the same colors.
 *
 * The colors will be -1 for transparency, packed 8bpc RGB otherwise,
 * i.e. 0x00RRGGBB hex.
 *
 * If the canvas is in an indexed mode, palette lookups will be made
 * for you.
 *
 * Since: 1.8
 **/
void
chafa_canvas_get_colors_at (ChafaCanvas *canvas, gint x, gint y,
                            gint *fg_out, gint *bg_out)
{
    const ChafaCanvasCell *cell;
    gint fg = -1, bg = -1;

    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (x >= 0 && x < canvas->config.width);
    g_return_if_fail (y >= 0 && y < canvas->config.height);

    cell = &canvas->cells [y * canvas->config.width + x];

    switch (canvas->config.canvas_mode)
    {
        case CHAFA_CANVAS_MODE_TRUECOLOR:
            fg = packed_rgba_to_rgb (canvas, cell->fg_color);
            bg = packed_rgba_to_rgb (canvas, cell->bg_color);
            break;
        case CHAFA_CANVAS_MODE_INDEXED_256:
        case CHAFA_CANVAS_MODE_INDEXED_240:
        case CHAFA_CANVAS_MODE_INDEXED_16:
        case CHAFA_CANVAS_MODE_INDEXED_16_8:
        case CHAFA_CANVAS_MODE_INDEXED_8:
        case CHAFA_CANVAS_MODE_FGBG_BGFG:
        case CHAFA_CANVAS_MODE_FGBG:
            if (cell->fg_color == CHAFA_PALETTE_INDEX_BG
                || cell->fg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
                fg = -1;
            else
                fg = color_to_rgb (canvas,
                                   *get_palette_color_with_color_space (&canvas->fg_palette, cell->fg_color,
                                                                        CHAFA_COLOR_SPACE_RGB));
            if (cell->bg_color == CHAFA_PALETTE_INDEX_BG
                || cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
                bg = -1;
            else
                bg = color_to_rgb (canvas,
                                   *get_palette_color_with_color_space (&canvas->bg_palette, cell->bg_color,
                                                                        CHAFA_COLOR_SPACE_RGB));
            break;
        case CHAFA_CANVAS_MODE_MAX:
            g_assert_not_reached ();
            break;
    }

    *fg_out = fg;
    *bg_out = bg;
}

/**
 * chafa_canvas_set_colors_at:
 * @canvas: The canvas to manipulate
 * @x: Column of character cell to manipulate
 * @y: Row of character cell to manipulate
 * @fg: Foreground color
 * @bg: Background color
 *
 * Sets the colors at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, both cells will be set to the same color.
 *
 * The colors must be -1 for transparency, packed 8bpc RGB otherwise,
 * i.e. 0x00RRGGBB hex.
 *
 * If the canvas is in an indexed mode, palette lookups will be made
 * for you.
 *
 * Since: 1.8
 **/
void
chafa_canvas_set_colors_at (ChafaCanvas *canvas, gint x, gint y,
                            gint fg, gint bg)
{
    ChafaCanvasCell *cell;

    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (x >= 0 && x < canvas->config.width);
    g_return_if_fail (y >= 0 && y < canvas->config.height);

    cell = &canvas->cells [y * canvas->config.width + x];

    switch (canvas->config.canvas_mode)
    {
        case CHAFA_CANVAS_MODE_TRUECOLOR:
            cell->fg_color = packed_rgb_to_rgba (fg);
            cell->bg_color = packed_rgb_to_rgba (bg);
            break;
        case CHAFA_CANVAS_MODE_INDEXED_256:
        case CHAFA_CANVAS_MODE_INDEXED_240:
        case CHAFA_CANVAS_MODE_INDEXED_16:
        case CHAFA_CANVAS_MODE_INDEXED_16_8:
        case CHAFA_CANVAS_MODE_INDEXED_8:
            cell->fg_color = packed_rgb_to_index (&canvas->fg_palette, canvas->config.color_space, fg);
            cell->bg_color = packed_rgb_to_index (&canvas->bg_palette, canvas->config.color_space, bg);
            break;
        case CHAFA_CANVAS_MODE_FGBG_BGFG:
            cell->fg_color = fg >= 0 ? CHAFA_PALETTE_INDEX_FG : CHAFA_PALETTE_INDEX_TRANSPARENT;
            cell->bg_color = bg >= 0 ? CHAFA_PALETTE_INDEX_FG : CHAFA_PALETTE_INDEX_TRANSPARENT;
            break;
        case CHAFA_CANVAS_MODE_FGBG:
            cell->fg_color = fg >= 0 ? fg : CHAFA_PALETTE_INDEX_TRANSPARENT;
            break;
        case CHAFA_CANVAS_MODE_MAX:
            g_assert_not_reached ();
            break;
    }

    /* If setting the color of half a wide char, set it for the other half too */

    if (x > 0 && cell->c == 0)
    {
        cell [-1].fg_color = cell->fg_color;
        cell [-1].bg_color = cell->bg_color;
    }

    if (x < canvas->config.width - 1 && cell [1].c == 0)
    {
        cell [1].fg_color = cell->fg_color;
        cell [1].bg_color = cell->bg_color;
    }
}

/**
 * chafa_canvas_get_raw_colors_at:
 * @canvas: The canvas to inspect
 * @x: Column of character cell to inspect
 * @y: Row of character cell to inspect
 * @fg_out: Storage for foreground color
 * @bg_out: Storage for background color
 *
 * Gets the colors at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, both cells will contain the same colors.
 *
 * The colors will be -1 for transparency, packed 8bpc RGB, i.e.
 * 0x00RRGGBB hex in truecolor mode, or the raw pen value (0-255) in
 * indexed modes.
 *
 * It's the caller's responsibility to handle the color values correctly
 * according to the canvas mode (truecolor or indexed).
 *
 * Since: 1.8
 **/
void
chafa_canvas_get_raw_colors_at (ChafaCanvas *canvas, gint x, gint y,
                                gint *fg_out, gint *bg_out)
{
    const ChafaCanvasCell *cell;
    gint fg = -1, bg = -1;

    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (x >= 0 && x < canvas->config.width);
    g_return_if_fail (y >= 0 && y < canvas->config.height);

    cell = &canvas->cells [y * canvas->config.width + x];

    switch (canvas->config.canvas_mode)
    {
        case CHAFA_CANVAS_MODE_TRUECOLOR:
            fg = packed_rgba_to_rgb (canvas, cell->fg_color);
            bg = packed_rgba_to_rgb (canvas, cell->bg_color);
            break;
        case CHAFA_CANVAS_MODE_INDEXED_256:
        case CHAFA_CANVAS_MODE_INDEXED_240:
        case CHAFA_CANVAS_MODE_INDEXED_16:
        case CHAFA_CANVAS_MODE_INDEXED_16_8:
        case CHAFA_CANVAS_MODE_INDEXED_8:
            fg = cell->fg_color < 256 ? (gint) cell->fg_color : -1;
            bg = cell->bg_color < 256 ? (gint) cell->bg_color : -1;
            break;
        case CHAFA_CANVAS_MODE_FGBG_BGFG:
            fg = cell->fg_color == CHAFA_PALETTE_INDEX_FG ? 0 : -1;
            bg = cell->bg_color == CHAFA_PALETTE_INDEX_FG ? 0 : -1;
            break;
        case CHAFA_CANVAS_MODE_FGBG:
            fg = 0;
            bg = -1;
            break;
        case CHAFA_CANVAS_MODE_MAX:
            g_assert_not_reached ();
            break;
    }

    if (fg_out)
        *fg_out = fg;
    if (bg_out)
        *bg_out = bg;
}

/**
 * chafa_canvas_set_raw_colors_at:
 * @canvas: The canvas to manipulate
 * @x: Column of character cell to manipulate
 * @y: Row of character cell to manipulate
 * @fg: Foreground color
 * @bg: Background color
 *
 * Sets the colors at cell (x, y). The coordinates are zero-indexed. For
 * double-width characters, both cells will be set to the same color.
 *
 * The colors must be -1 for transparency, packed 8bpc RGB, i.e.
 * 0x00RRGGBB hex in truecolor mode, or the raw pen value (0-255) in
 * indexed modes.
 *
 * It's the caller's responsibility to handle the color values correctly
 * according to the canvas mode (truecolor or indexed).
 *
 * Since: 1.8
 **/
void
chafa_canvas_set_raw_colors_at (ChafaCanvas *canvas, gint x, gint y,
                                gint fg, gint bg)
{
    ChafaCanvasCell *cell;

    g_return_if_fail (canvas != NULL);
    g_return_if_fail (canvas->refs > 0);
    g_return_if_fail (x >= 0 && x < canvas->config.width);
    g_return_if_fail (y >= 0 && y < canvas->config.height);

    cell = &canvas->cells [y * canvas->config.width + x];

    switch (canvas->config.canvas_mode)
    {
        case CHAFA_CANVAS_MODE_TRUECOLOR:
            cell->fg_color = packed_rgb_to_rgba (fg);
            cell->bg_color = packed_rgb_to_rgba (bg);
            break;
        case CHAFA_CANVAS_MODE_INDEXED_256:
        case CHAFA_CANVAS_MODE_INDEXED_240:
        case CHAFA_CANVAS_MODE_INDEXED_16:
        case CHAFA_CANVAS_MODE_INDEXED_16_8:
        case CHAFA_CANVAS_MODE_INDEXED_8:
            cell->fg_color = fg >= 0 ? fg : CHAFA_PALETTE_INDEX_TRANSPARENT;
            cell->bg_color = bg >= 0 ? bg : CHAFA_PALETTE_INDEX_TRANSPARENT;
            break;
        case CHAFA_CANVAS_MODE_FGBG_BGFG:
            cell->fg_color = fg >= 0 ? CHAFA_PALETTE_INDEX_FG : CHAFA_PALETTE_INDEX_TRANSPARENT;
            cell->bg_color = bg >= 0 ? CHAFA_PALETTE_INDEX_FG : CHAFA_PALETTE_INDEX_TRANSPARENT;
            break;
        case CHAFA_CANVAS_MODE_FGBG:
            cell->fg_color = fg >= 0 ? fg : CHAFA_PALETTE_INDEX_TRANSPARENT;
            break;
        case CHAFA_CANVAS_MODE_MAX:
            g_assert_not_reached ();
            break;
    }

    /* If setting the color of half a wide char, set it for the other half too */

    if (x > 0 && cell->c == 0)
    {
        cell [-1].fg_color = cell->fg_color;
        cell [-1].bg_color = cell->bg_color;
    }

    if (x < canvas->config.width - 1 && cell [1].c == 0)
    {
        cell [1].fg_color = cell->fg_color;
        cell [1].bg_color = cell->bg_color;
    }
}
