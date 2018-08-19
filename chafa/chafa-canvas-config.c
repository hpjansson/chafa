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
#include "chafa/chafa.h"
#include "chafa/chafa-private.h"

/**
 * SECTION:chafa-canvas-config
 * @title: ChafaCanvasConfig
 * @short_description: Describes a configuration for #ChafaCanvas
 *
 * A #ChafaCanvasConfig describes a set of parameters for #ChafaCanvas, such
 * as its geometry, color space and other output characteristics.
 *
 * To create a new #ChafaCanvasConfig, use chafa_canvas_config_new (). You
 * can then modify it using its setters, e.g. chafa_canvas_config_set_canvas_mode ()
 * before assigning it to a new #ChafaCanvas with chafa_canvas_new ().
 *
 * Note that it is not possible to change a canvas' configuration after
 * the canvas is created.
 **/

/**
 * ChafaCanvasMode:
 * @CHAFA_CANVAS_MODE_TRUECOLOR: Truecolor.
 * @CHAFA_CANVAS_MODE_INDEXED_256: 256 colors.
 * @CHAFA_CANVAS_MODE_INDEXED_240: 256 colors, but avoid using the lower 16 whose values vary between terminal environments.
 * @CHAFA_CANVAS_MODE_INDEXED_16: 16 colors using the aixterm ANSI extension.
 * @CHAFA_CANVAS_MODE_FGBG_BGFG: Default foreground and background colors, plus inversion.
 * @CHAFA_CANVAS_MODE_FGBG: Default foreground and background colors. No ANSI codes will be used.
 * @CHAFA_CANVAS_MODE_MAX: Last supported canvas mode plus one.
 **/

/**
 * ChafaColorSpace:
 * @CHAFA_COLOR_SPACE_RGB: RGB color space. Fast but imprecise.
 * @CHAFA_COLOR_SPACE_DIN99D: DIN99d color space. Slower, but good perceptual color precision.
 * @CHAFA_COLOR_SPACE_MAX: Last supported color space plus one.
 **/

/* Private */

void
chafa_canvas_config_init (ChafaCanvasConfig *canvas_config)
{
    g_return_if_fail (canvas_config != NULL);

    memset (canvas_config, 0, sizeof (*canvas_config));
    canvas_config->refs = 1;

    canvas_config->canvas_mode = CHAFA_CANVAS_MODE_TRUECOLOR;
    canvas_config->color_space = CHAFA_COLOR_SPACE_RGB;
    canvas_config->width = 80;
    canvas_config->height = 24;
    canvas_config->fg_color_packed_rgb = 0xffffff;
    canvas_config->bg_color_packed_rgb = 0x000000;
    canvas_config->alpha_threshold = 127;
    canvas_config->work_factor = 0.5;
    canvas_config->preprocessing_enabled = TRUE;

    chafa_symbol_map_init (&canvas_config->symbol_map);
    chafa_symbol_map_add_by_tags (&canvas_config->symbol_map, CHAFA_SYMBOL_TAG_ALL);

    chafa_symbol_map_init (&canvas_config->fill_symbol_map);
}

void
chafa_canvas_config_deinit (ChafaCanvasConfig *canvas_config)
{
    g_return_if_fail (canvas_config != NULL);

    chafa_symbol_map_deinit (&canvas_config->symbol_map);
    chafa_symbol_map_deinit (&canvas_config->fill_symbol_map);
}

void
chafa_canvas_config_copy_contents (ChafaCanvasConfig *dest, const ChafaCanvasConfig *src)
{
    g_return_if_fail (dest != NULL);
    g_return_if_fail (src != NULL);

    memcpy (dest, src, sizeof (*dest));
    chafa_symbol_map_copy_contents (&dest->symbol_map, &src->symbol_map);
    chafa_symbol_map_copy_contents (&dest->fill_symbol_map, &src->fill_symbol_map);
    dest->refs = 1;
}

/* Public */

/**
 * chafa_canvas_config_new:
 *
 * Creates a new #ChafaCanvasConfig with default settings. This
 * object can later be used in the creation of a #ChafaCanvas.
 *
 * Returns: The new #ChafaCanvasConfig
 **/
ChafaCanvasConfig *
chafa_canvas_config_new (void)
{
    ChafaCanvasConfig *canvas_config;

    canvas_config = g_new (ChafaCanvasConfig, 1);
    chafa_canvas_config_init (canvas_config);
    return canvas_config;
}

/**
 * chafa_canvas_config_copy:
 * @config: A #ChafaSymbolMap to copy.
 *
 * Creates a new #ChafaCanvasConfig that's a copy of @config.
 *
 * Returns: The new #ChafaCanvasConfig
 **/
ChafaCanvasConfig *
chafa_canvas_config_copy (const ChafaCanvasConfig *config)
{
    ChafaCanvasConfig *new_config;

    new_config = g_new (ChafaCanvasConfig, 1);
    chafa_canvas_config_copy_contents (new_config, config);
    return new_config;
}

/**
 * chafa_canvas_config_ref:
 * @config: #ChafaCanvasConfig to add a reference to.
 *
 * Adds a reference to @config.
 **/
void
chafa_canvas_config_ref (ChafaCanvasConfig *config)
{
    gint refs;

    g_return_if_fail (config != NULL);
    refs = g_atomic_int_get (&config->refs);
    g_return_if_fail (refs > 0);

    g_atomic_int_inc (&config->refs);
}

/**
 * chafa_canvas_config_unref:
 * @config: #ChafaCanvasConfig to remove a reference from.
 *
 * Removes a reference from @config.
 **/
void
chafa_canvas_config_unref (ChafaCanvasConfig *config)
{
    gint refs;

    g_return_if_fail (config != NULL);
    refs = g_atomic_int_get (&config->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&config->refs))
    {
        chafa_canvas_config_deinit (config);
        g_free (config);
    }
}

/**
 * chafa_canvas_config_get_geometry:
 * @config: A #ChafaCanvasConfig
 * @width_out: Location to store width in, or %NULL
 * @height_out: Location to store height in, or %NULL
 *
 * Returns @config's width and height in character cells in the
 * provided output locations.
 **/
void
chafa_canvas_config_get_geometry (const ChafaCanvasConfig *config, gint *width_out, gint *height_out)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);

    if (width_out)
        *width_out = config->width;
    if (height_out)
        *height_out = config->height;
}

/**
 * chafa_canvas_config_set_geometry:
 * @config: A #ChafaCanvasConfig
 * @width: Width in character cells
 * @height: Height in character cells
 *
 * Sets @config's width and height in character cells to @width x @height.
 **/
void
chafa_canvas_config_set_geometry (ChafaCanvasConfig *config, gint width, gint height)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);
    g_return_if_fail (width > 0);
    g_return_if_fail (height > 0);

    config->width = width;
    config->height = height;
}

/**
 * chafa_canvas_config_get_canvas_mode:
 * @config: A #ChafaCanvasConfig
 *
 * Returns @config's #ChafaCanvasMode.
 *
 * Returns: The #ChafaCanvasMode.
 **/
ChafaCanvasMode
chafa_canvas_config_get_canvas_mode (const ChafaCanvasConfig *config)
{
    g_return_val_if_fail (config != NULL, CHAFA_CANVAS_MODE_TRUECOLOR);
    g_return_val_if_fail (config->refs > 0, CHAFA_CANVAS_MODE_TRUECOLOR);

    return config->canvas_mode;
}

/**
 * chafa_canvas_config_set_canvas_mode:
 * @config: A #ChafaCanvasConfig
 * @mode: A #ChafaCanvasMode
 *
 * Sets @config's stored #ChafaCanvasMode to @mode.
 **/
void
chafa_canvas_config_set_canvas_mode (ChafaCanvasConfig *config, ChafaCanvasMode mode)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);
    g_return_if_fail (mode < CHAFA_CANVAS_MODE_MAX);

    config->canvas_mode = mode;
}

/**
 * chafa_canvas_config_get_color_space:
 * @config: A #ChafaCanvasConfig
 *
 * Returns @config's #ChafaColorSpace.
 *
 * Returns: The #ChafaColorSpace.
 **/
ChafaColorSpace
chafa_canvas_config_get_color_space (const ChafaCanvasConfig *config)
{
    g_return_val_if_fail (config != NULL, CHAFA_COLOR_SPACE_RGB);
    g_return_val_if_fail (config->refs > 0, CHAFA_COLOR_SPACE_RGB);

    return config->color_space;
}

/**
 * chafa_canvas_config_set_color_space:
 * @config: A #ChafaCanvasConfig
 * @color_space: A #ChafaColorSpace
 *
 * Sets @config's stored #ChafaColorSpace to @color_space.
 **/
void
chafa_canvas_config_set_color_space (ChafaCanvasConfig *config, ChafaColorSpace color_space)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);
    g_return_if_fail (color_space < CHAFA_COLOR_SPACE_MAX);

    config->color_space = color_space;
}

/**
 * chafa_canvas_config_peek_symbol_map:
 * @config: A #ChafaCanvasConfig
 *
 * Returns a pointer to the symbol map belonging to @config.
 * This can be inspected using the #ChafaSymbolMap getter
 * functions, but not changed.
 *
 * Returns: A pointer to the config's immutable symbol map
 **/
const ChafaSymbolMap *
chafa_canvas_config_peek_symbol_map (const ChafaCanvasConfig *config)
{
    g_return_val_if_fail (config != NULL, NULL);
    g_return_val_if_fail (config->refs > 0, NULL);

    return &config->symbol_map;
}

/**
 * chafa_canvas_config_set_symbol_map:
 * @config: A #ChafaCanvasConfig
 * @symbol_map: A #ChafaSymbolMap
 *
 * Assigns a copy of @symbol_map to @config.
 **/
void
chafa_canvas_config_set_symbol_map (ChafaCanvasConfig *config, const ChafaSymbolMap *symbol_map)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);

    chafa_symbol_map_deinit (&config->symbol_map);
    chafa_symbol_map_copy_contents (&config->symbol_map, symbol_map);
}

/**
 * chafa_canvas_config_peek_fill_symbol_map:
 * @config: A #ChafaCanvasConfig
 *
 * Returns a pointer to the fill symbol map belonging to @config.
 * This can be inspected using the #ChafaSymbolMap getter
 * functions, but not changed.
 *
 * Fill symbols are assigned according to their overall foreground to
 * background coverage, disregarding shape.
 *
 * Returns: A pointer to the config's immutable fill symbol map
 **/
const ChafaSymbolMap *
chafa_canvas_config_peek_fill_symbol_map (const ChafaCanvasConfig *config)
{
    g_return_val_if_fail (config != NULL, NULL);
    g_return_val_if_fail (config->refs > 0, NULL);

    return &config->fill_symbol_map;
}

/**
 * chafa_canvas_config_set_fill_symbol_map:
 * @config: A #ChafaCanvasConfig
 * @fill_symbol_map: A #ChafaSymbolMap
 *
 * Assigns a copy of @fill_symbol_map to @config.
 **/
void
chafa_canvas_config_set_fill_symbol_map (ChafaCanvasConfig *config, const ChafaSymbolMap *fill_symbol_map)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);

    chafa_symbol_map_deinit (&config->fill_symbol_map);
    chafa_symbol_map_copy_contents (&config->fill_symbol_map, fill_symbol_map);
}

/**
 * chafa_canvas_config_get_transparency_threshold:
 * @config: A #ChafaCanvasConfig
 *
 * Returns the threshold above which full transparency will be used.
 *
 * Returns: The transparency threshold [0.0 - 1.0]
 **/
gfloat
chafa_canvas_config_get_transparency_threshold (const ChafaCanvasConfig *config)
{
    g_return_val_if_fail (config != NULL, 0.0);
    g_return_val_if_fail (config->refs > 0, 0.0);

    return 1.0 - (config->alpha_threshold / 256.0);
}

/**
 * chafa_canvas_config_set_transparency_threshold:
 * @config: A #ChafaCanvasConfig
 * @alpha_threshold: The transparency threshold [0.0 - 1.0].
 *
 * Sets the threshold above which full transparency will be used.
 **/
void
chafa_canvas_config_set_transparency_threshold (ChafaCanvasConfig *config, gfloat alpha_threshold)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);
    g_return_if_fail (alpha_threshold >= 0.0);
    g_return_if_fail (alpha_threshold <= 1.0);

    /* Invert the scale; internally it's more like an opacity threshold */
    config->alpha_threshold = 256.0 * (1.0 - alpha_threshold);
}

/**
 * chafa_canvas_config_get_fg_color:
 * @config: A #ChafaCanvasConfig
 *
 * Gets the assumed foreground color of the output device. This is used to
 * determine how to apply the foreground pen in FGBG modes.
 *
 * Returns: Foreground color as packed RGB triplet
 **/
guint32
chafa_canvas_config_get_fg_color (const ChafaCanvasConfig *config)
{
    g_return_val_if_fail (config != NULL, 0);
    g_return_val_if_fail (config->refs > 0, 0);

    return config->fg_color_packed_rgb;
}

/**
 * chafa_canvas_config_set_fg_color:
 * @config: A #ChafaCanvasConfig
 * @fg_color_packed_rgb: Foreground color as packed RGB triplet
 *
 * Sets the assumed foreground color of the output device. This is used to
 * determine how to apply the foreground pen in FGBG modes.
 **/
void
chafa_canvas_config_set_fg_color (ChafaCanvasConfig *config, guint32 fg_color_packed_rgb)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);

    config->fg_color_packed_rgb = fg_color_packed_rgb;
}

/**
 * chafa_canvas_config_get_bg_color:
 * @config: A #ChafaCanvasConfig
 *
 * Gets the assumed background color of the output device. This is used to
 * determine how to apply the background pen in FGBG modes.
 *
 * Returns: Background color as packed RGB triplet
 **/
guint32
chafa_canvas_config_get_bg_color (const ChafaCanvasConfig *config)
{
    g_return_val_if_fail (config != NULL, 0);
    g_return_val_if_fail (config->refs > 0, 0);

    return config->bg_color_packed_rgb;
}

/**
 * chafa_canvas_config_set_bg_color:
 * @config: A #ChafaCanvasConfig
 * @bg_color_packed_rgb: Background color as packed RGB triplet
 *
 * Sets the assumed background color of the output device. This is used to
 * determine how to apply the background and transparency pens in FGBG modes,
 * and will also be substituted for partial transparency.
 **/
void
chafa_canvas_config_set_bg_color (ChafaCanvasConfig *config, guint32 bg_color_packed_rgb)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);

    config->bg_color_packed_rgb = bg_color_packed_rgb;
}

/**
 * chafa_canvas_config_get_work_factor:
 * @config: A #ChafaCanvasConfig
 *
 * Gets the work/quality tradeoff factor. A higher value means more time
 * and memory will be spent towards a higher quality output.
 *
 * Returns: The work factor [0.0 - 1.0]
 **/
gfloat
chafa_canvas_config_get_work_factor (const ChafaCanvasConfig *config)
{
    g_return_val_if_fail (config != NULL, 1);
    g_return_val_if_fail (config->refs > 0, 1);

    return config->work_factor;
}

/**
 * chafa_canvas_config_set_work_factor:
 * @config: A #ChafaCanvasConfig
 * @work_factor: Work factor [0.0 - 1.0]
 *
 * Sets the work/quality tradeoff factor. A higher value means more time
 * and memory will be spent towards a higher quality output.
 *
 **/
void
chafa_canvas_config_set_work_factor (ChafaCanvasConfig *config, gfloat work_factor)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);
    g_return_if_fail (work_factor >= 0.0 && work_factor <= 1.0);

    config->work_factor = work_factor;
}

/**
 * chafa_canvas_config_get_preprocessing_enabled:
 * @config: A #ChafaCanvasConfig
 *
 * Queries whether automatic image preprocessing is enabled. This allows
 * Chafa to boost contrast and saturation in an attempt to improve
 * legibility. The type of preprocessing applied (if any) depends on the
 * canvas mode.
 *
 * Returns: Whether automatic preprocessing is enabled
 **/
gboolean
chafa_canvas_config_get_preprocessing_enabled (const ChafaCanvasConfig *config)
{
    g_return_val_if_fail (config != NULL, FALSE);
    g_return_val_if_fail (config->refs > 0, FALSE);

    return config->preprocessing_enabled;
}

/**
 * chafa_canvas_config_set_preprocessing_enabled:
 * @config: A #ChafaCanvasConfig
 * @preprocessing_enabled: Whether automatic preprocessing should be enabled
 *
 * Indicates whether automatic image preprocessing should be enabled. This
 * allows Chafa to boost contrast and saturation in an attempt to improve
 * legibility. The type of preprocessing applied (if any) depends on the
 * canvas mode.
 **/
void
chafa_canvas_config_set_preprocessing_enabled (ChafaCanvasConfig *config, gboolean preprocessing_enabled)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);

    config->preprocessing_enabled = preprocessing_enabled;
}
