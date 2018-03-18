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

#ifndef __CHAFA_H__
#define __CHAFA_H__

#include <glib.h>

G_BEGIN_DECLS

/* Exported symbol versioning/visibility */

#ifndef _CHAFA_EXTERN
# define _CHAFA_EXTERN extern
#endif

#define CHAFA_AVAILABLE_IN_ALL _CHAFA_EXTERN

/* Color spaces */

typedef enum
{
    CHAFA_COLOR_SPACE_RGB,
    CHAFA_COLOR_SPACE_DIN99D
}
ChafaColorSpace;

#define CHAFA_COLOR_SPACE_MAX (CHAFA_COLOR_SPACE_DIN99D + 1)

/* Character symbols and symbol classes */

#define CHAFA_SYMBOL_WIDTH_PIXELS 8
#define CHAFA_SYMBOL_HEIGHT_PIXELS 8

#define CHAFA_SYMBOLS_ALL  0xffffffff
#define CHAFA_SYMBOLS_NONE 0

typedef enum
{
    CHAFA_SYMBOL_CLASS_SPACE    = (1 <<  0),
    CHAFA_SYMBOL_CLASS_SOLID    = (1 <<  1),
    CHAFA_SYMBOL_CLASS_STIPPLE  = (1 <<  2),
    CHAFA_SYMBOL_CLASS_BLOCK    = (1 <<  3),
    CHAFA_SYMBOL_CLASS_BORDER   = (1 <<  4),
    CHAFA_SYMBOL_CLASS_DIAGONAL = (1 <<  5),
    CHAFA_SYMBOL_CLASS_DOT      = (1 <<  6),
    CHAFA_SYMBOL_CLASS_QUAD     = (1 <<  7),
    CHAFA_SYMBOL_CLASS_HALF     = (1 <<  8),
}
ChafaSymbolClass;

/* Canvas modes */

typedef enum
{
    CHAFA_CANVAS_MODE_TRUECOLOR,
    CHAFA_CANVAS_MODE_INDEXED_256,
    CHAFA_CANVAS_MODE_INDEXED_240,
    CHAFA_CANVAS_MODE_INDEXED_16,
    CHAFA_CANVAS_MODE_FGBG_BGFG,
    CHAFA_CANVAS_MODE_FGBG
}
ChafaCanvasMode;

#define CHAFA_CANVAS_MODE_MAX (CHAFA_CANVAS_MODE_FGBG + 1)

/* Canvas config */

typedef struct ChafaCanvasConfig ChafaCanvasConfig;

CHAFA_AVAILABLE_IN_ALL
ChafaCanvasConfig *chafa_canvas_config_new (void);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_config_ref (ChafaCanvasConfig *config);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_config_unref (ChafaCanvasConfig *config);

CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_config_set_canvas_mode (ChafaCanvasConfig *config, ChafaCanvasMode mode);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_config_set_color_space (ChafaCanvasConfig *config, ChafaColorSpace color_space);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_config_set_include_symbols (ChafaCanvasConfig *config, guint32 include_symbols);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_config_set_exclude_symbols (ChafaCanvasConfig *config, guint32 exclude_symbols);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_config_set_transparency_threshold (ChafaCanvasConfig *config, gfloat alpha_threshold);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_config_set_fg_color (ChafaCanvasConfig *config, guint32 fg_color_packed_rgb);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_config_set_bg_color (ChafaCanvasConfig *config, guint32 bg_color_packed_rgb);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_config_set_quality (ChafaCanvasConfig *config, gint quality);

/* Canvas */

typedef struct ChafaCanvas ChafaCanvas;

CHAFA_AVAILABLE_IN_ALL
ChafaCanvas *chafa_canvas_new (ChafaCanvasConfig *config, gint width, gint height);
CHAFA_AVAILABLE_IN_ALL
ChafaCanvas *chafa_canvas_new_similar (ChafaCanvas *orig);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_ref (ChafaCanvas *canvas);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_unref (ChafaCanvas *canvas);

CHAFA_AVAILABLE_IN_ALL
const ChafaCanvasConfig *chafa_canvas_peek_config (ChafaCanvas *canvas);

CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_paint_rgba (ChafaCanvas *canvas, guint8 *src_pixels,
                              gint src_width, gint src_height, gint src_rowstride);

CHAFA_AVAILABLE_IN_ALL
GString *chafa_canvas_build_gstring (ChafaCanvas *canvas);
CHAFA_AVAILABLE_IN_ALL
gchar *chafa_canvas_build_str (ChafaCanvas *canvas);

G_END_DECLS

#endif /* __CHAFA_H__ */
