#include <glib.h>

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
    CHAFA_SYMBOL_CLASS_DOT      = (1 <<  6)
}
ChafaSymbolClass;

typedef struct
{
    ChafaSymbolClass sc;
    gunichar c;
    gchar *coverage;
}
ChafaSymbol;

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

extern const ChafaSymbol chafa_symbols [];
extern const ChafaSymbol chafa_fill_symbols [];

void chafa_init_palette (void);

ChafaCanvas *chafa_canvas_new (ChafaCanvasMode mode, gint width, gint height);
void chafa_canvas_unref (ChafaCanvas *canvas);

void chafa_canvas_set_color_space (ChafaCanvas *canvas, ChafaColorSpace color_space);
void chafa_canvas_set_include_symbols (ChafaCanvas *canvas, guint32 include_symbols);
void chafa_canvas_set_exclude_symbols (ChafaCanvas *canvas, guint32 exclude_symbols);
void chafa_canvas_set_transparency_threshold (ChafaCanvas *canvas, gfloat alpha_threshold);
void chafa_canvas_set_transparency_color (ChafaCanvas *canvas, guint32 alpha_color_packed_rgb);
void chafa_canvas_set_quality (ChafaCanvas *canvas, gint quality);

void chafa_canvas_paint_rgba (ChafaCanvas *canvas, guint8 *pixels, gint width, gint height);

void chafa_canvas_print (ChafaCanvas *canvas);

guint32 chafa_pack_color (const ChafaColor *color);
void chafa_unpack_color (guint32 packed, ChafaColor *color_out);

void chafa_color_add (ChafaColor *accum, const ChafaColor *src);
void chafa_color_div_scalar (ChafaColor *color, gint scalar);
gint chafa_color_diff (const ChafaColor *col_a, const ChafaColor *col_b,
                      ChafaColorSpace color_space);

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
