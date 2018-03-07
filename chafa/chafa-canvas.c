#include "config.h"

#ifdef HAVE_MMX_INTRINSICS
# include <mmintrin.h>
#endif

#ifdef HAVE_SSE41_INTRINSICS
# include <smmintrin.h>
#endif

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <glib.h>
#include <wand/MagickWand.h>
#include "chafa/chafa.h"
#include "chafa/chafa-private.h"

/* Maximum number of symbols in symbols[]. Used for statically allocated arrays */
#define SYMBOLS_MAX 256

/* Fixed point multiplier */
#define FIXED_MULT 4096

struct ChafaCanvasCell
{
    gunichar c;

    /* Colors can be either packed RGBA or index */
    guint32 fg_color;
    guint32 bg_color;
};

struct ChafaCanvas
{
    gint refs;
    ChafaCanvasMode mode;
    ChafaColorSpace color_space;
    guint32 include_symbols;
    guint32 exclude_symbols;
    gint width, height;
    gint width_pixels, height_pixels;
    ChafaPixel *pixels;
    ChafaCanvasCell *cells;
    guint32 alpha_color_packed_rgb;
    ChafaColor alpha_color;
    gint alpha_threshold;  /* 0-255. 255 = no alpha in output */
    guint have_alpha : 1;
    gint quality;
};

typedef struct
{
    ChafaPixel fg;
    ChafaPixel bg;
    gint error;
}
SymbolEval;

/* pixels_out must point to CHAFA_SYMBOL_N_PIXELS-element array */
static void
fetch_canvas_pixel_block (ChafaCanvas *canvas, gint cx, gint cy, ChafaPixel *pixels_out)
{
    ChafaPixel *row_p;
    ChafaPixel *end_p;
    gint i = 0;

    row_p = &canvas->pixels [cy * CHAFA_SYMBOL_HEIGHT_PIXELS * canvas->width_pixels + cx * CHAFA_SYMBOL_WIDTH_PIXELS];
    end_p = row_p + (canvas->width_pixels * CHAFA_SYMBOL_HEIGHT_PIXELS);

    for ( ; row_p < end_p; row_p += canvas->width_pixels)
    {
        ChafaPixel *p0 = row_p;
        ChafaPixel *p1 = p0 + CHAFA_SYMBOL_WIDTH_PIXELS;

        for ( ; p0 < p1; p0++)
            pixels_out [i++] = *p0;
    }
}

static void
calc_mean_color (ChafaCanvas *canvas, ChafaPixel *pixels, ChafaColor *color_out)
{
    ChafaColor accum = { 0 };
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        chafa_color_add (&accum, &pixels->col);
        pixels++;
    }

    chafa_color_div_scalar (&accum, CHAFA_SYMBOL_N_PIXELS);
    *color_out = accum;
}

#ifdef HAVE_MMX_INTRINSICS

#define calc_colors_faster calc_colors_mmx
static void
calc_colors_mmx (const ChafaPixel *pixels, ChafaColor *cols, const guint8 *cov)
{
    __m64 *m64p0 = (__m64 *) pixels;
    __m64 *cols_m64 = (__m64 *) cols;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        __m64 *m64p1;

        m64p1 = cols_m64 + cov [i];
        *m64p1 = _mm_adds_pi16 (*m64p1, m64p0 [i]);
    }

#if 0
    /* Called after outer loop is done */
    _mm_empty ();
#endif
}

#else
# define calc_colors_faster calc_colors_plain
#endif

static void
calc_colors_plain (const ChafaPixel *pixels, ChafaColor *cols, const guint8 *cov)
{
    const gint16 *in_s16 = (const gint16 *) pixels;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        gint16 *out_s16 = (gint16 *) (cols + *cov++);

        *out_s16++ += *in_s16++;
        *out_s16++ += *in_s16++;
        *out_s16++ += *in_s16++;
        *out_s16++ += *in_s16++;
    }
}

static void
eval_symbol_colors (ChafaCanvas *canvas, const ChafaPixel *canvas_pixels,
                    const ChafaSymbol *sym, SymbolEval *eval)
{
    const guint8 *covp = (guint8 *) &sym->coverage [0];
    ChafaColor cols [11] = { 0 };
    gint i;

#if 0
    calc_colors_plain (canvas_pixels, cols, covp);
#else
    calc_colors_faster (canvas_pixels, cols, covp);
#endif

    eval->fg.col = cols [10];
    eval->bg.col = cols [0];

    if (sym->have_mixed)
    {
        for (i = 1; i < 10; i++)
        {
            eval->fg.col.ch [0] += ((gint) cols [i].ch [0] * 100 * (gint) i) / 1000;
            eval->fg.col.ch [1] += ((gint) cols [i].ch [1] * 100 * (gint) i) / 1000;
            eval->fg.col.ch [2] += ((gint) cols [i].ch [2] * 100 * (gint) i) / 1000;
            eval->fg.col.ch [3] += ((gint) cols [i].ch [3] * 100 * (gint) i) / 1000;

            eval->bg.col.ch [0] += ((gint) cols [i].ch [0] * 100 * (gint) (10 - i)) / 1000;
            eval->bg.col.ch [1] += ((gint) cols [i].ch [1] * 100 * (gint) (10 - i)) / 1000;
            eval->bg.col.ch [2] += ((gint) cols [i].ch [2] * 100 * (gint) (10 - i)) / 1000;
            eval->bg.col.ch [3] += ((gint) cols [i].ch [3] * 100 * (gint) (10 - i)) / 1000;
        }
    }

    if (sym->fg_weight > 1)
        chafa_color_div_scalar (&eval->fg.col, (sym->fg_weight + 9) / 10);

    if (sym->bg_weight > 1)
        chafa_color_div_scalar (&eval->bg.col, (sym->bg_weight + 9) / 10);
}

#ifdef HAVE_SSE41_INTRINSICS

# define calc_error_faster calc_error_sse41
static gint
calc_error_sse41 (const ChafaPixel *pixels, const ChafaColor *cols, const guint8 *cov)
{
    __m64 *m64p0 = (__m64 *) pixels;
    __m64 *m64p1 = (__m64 *) cols;
    __m128i err4 = { 0 };
    const gint32 *e = (gint32 *) &err4;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        __m128i t0, t1, t;

        t0 = _mm_cvtepi16_epi32 (_mm_loadl_epi64 ((__m128i *) &m64p0 [i]));
        t1 = _mm_cvtepi16_epi32 (_mm_loadl_epi64 ((__m128i *) &m64p1 [cov [i]]));

        t = t0 - t1;
        t = _mm_mullo_epi32 (t, t);
        err4 += t;
    }

    return e [0] + e [1] + e [2];
}

#else
# define calc_error_faster calc_error_plain
#endif

static gint
calc_error_plain (const ChafaPixel *pixels, const ChafaColor *cols, const guint8 *cov)
{
    gint error = 0;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        guint8 p = *cov++;
        const ChafaPixel *p0 = pixels++;

        error += chafa_color_diff_fast (&cols [p], &p0->col, canvas->color_space);
    }

    return error;
}

static gint
calc_error_with_alpha (const ChafaPixel *pixels, const ChafaColor *cols, const guint8 *cov, ChafaColorSpace cs)
{
    gint error = 0;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        guint8 p = *cov++;
        const ChafaPixel *p0 = pixels++;

        error += chafa_color_diff_slow (&cols [p], &p0->col, cs);
    }

    return error;
}

static void
eval_symbol_error (ChafaCanvas *canvas, const ChafaPixel *canvas_pixels,
                   const ChafaSymbol *sym, SymbolEval *eval)
{
    const guint8 *covp = (guint8 *) &sym->coverage [0];
    ChafaColor cols [11] = { 0 };
    gint error;
    gint i;

    cols [0] = eval->bg.col;
    cols [10] = eval->fg.col;

    if (sym->have_mixed)
    {
        for (i = 1; i < 10; i++)
        {
            chafa_color_mix (&cols [i], &eval->fg.col, &eval->bg.col, i * 1000);
        }
    }

    if (canvas->have_alpha)
    {
        error = calc_error_with_alpha (canvas_pixels, cols, covp, canvas->color_space);
    }
    else
    {
#if 0
        error = calc_error_plain (canvas_pixels, cols, covp);
#else
        error = calc_error_faster (canvas_pixels, cols, covp);
#endif
    }

    eval->error = error;
}

static void
pick_symbol_and_colors (ChafaCanvas *canvas, gint cx, gint cy,
                        gunichar *sym_out,
                        ChafaColor *fg_col_out,
                        ChafaColor *bg_col_out,
                        gint *error_out)
{
    ChafaPixel canvas_pixels [CHAFA_SYMBOL_N_PIXELS];
    SymbolEval eval [SYMBOLS_MAX] = { 0 };
    gint n;
    gint i;

    fetch_canvas_pixel_block (canvas, cx, cy, canvas_pixels);

    for (i = 0; chafa_symbols [i].c != 0; i++)
    {
        eval [i].error = G_MAXINT;

        /* Always evaluate space so we get fallback colors */
        if (chafa_symbols [i].sc != CHAFA_SYMBOL_CLASS_SPACE &&
            (!(chafa_symbols [i].sc & canvas->include_symbols)
             || (chafa_symbols [i].sc & canvas->exclude_symbols)))
            continue;

        if (canvas->mode == CHAFA_CANVAS_MODE_SHAPES_WHITE_ON_BLACK)
        {
            eval [i].fg.col = *chafa_get_palette_color_256 (CHAFA_PALETTE_INDEX_WHITE, canvas->mode);
            eval [i].bg.col = *chafa_get_palette_color_256 (CHAFA_PALETTE_INDEX_BLACK, canvas->mode);
        }
        else if (canvas->mode == CHAFA_CANVAS_MODE_SHAPES_BLACK_ON_WHITE)
        {
            eval [i].fg.col = *chafa_get_palette_color_256 (CHAFA_PALETTE_INDEX_BLACK, canvas->mode);
            eval [i].bg.col = *chafa_get_palette_color_256 (CHAFA_PALETTE_INDEX_WHITE, canvas->mode);
        }
        else
        {
            ChafaColor fg_col, bg_col;

            eval_symbol_colors (canvas, canvas_pixels, &chafa_symbols [i], &eval [i]);

            /* Threshold alpha */

            if (eval [i].fg.col.ch [3] < canvas->alpha_threshold)
                eval [i].fg.col.ch [3] = 0x00;
            else
                eval [i].fg.col.ch [3] = 0xff;

            if (eval [i].bg.col.ch [3] < canvas->alpha_threshold)
                eval [i].bg.col.ch [3] = 0x00;
            else
                eval [i].bg.col.ch [3] = 0xff;

            fg_col = eval [i].fg.col;
            bg_col = eval [i].bg.col;

            /* Pick palette colors before error evaluation; this improves
             * fine detail fidelity slightly. */

            if (canvas->mode == CHAFA_CANVAS_MODE_INDEXED_16)
            {
                if (canvas->quality >= 5)
                {
                    fg_col = *chafa_get_palette_color_256 (chafa_pick_color_16 (&eval [i].fg.col,
                                                                                canvas->color_space), canvas->color_space);
                    bg_col = *chafa_get_palette_color_256 (chafa_pick_color_16 (&eval [i].bg.col,
                                                                                canvas->color_space), canvas->color_space);
                }
            }
            else if (canvas->mode == CHAFA_CANVAS_MODE_INDEXED_240)
            {
                if (canvas->quality >= 8)
                {
                    fg_col = *chafa_get_palette_color_256 (chafa_pick_color_240 (&eval [i].fg.col,
                                                                                 canvas->color_space), canvas->color_space);
                    bg_col = *chafa_get_palette_color_256 (chafa_pick_color_240 (&eval [i].bg.col,
                                                                                 canvas->color_space), canvas->color_space);
                }
            }
            else if (canvas->mode == CHAFA_CANVAS_MODE_INDEXED_256)
            {
                if (canvas->quality >= 8)
                {
                    fg_col = *chafa_get_palette_color_256 (chafa_pick_color_256 (&eval [i].fg.col,
                                                                                 canvas->color_space), canvas->color_space);
                    bg_col = *chafa_get_palette_color_256 (chafa_pick_color_256 (&eval [i].bg.col,
                                                                                 canvas->color_space), canvas->color_space);
                }
            }

            /* FIXME: The logic here seems overly complicated */
            if (canvas->mode != CHAFA_CANVAS_MODE_RGBA)
            {
                /* Transfer mean alpha over so we can use it later */

                fg_col.ch [3] = eval [i].fg.col.ch [3];
                bg_col.ch [3] = eval [i].bg.col.ch [3];

                eval [i].fg.col = fg_col;
                eval [i].bg.col = bg_col;
            }
        }

        eval_symbol_error (canvas, canvas_pixels, &chafa_symbols [i], &eval [i]);
    }

#ifdef HAVE_MMX_INTRINSICS
    /* Make FPU happy again */
    _mm_empty ();
#endif

    if (error_out)
        *error_out = eval [0].error;

    for (i = 0, n = 0; chafa_symbols [i].c != 0; i++)
    {
        if (!(chafa_symbols [i].sc & canvas->include_symbols)
            || (chafa_symbols [i].sc & canvas->exclude_symbols))
            continue;

        if ((eval [i].fg.col.ch [0] != eval [i].bg.col.ch [0]
             || eval [i].fg.col.ch [1] != eval [i].bg.col.ch [1]
             || eval [i].fg.col.ch [2] != eval [i].bg.col.ch [2])
            && eval [i].error < eval [n].error)
        {
            n = i;
            if (error_out)
                *error_out = eval [i].error;
        }
    }

    /* If we couldn't find a symbol and space is excluded by user, try again
     * but allow symbols with equal fg/bg colors. */

    /* FIXME: Ugly duplicate code */

    if (!(chafa_symbols [n].sc & canvas->include_symbols)
        || (chafa_symbols [n].sc & canvas->exclude_symbols))
    {
        for (i = 0, n = -1; chafa_symbols [i].c != 0; i++)
        {
            if (!(chafa_symbols [i].sc & canvas->include_symbols)
                || (chafa_symbols [i].sc & canvas->exclude_symbols))
                continue;

            if (n < 0 || eval [i].error < eval [n].error)
            {
                n = i;
                if (error_out)
                    *error_out = eval [i].error;
            }
        }
    }

    /* Fall back to space */
    if (n < 0)
        n = 0;

    *sym_out = chafa_symbols [n].c;
    *fg_col_out = eval [n].fg.col;
    *bg_col_out = eval [n].bg.col;
}

static void
pick_fill_symbol_16 (ChafaCanvas *canvas, SymbolEval *eval, const ChafaColor *square_col,
                     const gint *sym_coverage, gint fg_col, gint bg_col,
                     SymbolEval *best_eval, gint *best_sym)
{
    gint i;
    gint n;

    for (i = 0; chafa_fill_symbols [i].c != 0; i++)
    {
        const ChafaSymbol *sym = &chafa_fill_symbols [i];
        SymbolEval *e = &eval [i];
        ChafaColor mixed_col;

        if (!(sym->sc & canvas->include_symbols)
            || (sym->sc & canvas->exclude_symbols))
            continue;

        e->fg.col = *chafa_get_palette_color_256 (fg_col, canvas->color_space);
        e->bg.col = *chafa_get_palette_color_256 (bg_col, canvas->color_space);

        chafa_color_mix (&mixed_col, &e->fg.col, &e->bg.col, sym_coverage [i]);

        if (canvas->have_alpha)
            e->error = chafa_color_diff_slow (&mixed_col, square_col, canvas->color_space);
        else
            e->error = chafa_color_diff_fast (&mixed_col, square_col, canvas->color_space);
    }

    for (i = 0, n = -1; chafa_fill_symbols [i].c != 0; i++)
    {
        const ChafaSymbol *sym = &chafa_fill_symbols [i];

        if (!(sym->sc & canvas->include_symbols)
            || (sym->sc & canvas->exclude_symbols))
            continue;

        if (n < 0 || eval [i].error < eval [n].error)
        {
            n = i;
        }
    }

    if (n >= 0 && eval [n].error < best_eval->error)
    {
        *best_eval = eval [n];
        *best_sym = n;
    }
}

static gboolean
pick_fill_16 (ChafaCanvas *canvas, gint cx, gint cy,
              gunichar *sym_out, ChafaColor *fg_col_out, ChafaColor *bg_col_out, gint *error_out)
{
    ChafaPixel canvas_pixels [CHAFA_SYMBOL_N_PIXELS];
    SymbolEval eval [SYMBOLS_MAX] = { 0 };
    SymbolEval best_eval = { 0 };
    gint fg_col, bg_col;
    gint best_sym = -1;
    ChafaColor square_col;
    gint sym_coverage [SYMBOLS_MAX] = { 0 };
    gint i, j;

    best_eval.error = G_MAXINT;

    fetch_canvas_pixel_block (canvas, cx, cy, canvas_pixels);
    calc_mean_color (canvas, canvas_pixels, &square_col);

    for (i = 0; chafa_fill_symbols [i].c != 0; i++)
    {
        const ChafaSymbol *sym = &chafa_fill_symbols [i];
        sym_coverage [i] = (sym->fg_weight * (gint) 100) / CHAFA_SYMBOL_N_PIXELS;
    }

    for (fg_col = 0; fg_col < 16; fg_col++)
    {
        for (bg_col = 0; bg_col < 16; bg_col++)
        {
            pick_fill_symbol_16 (canvas, eval, &square_col, sym_coverage, fg_col, bg_col,
                                 &best_eval, &best_sym);
        }

        pick_fill_symbol_16 (canvas, eval, &square_col, sym_coverage, fg_col, CHAFA_PALETTE_INDEX_TRANSPARENT,
                             &best_eval, &best_sym);
    }

    for (bg_col = 0; bg_col < 16; bg_col++)
    {
        pick_fill_symbol_16 (canvas, eval, &square_col, sym_coverage, CHAFA_PALETTE_INDEX_TRANSPARENT, bg_col,
                             &best_eval, &best_sym);
    }

    if (best_sym < 0)
        return FALSE;

    *sym_out = chafa_fill_symbols [best_sym].c;
    *fg_col_out = best_eval.fg.col;
    *bg_col_out = best_eval.bg.col;

    if (error_out)
        *error_out = best_eval.error;

    return TRUE;
}

static void
update_cells (ChafaCanvas *canvas)
{
    gint cx, cy;
    gint i = 0;

    for (cy = 0; cy < canvas->height; cy++)
    {
        for (cx = 0; cx < canvas->width; cx++, i++)
        {
            ChafaCanvasCell *cell = &canvas->cells [i];
            gunichar sym;
            ChafaColor fg_col, bg_col;

            pick_symbol_and_colors (canvas, cx, cy, &sym, &fg_col, &bg_col, NULL);
            cell->c = sym;

            if (canvas->mode == CHAFA_CANVAS_MODE_INDEXED_256)
            {
                cell->fg_color = chafa_pick_color_256 (&fg_col, canvas->color_space);
                cell->bg_color = chafa_pick_color_256 (&bg_col, canvas->color_space);
            }
            else if (canvas->mode == CHAFA_CANVAS_MODE_INDEXED_240)
            {
                cell->fg_color = chafa_pick_color_240 (&fg_col, canvas->color_space);
                cell->bg_color = chafa_pick_color_240 (&bg_col, canvas->color_space);
            }
            else if (canvas->mode == CHAFA_CANVAS_MODE_INDEXED_16)
            {
                cell->fg_color = chafa_pick_color_16 (&fg_col, canvas->color_space);
                cell->bg_color = chafa_pick_color_16 (&bg_col, canvas->color_space);

                /* Apply stipple symbols in solid cells (space or full block) */
                if ((cell->fg_color == cell->bg_color
                     || cell->c == 0x20
                     || cell->c == 0x2588 /* Full block */)
                    && pick_fill_16 (canvas, cx, cy, &sym, &fg_col, &bg_col, NULL))
                {
                    cell->c = sym;
                    cell->fg_color = chafa_pick_color_16 (&fg_col, canvas->color_space);
                    cell->bg_color = chafa_pick_color_16 (&bg_col, canvas->color_space);
                }
            }
            else if (canvas->mode == CHAFA_CANVAS_MODE_INDEXED_WHITE_ON_BLACK)
            {
                cell->fg_color = chafa_pick_color_2 (&fg_col, canvas->color_space);
                cell->bg_color = chafa_pick_color_2 (&bg_col, canvas->color_space);
            }
            else if (canvas->mode == CHAFA_CANVAS_MODE_INDEXED_BLACK_ON_WHITE)
            {
                cell->fg_color = chafa_pick_color_2 (&fg_col, canvas->color_space);
                cell->bg_color = chafa_pick_color_2 (&bg_col, canvas->color_space);

                /* Invert */

                cell->fg_color = (cell->fg_color == CHAFA_PALETTE_INDEX_BLACK)
                  ? CHAFA_PALETTE_INDEX_WHITE : CHAFA_PALETTE_INDEX_BLACK;

                cell->bg_color = (cell->bg_color == CHAFA_PALETTE_INDEX_BLACK)
                  ? CHAFA_PALETTE_INDEX_WHITE : CHAFA_PALETTE_INDEX_BLACK;
            }
            else
            {
                cell->fg_color = chafa_pack_color (&fg_col);
                cell->bg_color = chafa_pack_color (&bg_col);
            }
        }
    }
}

static void
multiply_alpha (ChafaCanvas *canvas)
{
    ChafaPixel *p0, *p1;

    p0 = canvas->pixels;
    p1 = p0 + canvas->width_pixels * canvas->height_pixels;

    for ( ; p0 < p1; p0++)
    {
        chafa_color_mix (&p0->col, &canvas->alpha_color, &p0->col, 1000 - ((p0->col.ch [3] * 1000) / 255));
    }
}

static void
update_alpha_color (ChafaCanvas *canvas)
{
    ChafaColor col;

    chafa_unpack_color (canvas->alpha_color_packed_rgb, &col);
    if (canvas->color_space == CHAFA_COLOR_SPACE_DIN99D)
        chafa_color_rgb_to_din99d (&col, &canvas->alpha_color);
    else
        canvas->alpha_color = col;
}

static void
rgba_to_internal_rgb (ChafaCanvas *canvas, const guint8 *data, gint width, gint height)
{
    ChafaPixel *pixel;
    gint px, py;
    gint x_inc, y_inc;
    gint alpha_sum = 0;

    x_inc = (width * FIXED_MULT) / (canvas->width_pixels);
    y_inc = (height * FIXED_MULT) / (canvas->height_pixels);

    pixel = canvas->pixels;

    for (py = 0; py < canvas->height_pixels; py++)
    {
        const guint8 *data_row_p;

        data_row_p = data + ((py * y_inc) / FIXED_MULT) * width * 4;

        for (px = 0; px < canvas->width_pixels; px++)
        {
            const guint8 *data_p = data_row_p + ((px * x_inc) / FIXED_MULT) * 4;

            pixel->col.ch [0] = *data_p++;
            pixel->col.ch [1] = *data_p++;
            pixel->col.ch [2] = *data_p++;
            pixel->col.ch [3] = *data_p;

            alpha_sum += (0xff - pixel->col.ch [3]);

            pixel++;
        }
    }

    if (alpha_sum == 0)
    {
        canvas->have_alpha = FALSE;
    }
    else
    {
        canvas->have_alpha = TRUE;
        multiply_alpha (canvas);
    }
}

static void
rgba_to_internal_din99d (ChafaCanvas *canvas, const guint8 *data, gint width, gint height)
{
    ChafaPixel *pixel;
    gint px, py;
    gint x_inc, y_inc;
    gint alpha_sum = 0;

    x_inc = (width * FIXED_MULT) / (canvas->width_pixels);
    y_inc = (height * FIXED_MULT) / (canvas->height_pixels);

    pixel = canvas->pixels;

    for (py = 0; py < canvas->height_pixels; py++)
    {
        const guint8 *data_row_p;

        data_row_p = data + ((py * y_inc) / FIXED_MULT) * width * 4;

        for (px = 0; px < canvas->width_pixels; px++)
        {
            const guint8 *data_p = data_row_p + ((px * x_inc) / FIXED_MULT) * 4;
            ChafaColor rgb_col;

            rgb_col.ch [0] = *data_p++;
            rgb_col.ch [1] = *data_p++;
            rgb_col.ch [2] = *data_p++;
            rgb_col.ch [3] = *data_p;

            alpha_sum += (0xff - rgb_col.ch [3]);

            chafa_color_rgb_to_din99d (&rgb_col, &pixel->col);

            pixel++;
        }
    }

    if (alpha_sum == 0)
    {
        canvas->have_alpha = FALSE;
    }
    else
    {
        canvas->have_alpha = TRUE;
        multiply_alpha (canvas);
    }
}

ChafaCanvas *
chafa_canvas_new (ChafaCanvasMode mode, gint width, gint height)
{
    ChafaCanvas *canvas;

    chafa_init_palette ();
    chafa_init_symbols ();

    canvas = g_new0 (ChafaCanvas, 1);
    canvas->refs = 1;
    canvas->mode = mode;
    canvas->color_space = CHAFA_COLOR_SPACE_RGB;
    canvas->include_symbols = CHAFA_SYMBOLS_ALL;
    canvas->exclude_symbols = CHAFA_SYMBOLS_NONE;
    canvas->width_pixels = width * CHAFA_SYMBOL_WIDTH_PIXELS;
    canvas->height_pixels = height * CHAFA_SYMBOL_HEIGHT_PIXELS;
    canvas->pixels = g_new (ChafaPixel, canvas->width_pixels * canvas->height_pixels);
    canvas->width = width;
    canvas->height = height;
    canvas->cells = g_new (ChafaCanvasCell, width * height);
    canvas->alpha_color_packed_rgb = 0x808080;
    canvas->alpha_threshold = 127;
    canvas->quality = 5;

    update_alpha_color (canvas);

    return canvas;
}

void
chafa_canvas_unref (ChafaCanvas *canvas)
{
    if (--canvas->refs == 0)
    {
        g_free (canvas->pixels);
        g_free (canvas->cells);
        g_free (canvas);
    }
}

void
chafa_canvas_set_color_space (ChafaCanvas *canvas, ChafaColorSpace color_space)
{
    g_return_if_fail (canvas != NULL);

    /* In truecolor mode we don't support any fancy color spaces for now, since
     * we'd have to convert back to RGB space when emitting control codes, and
     * the code for that has yet to be written. In palette modes we just use
     * the palette mappings. */
    if (canvas->mode == CHAFA_CANVAS_MODE_RGBA)
      return;

    canvas->color_space = color_space;
    update_alpha_color (canvas);
}

void
chafa_canvas_set_include_symbols (ChafaCanvas *canvas, guint32 include_symbols)
{
    g_return_if_fail (canvas != NULL);

    canvas->include_symbols = include_symbols;
}

void
chafa_canvas_set_exclude_symbols (ChafaCanvas *canvas, guint32 exclude_symbols)
{
    g_return_if_fail (canvas != NULL);

    canvas->exclude_symbols = exclude_symbols;
}

void
chafa_canvas_set_transparency_threshold (ChafaCanvas *canvas, gfloat alpha_threshold)
{
    g_return_if_fail (canvas != NULL);
    g_return_if_fail (alpha_threshold >= 0.0);
    g_return_if_fail (alpha_threshold <= 1.0);

    /* Invert the scale; internally it's more like an opacity threshold */
    canvas->alpha_threshold = 256.0 * (1.0 - alpha_threshold);
}

void
chafa_canvas_set_transparency_color (ChafaCanvas *canvas, guint32 alpha_color_packed_rgb)
{
    g_return_if_fail (canvas != NULL);

    canvas->alpha_color_packed_rgb = alpha_color_packed_rgb;
    update_alpha_color (canvas);
}

void
chafa_canvas_set_quality (ChafaCanvas *canvas, gint quality)
{
    g_return_if_fail (canvas != NULL);

    canvas->quality = quality;
}

void
chafa_canvas_paint_rgba (ChafaCanvas *canvas, guint8 *pixels, gint width, gint height)
{
    g_return_if_fail (width >= CHAFA_SYMBOL_WIDTH_PIXELS);
    g_return_if_fail (height >= CHAFA_SYMBOL_HEIGHT_PIXELS);

    switch (canvas->color_space)
    {
        case CHAFA_COLOR_SPACE_RGB:
            rgba_to_internal_rgb (canvas, pixels, width, height);
            break;

        case CHAFA_COLOR_SPACE_DIN99D:
            rgba_to_internal_din99d (canvas, pixels, width, height);
            break;
    }

    if (canvas->alpha_threshold == 0)
        canvas->have_alpha = FALSE;

    update_cells (canvas);
}

void
chafa_canvas_print (ChafaCanvas *canvas)
{
    gint x, y;
    gint i = 0;

    for (y = 0; y < canvas->height; y++)
    {
        for (x = 0; x < canvas->width; x++, i++)
        {
            ChafaCanvasCell *cell = &canvas->cells [i];

            if (canvas->mode == CHAFA_CANVAS_MODE_INDEXED_256
                || canvas->mode == CHAFA_CANVAS_MODE_INDEXED_240
                || canvas->mode == CHAFA_CANVAS_MODE_INDEXED_16)
            {
                /* FIXME: Use old school control codes for 16-color palette */

                if (cell->fg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
                {
                    if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
                    {
                        printf ("\x1b[0m ");
                    }
                    else
                    {
                        printf ("\x1b[0m\x1b[7m\x1b[38;5;%dm%lc",
                                cell->bg_color,
                                (wint_t) cell->c);
                    }
                }
                else if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
                {
                    printf ("\x1b[0m\x1b[38;5;%dm%lc",
                            cell->fg_color,
                            (wint_t) cell->c);
                }
                else
                {
                    printf ("\x1b[0m\x1b[38;5;%dm\x1b[48;5;%dm%lc",
                            cell->fg_color,
                            cell->bg_color,
                            (wint_t) cell->c);
                }
            }
            else if (canvas->mode == CHAFA_CANVAS_MODE_INDEXED_WHITE_ON_BLACK
                     || canvas->mode == CHAFA_CANVAS_MODE_INDEXED_BLACK_ON_WHITE)
            {
                printf ("\x1b[%dm%lc",
                        cell->bg_color == CHAFA_PALETTE_INDEX_WHITE ? 7 : 0,
                        cell->fg_color == cell->bg_color ? ' ' : (wint_t) cell->c);
            }
            else if (canvas->mode == CHAFA_CANVAS_MODE_SHAPES_WHITE_ON_BLACK
                     || canvas->mode == CHAFA_CANVAS_MODE_SHAPES_BLACK_ON_WHITE)
            {
                printf ("%lc", (wint_t) cell->c);
            }
            else
            {
                ChafaColor fg, bg;

                chafa_unpack_color (cell->fg_color, &fg);
                chafa_unpack_color (cell->bg_color, &bg);

                if (fg.ch [3] < canvas->alpha_threshold)
                {
                    if (bg.ch [3] < canvas->alpha_threshold)
                    {
                        /* FIXME: Respect include/exclude for space */
                        printf ("\x1b[0m ");
                    }
                    else
                    {
                        printf ("\x1b[0m\x1b[7m\x1b[38;2;%d;%d;%dm%lc",
                                bg.ch [0], bg.ch [1], bg.ch [2],
                                (wint_t) cell->c);
                    }
                }
                else if (bg.ch [3] < canvas->alpha_threshold)
                {
                    printf ("\x1b[0m\x1b[38;2;%d;%d;%dm%lc",
                            fg.ch [0], fg.ch [1], fg.ch [2],
                            (wint_t) cell->c);
                }
                else
                {
                    printf ("\x1b[0m\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm%lc",
                            fg.ch [0], fg.ch [1], fg.ch [2],
                            bg.ch [0], bg.ch [1], bg.ch [2],
                            (wint_t) cell->c);
                }
            }
        }

        printf ("\x1b[0m\n");
    }
}
