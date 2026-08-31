/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2025 Hans Petter Jansson
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

#include "chafa.h"
#include "smolscale/smolscale.h"
#include "internal/chafa-batch.h"
#include "internal/chafa-indexed-image.h"
#include "internal/chafa-math-util.h"
#include "internal/chafa-passthrough-encoder.h"
#include "internal/chafa-sixel-renderer.h"
#include "internal/chafa-string-util.h"

#define SIXEL_CELL_HEIGHT 6

/* Sixel color channels range over 0..100 */
#define SIXEL_CHANNEL_LEVELS 101

typedef struct
{
    ChafaSixelRenderer *sixel_renderer;
    ChafaPassthroughEncoder *ptenc;
}
BuildSixelsCtx;

ChafaSixelRenderer *
chafa_sixel_renderer_new (gint width, gint height,
                        ChafaColorSpace color_space,
                        const ChafaPalette *palette,
                        const ChafaDither *dither)
{
    ChafaSixelRenderer *sixel_renderer;

    sixel_renderer = g_new (ChafaSixelRenderer, 1);
    sixel_renderer->width = width;
    sixel_renderer->height = height;
    sixel_renderer->color_space = color_space;
    sixel_renderer->image = chafa_indexed_image_new (width, chafa_round_up_to_multiple_of (height, SIXEL_CELL_HEIGHT),
                                                     palette, dither);

    if (!sixel_renderer->image)
    {
        g_free (sixel_renderer);
        sixel_renderer = NULL;
    }
    else
    {
        chafa_palette_set_channel_levels (&sixel_renderer->image->palette, SIXEL_CHANNEL_LEVELS);
    }

    return sixel_renderer;
}

void
chafa_sixel_renderer_destroy (ChafaSixelRenderer *sixel_renderer)
{
    chafa_indexed_image_destroy (sixel_renderer->image);
    g_free (sixel_renderer);
}

void
chafa_sixel_renderer_draw_all_pixels (ChafaSixelRenderer *sixel_renderer, ChafaPixelType src_pixel_type,
                                    gconstpointer src_pixels,
                                    gint src_width, gint src_height, gint src_rowstride,
                                    ChafaAlign halign, ChafaAlign valign,
                                    ChafaTuck tuck,
                                    gfloat quality)
{
    g_return_if_fail (sixel_renderer != NULL);
    g_return_if_fail (src_pixel_type < CHAFA_PIXEL_MAX);
    g_return_if_fail (src_pixels != NULL);
    g_return_if_fail (src_width >= 0);
    g_return_if_fail (src_height >= 0);

    if (src_width == 0 || src_height == 0)
        return;

    chafa_indexed_image_draw_pixels (sixel_renderer->image,
                                     sixel_renderer->color_space,
                                     src_pixel_type,
                                     src_pixels,
                                     src_width, src_height, src_rowstride,
                                     sixel_renderer->width, sixel_renderer->height,
                                     halign, valign,
                                     tuck,
                                     quality);
}

/* Columns are processed in banks. For each pen, a sixel row keeps the list
 * of banks that contain at least one pixel of that pen, in increasing
 * order, so the pen's scan visits only those and turns the gaps into runs
 * of empty sixels. */
#define BANK_WIDTH 64

typedef struct
{
    gint n_banks;
    guint16 *bank_lists;  /* [CHAFA_PALETTE_INDEX_MAX] [n_banks] */
    guint16 n_pen_banks [CHAFA_PALETTE_INDEX_MAX];

    /* The sixel patterns of every bank-pen, one byte per column, filled
     * in by scattering each pixel once. Only the strips of pens present in a
     * bank are ever touched. */
    guint8 *pattern_strips;  /* [n_banks] [CHAFA_PALETTE_INDEX_MAX] [BANK_WIDTH] */
}
SixelRow;

static void
sixel_row_init (SixelRow *srow, gint width)
{
    srow->n_banks = (width + BANK_WIDTH - 1) / BANK_WIDTH;
    srow->bank_lists = g_malloc ((gsize) CHAFA_PALETTE_INDEX_MAX * srow->n_banks * sizeof (guint16));
    srow->pattern_strips = g_malloc ((gsize) srow->n_banks * CHAFA_PALETTE_INDEX_MAX * BANK_WIDTH);
}

static void
sixel_row_deinit (SixelRow *srow)
{
    g_free (srow->pattern_strips);
    g_free (srow->bank_lists);
}

static inline guint8 *
pattern_strip (const SixelRow *srow, gint bank, gint pen)
{
    return srow->pattern_strips + ((gsize) bank * CHAFA_PALETTE_INDEX_MAX + pen) * BANK_WIDTH;
}

/* Mask of the columns whose pattern byte differs from the previous one,
 * column 0 being compared against prev_bits. Eight columns per 64-bit
 * word. */
static guint64
pattern_strip_changes (const guint8 *strip, gint n_cols, guint8 prev_bits)
{
    const guint64 lo7 = 0x7f7f7f7f7f7f7f7fULL;
    const guint64 hi = 0x8080808080808080ULL;
    guint64 change_mask = 0;
    gint x;

    for (x = 0; x < n_cols; x += 8)
    {
        guint64 w, prev, diff, changed;

        memcpy (&w, strip + x, 8);  /* The row is always 64 bytes */
        w = GUINT64_FROM_LE (w);

        prev = (w << 8) | prev_bits;
        diff = w ^ prev;
        changed = (((diff & lo7) + lo7) | diff) & hi;  /* 0x80 in changed bytes */

        /* Gather the eight byte flags into eight bits */
        change_mask |= (((changed >> 7) * 0x0102040810204080ULL) >> 56) << x;

        prev_bits = w >> 56;
    }

    if (n_cols < 64)
        change_mask &= ((guint64) 1 << n_cols) - 1;

    return change_mask;
}

/* Build the per-pen bank lists and the sixel pattern rows of every pen present
 * in each bank. */
static void
fetch_sixel_row (SixelRow *srow, const guint8 *pixels, gint width)
{
    gint bank, x, y;

    memset (srow->n_pen_banks, 0, sizeof (srow->n_pen_banks));

    for (bank = 0, x = 0; x < width; x += BANK_WIDTH, bank++)
    {
        guint8 seen [CHAFA_PALETTE_INDEX_MAX] = { 0 };
        gint n = MIN (BANK_WIDTH, width - x);

        for (y = 0; y < SIXEL_CELL_HEIGHT; y++)
        {
            const guint8 *p = pixels + (gsize) y * width + x;
            gint col;

            for (col = 0; col < n; col++)
            {
                guint8 pen = p [col];
                guint8 *row = pattern_strip (srow, bank, pen);

                if (!seen [pen])
                {
                    seen [pen] = 1;
                    memset (row, 0, BANK_WIDTH);
                    srow->bank_lists [pen * srow->n_banks + srow->n_pen_banks [pen]++] = bank;
                }

                row [col] |= 1 << y;
            }
        }
    }
}

static gchar *
format_schar_reps (gchar rep_schar, gint n_reps, gchar *p)
{
    g_assert (n_reps > 0);

    for (;;)
    {
        if (n_reps < 4)
        {
            do *(p++) = rep_schar;
            while (--n_reps);

            goto out;
        }
        else if (n_reps < 255)
        {
            *(p++) = '!';
            p = chafa_format_dec_u8 (p, n_reps);
            *(p++) = rep_schar;
            goto out;
        }
        else
        {
            strcpy (p, "!255");
            p += 4;
            *(p++) = rep_schar;
            n_reps -= 255;

            if (n_reps == 0)
                goto out;
        }
    }

out:
    return p;
}

static inline gchar *
format_pen (guint8 pen, gchar *p)
{
    *(p++) = '#';
    return chafa_format_dec_u8 (p, pen);
}

/* Context for scanning one pen */
typedef struct
{
    gchar *p;
    gint pen;
    gchar rep_schar;  /* Character of the run in progress, or 0 for none */
    gint n_reps;
    gboolean need_pen;
    gboolean need_cr;
    gboolean need_cr_next;
}
PenScan;

static inline void
flush_run (PenScan *ps)
{
    if (ps->need_cr)
    {
        *(ps->p++) = '$';
        ps->need_cr = FALSE;
    }
    if (ps->need_pen)
    {
        ps->p = format_pen (ps->pen, ps->p);
        ps->need_pen = FALSE;
    }

    if (ps->n_reps <= 3)
    {
        gchar *p = ps->p;

        /* Write three chars unconditionally and advance by the actual count */
        p [0] = ps->rep_schar;
        p [1] = ps->rep_schar;
        p [2] = ps->rep_schar;
        ps->p = p + ps->n_reps;
    }
    else if (ps->n_reps < 255)
    {
        /* "!<count><char>", the count from a table */
        gchar *p = ps->p;

        *(p++) = '!';
        p = chafa_format_dec_u8 (p, ps->n_reps);
        *(p++) = ps->rep_schar;
        ps->p = p;
    }
    else
    {
        ps->p = format_schar_reps (ps->rep_schar, ps->n_reps, ps->p);
    }

    ps->need_cr_next = TRUE;
}

/* Account for n_cols columns without this pen */
static inline void
skip_columns (PenScan *ps, gint n_cols)
{
    if (ps->rep_schar != '?' && ps->rep_schar != 0)
    {
        flush_run (ps);
        ps->n_reps = 0;
    }

    ps->rep_schar = '?';
    ps->n_reps += n_cols;
}

/* force_full_width is a workaround for a bug in mlterm; we need to
 * draw the entire first row even if the rightmost pixels are transparent,
 * otherwise the first row with non-transparent pixels will have
 * garbage rendered in it */
static gchar *
build_sixel_row_ansi (const ChafaSixelRenderer *scanvas, const SixelRow *srow,
                      gchar *p, gboolean force_full_width)
{
    PenScan ps;
    gint width = scanvas->width;
    gint pen = 0;

    ps.p = p;
    ps.need_cr = FALSE;
    ps.need_cr_next = FALSE;

    do
    {
        const guint16 *banks = srow->bank_lists + pen * srow->n_banks;
        gint n_pen_banks = srow->n_pen_banks [pen];
        gint next_bank = 0;
        gint k;

        if (pen == chafa_palette_get_transparent_index (&scanvas->image->palette))
            continue;

        ps.pen = pen;
        ps.rep_schar = 0;
        ps.n_reps = 0;
        ps.need_pen = TRUE;

        for (k = 0; k < n_pen_banks; k++)
        {
            gint bank = banks [k];
            gint i = bank * BANK_WIDTH;
            gint step = MIN (BANK_WIDTH, width - i);
            const guint8 *strip = pattern_strip (srow, bank, pen);
            guint64 changes;
            gint cur = 0;

            /* Banks before this one have no pixels of this pen */
            if (bank > next_bank)
                skip_columns (&ps, (bank - next_bank) * BANK_WIDTH);
            next_bank = bank + 1;

            /* Visit only the columns where the pattern changes */
            changes = pattern_strip_changes (strip, step, ps.rep_schar ? ps.rep_schar - '?' : 0xff);

            while (changes)
            {
                gint b = chafa_count_trailing_zeros_u64 (changes);

                changes &= changes - 1;
                ps.n_reps += b - cur;
                cur = b;

                if (ps.rep_schar != 0)
                    flush_run (&ps);

                ps.rep_schar = '?' + strip [b];
                ps.n_reps = 0;
            }

            ps.n_reps += step - cur;
        }

        /* Trailing banks without this pen */
        if (next_bank < srow->n_banks)
            skip_columns (&ps, width - next_bank * BANK_WIDTH);

        if (ps.rep_schar != '?' || force_full_width)
        {
            flush_run (&ps);

            /* Only need to do this for a single pen */
            force_full_width = FALSE;
        }

        ps.need_cr = ps.need_cr_next;
    }
    while (++pen < chafa_palette_get_n_colors (&scanvas->image->palette));

    return ps.p;
}

static void
build_sixel_row_worker (ChafaBatchInfo *batch, const BuildSixelsCtx *ctx)
{
    SixelRow srow;
    gchar *sixel_ansi, *p;
    gint n_sixel_rows;
    gint i;

    n_sixel_rows = (batch->n_rows + SIXEL_CELL_HEIGHT - 1) / SIXEL_CELL_HEIGHT;
    sixel_row_init (&srow, ctx->sixel_renderer->width);

    /* +16 = terminator and slack for flush_run()'s unconditional short-run stores */
    sixel_ansi = p = g_malloc (256 * ((gsize) ctx->sixel_renderer->width + 5) * n_sixel_rows + 16);

    for (i = 0; i < n_sixel_rows; i++)
    {
        gboolean is_global_first_row = batch->first_row + i == 0;
        gboolean is_global_last_row = batch->first_row + (i + 1) * SIXEL_CELL_HEIGHT >= ctx->sixel_renderer->height;
        const guint8 *pixels = ctx->sixel_renderer->image->pixels
            + (gsize) ctx->sixel_renderer->image->width * (batch->first_row + i * SIXEL_CELL_HEIGHT);

        fetch_sixel_row (&srow, pixels, ctx->sixel_renderer->image->width);
        p = build_sixel_row_ansi (ctx->sixel_renderer, &srow, p,
                                  (is_global_first_row) || (is_global_last_row)
                                  ? TRUE : FALSE);

        /* GNL after every row except final */
        if (!is_global_last_row)
            *(p++) = '-';
    }

    batch->ret_p = sixel_ansi;
    batch->ret_n = p - sixel_ansi;

    sixel_row_deinit (&srow);
}

static void
build_sixel_row_post (ChafaBatchInfo *batch, BuildSixelsCtx *ctx)
{
    chafa_passthrough_encoder_append_len (ctx->ptenc, batch->ret_p, batch->ret_n);
    g_free (batch->ret_p);
}

static void
build_sixel_palette (ChafaSixelRenderer *sixel_renderer, ChafaPassthroughEncoder *ptenc)
{
    gchar str [256 * 20 + 1];
    gchar *p = str;
    gint first_color;
    gint pen;

    first_color = chafa_palette_get_first_color (&sixel_renderer->image->palette);

    for (pen = 0; pen < chafa_palette_get_n_colors (&sixel_renderer->image->palette); pen++)
    {
        const ChafaColor *col;

        if (pen == chafa_palette_get_transparent_index (&sixel_renderer->image->palette))
            continue;

        col = chafa_palette_get_color (&sixel_renderer->image->palette, CHAFA_COLOR_SPACE_RGB,
                                       first_color + pen);
        *(p++) = '#';
        p = chafa_format_dec_u8 (p, pen);
        *(p++) = ';';
        *(p++) = '2';  /* Color space: RGB */
        *(p++) = ';';

        p = chafa_format_dec_u8 (p, chafa_palette_channel_to_level (
            &sixel_renderer->image->palette, col->ch [0]));
        *(p++) = ';';
        p = chafa_format_dec_u8 (p, chafa_palette_channel_to_level (
            &sixel_renderer->image->palette, col->ch [1]));
        *(p++) = ';';
        p = chafa_format_dec_u8 (p, chafa_palette_channel_to_level (
            &sixel_renderer->image->palette, col->ch [2]));
    }

    chafa_passthrough_encoder_append_len (ptenc, str, p - str);
}

static void
end_sixels (ChafaPassthroughEncoder *ptenc, ChafaTermInfo *term_info)
{
    gchar buf [CHAFA_TERM_SEQ_LENGTH_MAX + 1];
    gint i;

    *chafa_term_info_emit_end_sixels (term_info, buf) = '\0';

    if (ptenc->mode == CHAFA_PASSTHROUGH_SCREEN)
    {
        /* In GNU Screen, the end of an emitted sixel passthrough sequence should
         * look something like this: \e P \e \e \\ \e P \\ \e \\ */

        for (i = 0; buf [i]; i++)
        {
            chafa_passthrough_encoder_flush (ptenc);
            chafa_passthrough_encoder_append_len (ptenc, buf + i, 1);
        }
    }
    else
    {
        chafa_passthrough_encoder_append (ptenc, buf);
    }

    chafa_passthrough_encoder_flush (ptenc);
}

void
chafa_sixel_renderer_build_ansi (ChafaSixelRenderer *sixel_renderer, ChafaTermInfo *term_info,
                                 GString *str, ChafaPassthrough passthrough)
{
    ChafaPassthroughEncoder ptenc;
    BuildSixelsCtx ctx;
    gchar buf [CHAFA_TERM_SEQ_LENGTH_MAX + 1];

    g_assert (sixel_renderer->image->height % SIXEL_CELL_HEIGHT == 0);

    chafa_passthrough_encoder_begin (&ptenc, passthrough, term_info, str);

    *chafa_term_info_emit_begin_sixels (term_info, buf, 0, 1, 0) = '\0';
    chafa_passthrough_encoder_append (&ptenc, buf);

    g_snprintf (buf,
                CHAFA_TERM_SEQ_LENGTH_MAX,
                "\"1;1;%d;%d",
                sixel_renderer->width,
                sixel_renderer->height);
    chafa_passthrough_encoder_append (&ptenc, buf);

    ctx.sixel_renderer = sixel_renderer;
    ctx.ptenc = &ptenc;

    build_sixel_palette (sixel_renderer, &ptenc);

    chafa_process_batches (&ctx,
                           (GFunc) build_sixel_row_worker,
                           (GFunc) build_sixel_row_post,
                           sixel_renderer->image->height,
                           chafa_get_n_actual_threads (),
                           SIXEL_CELL_HEIGHT);

    end_sixels (&ptenc, term_info);
    chafa_passthrough_encoder_end (&ptenc);
}
