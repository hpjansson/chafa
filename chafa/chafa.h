#include <glib.h>

/* Exported symbol versioning */

#ifndef _CHAFA_EXTERN
# define _CHAFA_EXTERN extern
#endif

#define CHAFA_AVAILABLE_IN_ALL _CHAFA_EXTERN

/* Colors and color spaces */

#define CHAFA_PALETTE_INDEX_BLACK 0
#define CHAFA_PALETTE_INDEX_WHITE 15
#define CHAFA_PALETTE_INDEX_TRANSPARENT 256

typedef enum
{
    CHAFA_COLOR_SPACE_RGB,
    CHAFA_COLOR_SPACE_DIN99D
}
ChafaColorSpace;

#define CHAFA_COLOR_SPACE_MAX (CHAFA_COLOR_SPACE_DIN99D + 1)

/* Color space agnostic, using fixed point */
typedef struct
{
  gint ch [4];
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

#define CHAFA_SYMBOL_WIDTH_PIXELS 8
#define CHAFA_SYMBOL_HEIGHT_PIXELS 8
#define CHAFA_SYMBOL_N_PIXELS (CHAFA_SYMBOL_WIDTH_PIXELS * CHAFA_SYMBOL_HEIGHT_PIXELS)

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

typedef struct
{
    ChafaSymbolClass sc;
    gunichar c;
    gchar *coverage;
    gint fg_weight, bg_weight;
    gboolean have_mixed;
}
ChafaSymbol;

/* Canvas */

typedef enum
{
    CHAFA_CANVAS_MODE_RGBA,
    CHAFA_CANVAS_MODE_INDEXED_256,
    CHAFA_CANVAS_MODE_INDEXED_240,
    CHAFA_CANVAS_MODE_INDEXED_16,
    CHAFA_CANVAS_MODE_INDEXED_WHITE_ON_BLACK,
    CHAFA_CANVAS_MODE_INDEXED_BLACK_ON_WHITE,
    CHAFA_CANVAS_MODE_SHAPES_WHITE_ON_BLACK,
    CHAFA_CANVAS_MODE_SHAPES_BLACK_ON_WHITE
}
ChafaCanvasMode;

typedef struct ChafaCanvas ChafaCanvas;
typedef struct ChafaCanvasCell ChafaCanvasCell;

/* Library functions */

extern ChafaSymbol *chafa_symbols;
extern ChafaSymbol *chafa_fill_symbols;

void chafa_init_palette (void);
void chafa_init_symbols (void);

CHAFA_AVAILABLE_IN_ALL
ChafaCanvas *chafa_canvas_new (ChafaCanvasMode mode, gint width, gint height);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_unref (ChafaCanvas *canvas);

CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_set_color_space (ChafaCanvas *canvas, ChafaColorSpace color_space);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_set_include_symbols (ChafaCanvas *canvas, guint32 include_symbols);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_set_exclude_symbols (ChafaCanvas *canvas, guint32 exclude_symbols);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_set_transparency_threshold (ChafaCanvas *canvas, gfloat alpha_threshold);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_set_transparency_color (ChafaCanvas *canvas, guint32 alpha_color_packed_rgb);
CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_set_quality (ChafaCanvas *canvas, gint quality);

CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_paint_rgba (ChafaCanvas *canvas, guint8 *pixels, gint width, gint height);

CHAFA_AVAILABLE_IN_ALL
void chafa_canvas_print (ChafaCanvas *canvas);

guint32 chafa_pack_color (const ChafaColor *color);
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
gint chafa_color_diff_slow (const ChafaColor *col_a, const ChafaColor *col_b, ChafaColorSpace color_space);

void chafa_color_div_scalar (ChafaColor *color, gint scalar);

void chafa_color_rgb_to_din99d (const ChafaColor *rgb, ChafaColor *din99);

/* Ratio is in the range 0-1000 */
void chafa_color_mix (ChafaColor *out, const ChafaColor *a, const ChafaColor *b, gint ratio);

/* Takes values 0-255 for r, g, b and returns a universal palette index 0-255 */
gint chafa_pick_color_256 (const ChafaColor *color, ChafaColorSpace color_space);

/* Takes values 0-255 for r, g, b and returns a universal palette index 16-255 */
gint chafa_pick_color_240 (const ChafaColor *color, ChafaColorSpace color_space);

/* Takes values 0-255 for r, g, b and returns a universal palette index 0-15 */
gint chafa_pick_color_16 (const ChafaColor *color, ChafaColorSpace color_space);

/* Takes values 0-255 for r, g, b and returns 0 for black and 1 for white */
gint chafa_pick_color_2 (const ChafaColor *color, ChafaColorSpace color_space);

const ChafaColor *chafa_get_palette_color_256 (guint index, ChafaColorSpace color_space);
