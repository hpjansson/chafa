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
#include <glib.h>
#include "chafa/chafa.h"
#include "chafa/chafa-private.h"

/* Private */

void
chafa_canvas_config_init (ChafaCanvasConfig *canvas_config)
{
    g_return_if_fail (canvas_config != NULL);

    memset (canvas_config, 0, sizeof (*canvas_config));
    canvas_config->refs = 1;

    canvas_config->canvas_mode = CHAFA_CANVAS_MODE_TRUECOLOR;
    canvas_config->color_space = CHAFA_COLOR_SPACE_RGB;
    canvas_config->include_symbols = CHAFA_SYMBOLS_ALL;
    canvas_config->exclude_symbols = CHAFA_SYMBOLS_NONE;
    canvas_config->fg_color_packed_rgb = 0xffffff;
    canvas_config->bg_color_packed_rgb = 0x000000;
    canvas_config->alpha_threshold = 127;
    canvas_config->quality = 5;
}

void
chafa_canvas_config_deinit (ChafaCanvasConfig *canvas_config)
{
    g_return_if_fail (canvas_config != NULL);
}

void
chafa_canvas_config_copy_contents (ChafaCanvasConfig *dest, const ChafaCanvasConfig *src)
{
    g_return_if_fail (dest != NULL);
    g_return_if_fail (src != NULL);

    memcpy (dest, src, sizeof (*dest));
}

/* Public */

ChafaCanvasConfig *
chafa_canvas_config_new (void)
{
    ChafaCanvasConfig *canvas_config;

    canvas_config = g_new (ChafaCanvasConfig, 1);
    chafa_canvas_config_init (canvas_config);
    return canvas_config;
}

void
chafa_canvas_config_ref (ChafaCanvasConfig *canvas_config)
{
    g_return_if_fail (canvas_config != NULL);
    g_return_if_fail (canvas_config->refs > 0);

    canvas_config->refs++;
}

void
chafa_canvas_config_unref (ChafaCanvasConfig *canvas_config)
{
    g_return_if_fail (canvas_config != NULL);
    g_return_if_fail (canvas_config->refs > 0);

    if (--canvas_config->refs == 0)
    {
        chafa_canvas_config_deinit (canvas_config);
        g_free (canvas_config);
    }
}

void
chafa_canvas_config_set_canvas_mode (ChafaCanvasConfig *config, ChafaCanvasMode mode)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);
    g_return_if_fail (mode < CHAFA_CANVAS_MODE_MAX);

    config->canvas_mode = mode;
}

void
chafa_canvas_config_set_color_space (ChafaCanvasConfig *config, ChafaColorSpace color_space)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);
    g_return_if_fail (color_space < CHAFA_COLOR_SPACE_MAX);

    config->color_space = color_space;
}

void
chafa_canvas_config_set_include_symbols (ChafaCanvasConfig *config, ChafaSymbolClass include_symbols)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);

    config->include_symbols = include_symbols;
}

void
chafa_canvas_config_set_exclude_symbols (ChafaCanvasConfig *config, ChafaSymbolClass exclude_symbols)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);

    config->exclude_symbols = exclude_symbols;
}

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

void
chafa_canvas_config_set_fg_color (ChafaCanvasConfig *config, guint32 fg_color_packed_rgb)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);

    config->fg_color_packed_rgb = fg_color_packed_rgb;
}

void
chafa_canvas_config_set_bg_color (ChafaCanvasConfig *config, guint32 bg_color_packed_rgb)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);

    config->bg_color_packed_rgb = bg_color_packed_rgb;
}

void
chafa_canvas_config_set_quality (ChafaCanvasConfig *config, gint quality)
{
    g_return_if_fail (config != NULL);
    g_return_if_fail (config->refs > 0);
    g_return_if_fail (quality >= 1 && quality <= 9);

    config->quality = quality;
}
