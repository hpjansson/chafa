/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020-2021 Hans Petter Jansson
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

#include "chafa.h"
#include "internal/chafa-canvas-printer.h"

typedef struct
{
    ChafaCanvas *canvas;
    ChafaTermInfo *term_info;

    gunichar cur_char;
    gint n_reps;
    guint cur_inverted : 1;
    guint32 cur_fg;
    guint32 cur_bg;

    /* For direct-color mode */
    ChafaColor cur_fg_direct;
    ChafaColor cur_bg_direct;
}
PrintCtx;

static gint
cmp_colors (ChafaColor a, ChafaColor b)
{
    return memcmp (&a, &b, sizeof (ChafaColor));
}

static ChafaColor
threshold_alpha (ChafaColor col, gint alpha_threshold)
{
    col.ch [3] = col.ch [3] < alpha_threshold ? 0 : 255;
    return col;
}

G_GNUC_WARN_UNUSED_RESULT static gchar *
flush_chars (PrintCtx *ctx, gchar *out)
{
    gchar buf [8];
    gint len;

    if (!ctx->cur_char)
        return out;

    len = g_unichar_to_utf8 (ctx->cur_char, buf);

    if ((ctx->canvas->config.optimizations & CHAFA_OPTIMIZATION_REPEAT_CELLS)
        && chafa_term_info_have_seq (ctx->term_info, CHAFA_TERM_SEQ_REPEAT_CHAR)
        && ctx->n_reps > 1
        && ctx->n_reps * len > len + 4 /* ESC [#b */)
    {
        memcpy (out, buf, len);
        out += len;

        out = chafa_term_info_emit_repeat_char (ctx->term_info, out, ctx->n_reps - 1);

        ctx->n_reps = 0;
    }
    else
    {
        do
        {
            memcpy (out, buf, len);
            out += len;
            ctx->n_reps--;
        }
        while (ctx->n_reps != 0);
    }

    ctx->cur_char = 0;
    return out;
}

G_GNUC_WARN_UNUSED_RESULT static gchar *
queue_char (PrintCtx *ctx, gchar *out, gunichar c)
{
    if (ctx->cur_char == c)
    {
        ctx->n_reps++;
    }
    else
    {
        if (ctx->cur_char)
            out = flush_chars (ctx, out);

        ctx->cur_char = c;
        ctx->n_reps = 1;
    }

    return out;
}

G_GNUC_WARN_UNUSED_RESULT static gchar *
reset_attributes (PrintCtx *ctx, gchar *out)
{
    out = chafa_term_info_emit_reset_attributes (ctx->term_info, out);

    ctx->cur_inverted = FALSE;
    ctx->cur_fg = CHAFA_PALETTE_INDEX_TRANSPARENT;
    ctx->cur_bg = CHAFA_PALETTE_INDEX_TRANSPARENT;
    ctx->cur_fg_direct.ch [3] = 0;
    ctx->cur_bg_direct.ch [3] = 0;

    return out;
}

G_GNUC_WARN_UNUSED_RESULT static gchar *
emit_attributes_truecolor (PrintCtx *ctx, gchar *out,
                           ChafaColor fg, ChafaColor bg, gboolean inverted)
{
    if (ctx->canvas->config.optimizations & CHAFA_OPTIMIZATION_REUSE_ATTRIBUTES)
    {
        if (!ctx->canvas->config.fg_only_enabled
            && ((ctx->cur_inverted && !inverted)
                || (ctx->cur_fg_direct.ch [3] != 0 && fg.ch [3] == 0)
                || (ctx->cur_bg_direct.ch [3] != 0 && bg.ch [3] == 0)))
        {
            out = flush_chars (ctx, out);
            out = reset_attributes (ctx, out);
        }

        if (!ctx->cur_inverted && inverted)
        {
            out = flush_chars (ctx, out);
            out = chafa_term_info_emit_invert_colors (ctx->term_info, out);
        }

        if (cmp_colors (fg, ctx->cur_fg_direct))
        {
            if (cmp_colors (bg, ctx->cur_bg_direct) && bg.ch [3] != 0)
            {
                out = flush_chars (ctx, out);
                out = chafa_term_info_emit_set_color_fgbg_direct (ctx->term_info, out,
                                                                  fg.ch [0], fg.ch [1], fg.ch [2],
                                                                  bg.ch [0], bg.ch [1], bg.ch [2]);
            }
            else if (fg.ch [3] != 0)
            {
                out = flush_chars (ctx, out);
                out = chafa_term_info_emit_set_color_fg_direct (ctx->term_info, out,
                                                                fg.ch [0], fg.ch [1], fg.ch [2]);
            }
        }
        else if (cmp_colors (bg, ctx->cur_bg_direct) && bg.ch [3] != 0)
        {
            out = flush_chars (ctx, out);
            out = chafa_term_info_emit_set_color_bg_direct (ctx->term_info, out,
                                                            bg.ch [0], bg.ch [1], bg.ch [2]);
        }
    }
    else
    {
        out = flush_chars (ctx, out);
        out = reset_attributes (ctx, out);
        if (inverted)
            out = chafa_term_info_emit_invert_colors (ctx->term_info, out);

        if (fg.ch [3] != 0)
        {
            if (bg.ch [3] != 0)
            {
                out = chafa_term_info_emit_set_color_fgbg_direct (ctx->term_info, out,
                                                                  fg.ch [0], fg.ch [1], fg.ch [2],
                                                                  bg.ch [0], bg.ch [1], bg.ch [2]);
            }
            else
            {
                out = chafa_term_info_emit_set_color_fg_direct (ctx->term_info, out,
                                                                fg.ch [0], fg.ch [1], fg.ch [2]);
            }
        }
        else if (bg.ch [3] != 0)
        {
            out = chafa_term_info_emit_set_color_bg_direct (ctx->term_info, out,
                                                            bg.ch [0], bg.ch [1], bg.ch [2]);
        }
    }

    ctx->cur_fg_direct = fg;
    ctx->cur_bg_direct = bg;
    ctx->cur_inverted = inverted;
    return out;
}

G_GNUC_WARN_UNUSED_RESULT static gchar *
emit_ansi_truecolor (PrintCtx *ctx, gchar *out, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &ctx->canvas->cells [i];
        ChafaColor fg, bg;

        /* Wide symbols have a zero code point in the rightmost cell */
        if (cell->c == 0)
            continue;

        chafa_unpack_color (cell->fg_color, &fg);
        fg = threshold_alpha (fg, ctx->canvas->config.alpha_threshold);
        chafa_unpack_color (cell->bg_color, &bg);
        bg = threshold_alpha (bg, ctx->canvas->config.alpha_threshold);

        if (fg.ch [3] == 0 && bg.ch [3] != 0)
            out = emit_attributes_truecolor (ctx, out, bg, fg, TRUE);
        else
            out = emit_attributes_truecolor (ctx, out, fg, bg, FALSE);

        if (fg.ch [3] == 0 && bg.ch [3] == 0)
        {
            out = queue_char (ctx, out, ' ');
            if (i < i_max - 1 && ctx->canvas->cells [i + 1].c == 0)
                out = queue_char (ctx, out, ' ');
        }
        else
        {
            out = queue_char (ctx, out, cell->c);
        }
    }

    return out;
}

G_GNUC_WARN_UNUSED_RESULT static gchar *
handle_inverted_with_reuse (PrintCtx *ctx, gchar *out,
                            guint32 fg, guint32 bg, gboolean inverted)
{
    /* We must check fg_only_enabled because we can run into the situation where
     * fg is set to transparent. */
    if (!ctx->canvas->config.fg_only_enabled
        && ((ctx->cur_inverted && !inverted)
            || (ctx->cur_fg != CHAFA_PALETTE_INDEX_TRANSPARENT && fg == CHAFA_PALETTE_INDEX_TRANSPARENT)
            || (ctx->cur_bg != CHAFA_PALETTE_INDEX_TRANSPARENT && bg == CHAFA_PALETTE_INDEX_TRANSPARENT)))
    {
        out = flush_chars (ctx, out);
        out = reset_attributes (ctx, out);
    }

    if (!ctx->cur_inverted && inverted)
    {
        out = flush_chars (ctx, out);
        out = chafa_term_info_emit_invert_colors (ctx->term_info, out);
    }

    return out;
}

G_GNUC_WARN_UNUSED_RESULT static gchar *
emit_attributes_256 (PrintCtx *ctx, gchar *out,
                     guint32 fg, guint32 bg, gboolean inverted)
{
    if (ctx->canvas->config.optimizations & CHAFA_OPTIMIZATION_REUSE_ATTRIBUTES)
    {
        out = handle_inverted_with_reuse (ctx, out, fg, bg, inverted);

        if (fg != ctx->cur_fg)
        {
            if (bg != ctx->cur_bg && bg != CHAFA_PALETTE_INDEX_TRANSPARENT)
            {
                out = flush_chars (ctx, out);
                out = chafa_term_info_emit_set_color_fgbg_256 (ctx->term_info, out, fg, bg);
            }
            else if (fg != CHAFA_PALETTE_INDEX_TRANSPARENT)
            {
                out = flush_chars (ctx, out);
                out = chafa_term_info_emit_set_color_fg_256 (ctx->term_info, out, fg);
            }
        }
        else if (bg != ctx->cur_bg && bg != CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            out = flush_chars (ctx, out);
            out = chafa_term_info_emit_set_color_bg_256 (ctx->term_info, out, bg);
        }
    }
    else
    {
        out = flush_chars (ctx, out);
        out = reset_attributes (ctx, out);
        if (inverted)
            out = chafa_term_info_emit_invert_colors (ctx->term_info, out);

        if (fg != CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            if (bg != CHAFA_PALETTE_INDEX_TRANSPARENT)
            {
                out = chafa_term_info_emit_set_color_fgbg_256 (ctx->term_info, out, fg, bg);
            }
            else
            {
                out = chafa_term_info_emit_set_color_fg_256 (ctx->term_info, out, fg);
            }
        }
        else if (bg != CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            out = chafa_term_info_emit_set_color_bg_256 (ctx->term_info, out, bg);
        }
    }

    ctx->cur_fg = fg;
    ctx->cur_bg = bg;
    ctx->cur_inverted = inverted;
    return out;
}

G_GNUC_WARN_UNUSED_RESULT static gchar *
emit_ansi_256 (PrintCtx *ctx, gchar *out, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &ctx->canvas->cells [i];
        guint32 fg, bg;

        /* Wide symbols have a zero code point in the rightmost cell */
        if (cell->c == 0)
            continue;

        fg = cell->fg_color;
        bg = cell->bg_color;

        if (fg == CHAFA_PALETTE_INDEX_TRANSPARENT && bg != CHAFA_PALETTE_INDEX_TRANSPARENT)
            out = emit_attributes_256 (ctx, out, bg, fg, TRUE);
        else
            out = emit_attributes_256 (ctx, out, fg, bg, FALSE);

        if (fg == CHAFA_PALETTE_INDEX_TRANSPARENT && bg == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            out = queue_char (ctx, out, ' ');
            if (i < i_max - 1 && ctx->canvas->cells [i + 1].c == 0)
                out = queue_char (ctx, out, ' ');
        }
        else
        {
            out = queue_char (ctx, out, cell->c);
        }
    }

    return out;
}

G_GNUC_WARN_UNUSED_RESULT static gchar *
emit_attributes_16 (PrintCtx *ctx, gchar *out,
                    guint32 fg, guint32 bg, gboolean inverted)
{
    if (ctx->canvas->config.optimizations & CHAFA_OPTIMIZATION_REUSE_ATTRIBUTES)
    {
        out = handle_inverted_with_reuse (ctx, out, fg, bg, inverted);

        if (fg != ctx->cur_fg)
        {
            if (bg != ctx->cur_bg && bg != CHAFA_PALETTE_INDEX_TRANSPARENT)
            {
                out = flush_chars (ctx, out);
                out = chafa_term_info_emit_set_color_fgbg_16 (ctx->term_info, out, fg, bg);
            }
            else if (fg != CHAFA_PALETTE_INDEX_TRANSPARENT)
            {
                out = flush_chars (ctx, out);
                out = chafa_term_info_emit_set_color_fg_16 (ctx->term_info, out, fg);
            }
        }
        else if (bg != ctx->cur_bg && bg != CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            out = flush_chars (ctx, out);
            out = chafa_term_info_emit_set_color_bg_16 (ctx->term_info, out, bg);
        }
    }
    else
    {
        out = flush_chars (ctx, out);
        out = reset_attributes (ctx, out);
        if (inverted)
            out = chafa_term_info_emit_invert_colors (ctx->term_info, out);

        if (fg != CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            if (bg != CHAFA_PALETTE_INDEX_TRANSPARENT)
            {
                out = chafa_term_info_emit_set_color_fgbg_16 (ctx->term_info, out, fg, bg);
            }
            else
            {
                out = chafa_term_info_emit_set_color_fg_16 (ctx->term_info, out, fg);
            }
        }
        else if (bg != CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            out = chafa_term_info_emit_set_color_bg_16 (ctx->term_info, out, bg);
        }
    }

    ctx->cur_fg = fg;
    ctx->cur_bg = bg;
    ctx->cur_inverted = inverted;
    return out;
}

/* Uses aixterm control codes for bright colors */
G_GNUC_WARN_UNUSED_RESULT static gchar *
emit_ansi_16 (PrintCtx *ctx, gchar *out, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &ctx->canvas->cells [i];
        guint32 fg, bg;

        /* Wide symbols have a zero code point in the rightmost cell */
        if (cell->c == 0)
            continue;

        fg = cell->fg_color;
        bg = cell->bg_color;

        if (fg == CHAFA_PALETTE_INDEX_TRANSPARENT && bg != CHAFA_PALETTE_INDEX_TRANSPARENT)
            out = emit_attributes_16 (ctx, out, bg, fg, TRUE);
        else
            out = emit_attributes_16 (ctx, out, fg, bg, FALSE);

        if (fg == CHAFA_PALETTE_INDEX_TRANSPARENT && bg == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            out = queue_char (ctx, out, ' ');
            if (i < i_max - 1 && ctx->canvas->cells [i + 1].c == 0)
                out = queue_char (ctx, out, ' ');
        }
        else
        {
            out = queue_char (ctx, out, cell->c);
        }
    }

    return out;
}

G_GNUC_WARN_UNUSED_RESULT static gchar *
emit_ansi_fgbg_bgfg (PrintCtx *ctx, gchar *out, gint i, gint i_max)
{
    gunichar blank_symbol = 0;

    if (chafa_symbol_map_has_symbol (&ctx->canvas->config.symbol_map, ' '))
    {
        blank_symbol = ' ';
    }
    else if (chafa_symbol_map_has_symbol (&ctx->canvas->config.symbol_map, 0x2588 /* Solid block */ ))
    {
        blank_symbol = 0x2588;
    }

    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &ctx->canvas->cells [i];
        gboolean invert = FALSE;
        gunichar c = cell->c;

        /* Wide symbols have a zero code point in the rightmost cell */
        if (c == 0)
            continue;

        /* Replace with blank symbol only if this is a single-width cell */
        if (cell->fg_color == cell->bg_color && blank_symbol != 0
            && (i == i_max - 1 || (ctx->canvas->cells [i + 1].c != 0)))
        {
            c = blank_symbol;
            if (blank_symbol == 0x2588)
                invert = TRUE;
        }

        if (cell->bg_color == CHAFA_PALETTE_INDEX_FG)
        {
            invert ^= TRUE;
        }

        if (ctx->canvas->config.optimizations & CHAFA_OPTIMIZATION_REUSE_ATTRIBUTES)
        {
            if (!ctx->cur_inverted && invert)
            {
                out = flush_chars (ctx, out);
                out = chafa_term_info_emit_invert_colors (ctx->term_info, out);
            }
            else if (ctx->cur_inverted && !invert)
            {
                out = flush_chars (ctx, out);
                out = reset_attributes (ctx, out);
            }

            ctx->cur_inverted = invert;
        }
        else
        {
            out = flush_chars (ctx, out);
            if (invert)
                out = chafa_term_info_emit_invert_colors (ctx->term_info, out);
            else
                out = reset_attributes (ctx, out);
        }

        out = queue_char (ctx, out, c);
    }

    return out;
}

G_GNUC_WARN_UNUSED_RESULT static gchar *
emit_ansi_fgbg (PrintCtx *ctx, gchar *out, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &ctx->canvas->cells [i];

        /* Wide symbols have a zero code point in the rightmost cell */
        if (cell->c == 0)
            continue;

        out = queue_char (ctx, out, cell->c);
    }

    return out;
}

static void
prealloc_string (GString *gs, gint n_cells)
{
    guint needed_len;

    /* Each cell produces at most three control sequences and six bytes
     * for the UTF-8 character. Each row may add one seq and one newline. */
    needed_len = (n_cells + 1) * (CHAFA_TERM_SEQ_LENGTH_MAX * 3 + 6) + 1;

    if (gs->allocated_len - gs->len < needed_len)
    {
        guint current_len = gs->len;
        g_string_set_size (gs, gs->len + needed_len * 2);
        gs->len = current_len;
    }
}

static GString *
build_ansi_gstring (ChafaCanvas *canvas, ChafaTermInfo *ti)
{
    GString *gs = g_string_new ("");
    PrintCtx ctx = { 0 };
    gint i, i_max, i_step, i_next;

    ctx.canvas = canvas;
    ctx.term_info = ti;

    i = 0;
    i_max = canvas->config.width * canvas->config.height;
    i_step = canvas->config.width;

    for ( ; i < i_max; i = i_next)
    {
        gchar *out;

        i_next = i + i_step;

        prealloc_string (gs, i_step);
        out = gs->str + gs->len;

        /* Avoid control codes in FGBG mode. Don't reset attributes when BG
         * is held, to preserve any BG color set previously. */
        if (i == 0
            && canvas->config.canvas_mode != CHAFA_CANVAS_MODE_FGBG
            && !canvas->config.fg_only_enabled)
        {
            out = reset_attributes (&ctx, out);
        }

        switch (canvas->config.canvas_mode)
        {
            case CHAFA_CANVAS_MODE_TRUECOLOR:
                out = emit_ansi_truecolor (&ctx, out, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_INDEXED_256:
            case CHAFA_CANVAS_MODE_INDEXED_240:
                out = emit_ansi_256 (&ctx, out, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_INDEXED_16:
                out = emit_ansi_16 (&ctx, out, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_INDEXED_8:
                out = emit_ansi_16 (&ctx, out, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_FGBG_BGFG:
                out = emit_ansi_fgbg_bgfg (&ctx, out, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_FGBG:
                out = emit_ansi_fgbg (&ctx, out, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_MAX:
                g_assert_not_reached ();
                break;
        }

        out = flush_chars (&ctx, out);

        /* Avoid control codes in FGBG mode. Don't reset attributes when BG
         * is held, to preserve any BG color set previously. */
        if (canvas->config.canvas_mode != CHAFA_CANVAS_MODE_FGBG
            && !canvas->config.fg_only_enabled)
        {
            out = reset_attributes (&ctx, out);
        }

        /* Last line should not end in newline */
        if (i_next < i_max)
            *(out++) = '\n';

        *out = '\0';
        gs->len = out - gs->str;
    }

    return gs;
}

GString *
chafa_canvas_print_symbols (ChafaCanvas *canvas, ChafaTermInfo *ti)
{
    g_assert (canvas != NULL);
    g_assert (ti != NULL);

    return build_ansi_gstring (canvas, ti);
}
