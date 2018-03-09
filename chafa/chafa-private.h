#include <glib.h>

/* Colors and color spaces */

#define CHAFA_PALETTE_INDEX_BLACK 0
#define CHAFA_PALETTE_INDEX_WHITE 15
#define CHAFA_PALETTE_INDEX_TRANSPARENT 256

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
    ChafaSymbolClass sc;
    gunichar c;
    gchar *coverage;
    gint fg_weight, bg_weight;
    gboolean have_mixed;
}
ChafaSymbol;

/* Canvas */

typedef struct ChafaCanvasCell ChafaCanvasCell;

/* Library functions */

extern ChafaSymbol *chafa_symbols;
extern ChafaSymbol *chafa_fill_symbols;

void chafa_init_palette (void);
void chafa_init_symbols (void);

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

#ifdef HAVE_MMX_INTRINSICS
void calc_colors_mmx (const ChafaPixel *pixels, ChafaColor *cols, const guint8 *cov);
void leave_mmx (void);
#endif

#ifdef HAVE_SSE41_INTRINSICS
gint calc_error_sse41 (const ChafaPixel *pixels, const ChafaColor *cols, const guint8 *cov);
#endif
