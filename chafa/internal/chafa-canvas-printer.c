/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020 Hans Petter Jansson
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

static gchar *
emit_ansi_truecolor (ChafaCanvas *canvas, ChafaTermInfo *ti, gchar *out, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];
        ChafaColor fg, bg;

        /* Wide symbols have a zero code point in the rightmost cell */
        if (cell->c == 0)
            continue;

        chafa_unpack_color (cell->fg_color, &fg);
        chafa_unpack_color (cell->bg_color, &bg);

        if (fg.ch [3] < canvas->config.alpha_threshold)
        {
            if (bg.ch [3] < canvas->config.alpha_threshold)
            {
                out = chafa_term_info_emit_reset_attributes (ti, out);
                /* FIXME: Respect include/exclude for space */
                *(out++) = ' ';
                if (i < i_max - 1 && canvas->cells [i + 1].c == 0)
                    *(out++) = ' ';
            }
            else
            {
                out = chafa_term_info_emit_reset_attributes (ti, out);
                out = chafa_term_info_emit_invert_colors (ti, out);
                out = chafa_term_info_emit_set_color_fg_direct (ti, out,
                                                                bg.ch [0], bg.ch [1], bg.ch [2]);
                out += g_unichar_to_utf8 (cell->c, out);
            }
        }
        else if (bg.ch [3] < canvas->config.alpha_threshold)
        {
            out = chafa_term_info_emit_reset_attributes (ti, out);
            out = chafa_term_info_emit_set_color_fg_direct (ti, out,
                                                            fg.ch [0], fg.ch [1], fg.ch [2]);
            out += g_unichar_to_utf8 (cell->c, out);
        }
        else
        {
            out = chafa_term_info_emit_reset_attributes (ti, out);
            out = chafa_term_info_emit_set_color_fg_direct (ti, out,
                                                            fg.ch [0], fg.ch [1], fg.ch [2]);
            out = chafa_term_info_emit_set_color_bg_direct (ti, out,
                                                            bg.ch [0], bg.ch [1], bg.ch [2]);
            out += g_unichar_to_utf8 (cell->c, out);
        }
    }

    return out;
}

static gchar *
emit_ansi_256 (ChafaCanvas *canvas, ChafaTermInfo *ti, gchar *out, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];

        /* Wide symbols have a zero code point in the rightmost cell */
        if (cell->c == 0)
            continue;

        if (cell->fg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
            {
                out = chafa_term_info_emit_reset_attributes (ti, out);
                /* FIXME: Respect include/exclude for space */
                *(out++) = ' ';
                if (i < i_max - 1 && canvas->cells [i + 1].c == 0)
                    *(out++) = ' ';
            }
            else
            {
                out = chafa_term_info_emit_reset_attributes (ti, out);
                out = chafa_term_info_emit_invert_colors (ti, out);
                out = chafa_term_info_emit_set_color_fg_256 (ti, out, cell->bg_color);
                out += g_unichar_to_utf8 (cell->c, out);
            }
        }
        else if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            out = chafa_term_info_emit_reset_attributes (ti, out);
            out = chafa_term_info_emit_set_color_fg_256 (ti, out, cell->fg_color);
            out += g_unichar_to_utf8 (cell->c, out);
        }
        else
        {
            out = chafa_term_info_emit_reset_attributes (ti, out);
            out = chafa_term_info_emit_set_color_fg_256 (ti, out, cell->fg_color);
            out = chafa_term_info_emit_set_color_bg_256 (ti, out, cell->bg_color);
            out += g_unichar_to_utf8 (cell->c, out);
        }
    }

    return out;
}

/* Uses aixterm control codes for bright colors */
static gchar *
emit_ansi_16 (ChafaCanvas *canvas, ChafaTermInfo *ti, gchar *out, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];

        /* Wide symbols have a zero code point in the rightmost cell */
        if (cell->c == 0)
            continue;

        if (cell->fg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
            {
                out = chafa_term_info_emit_reset_attributes (ti, out);
                /* FIXME: Respect include/exclude for space */
                *(out++) = ' ';
                if (i < i_max - 1 && canvas->cells [i + 1].c == 0)
                    *(out++) = ' ';
            }
            else
            {
                out = chafa_term_info_emit_reset_attributes (ti, out);
                out = chafa_term_info_emit_invert_colors (ti, out);
                out = chafa_term_info_emit_set_color_fg_16 (ti, out, cell->bg_color);
                out += g_unichar_to_utf8 (cell->c, out);
            }
        }
        else if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            out = chafa_term_info_emit_reset_attributes (ti, out);
            out = chafa_term_info_emit_set_color_fg_16 (ti, out, cell->fg_color);
            out += g_unichar_to_utf8 (cell->c, out);
        }
        else
        {
            out = chafa_term_info_emit_reset_attributes (ti, out);
            out = chafa_term_info_emit_set_color_fg_256 (ti, out, cell->fg_color);
            out = chafa_term_info_emit_set_color_bg_256 (ti, out, cell->bg_color);
            out += g_unichar_to_utf8 (cell->c, out);
        }
    }

    return out;
}

static gchar *
emit_ansi_fgbg_bgfg (ChafaCanvas *canvas, ChafaTermInfo *ti, gchar *out, gint i, gint i_max)
{
    gunichar blank_symbol = 0;

    if (chafa_symbol_map_has_symbol (&canvas->config.symbol_map, ' '))
    {
        blank_symbol = ' ';
    }
    else if (chafa_symbol_map_has_symbol (&canvas->config.symbol_map, 0x2588 /* Solid block */ ))
    {
        blank_symbol = 0x2588;
    }

    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];
        gboolean invert = FALSE;
        gunichar c = cell->c;

        /* Wide symbols have a zero code point in the rightmost cell */
        if (c == 0)
            continue;

        /* Replace with blank symbol only if this is a single-width cell */
        if (cell->fg_color == cell->bg_color && blank_symbol != 0
            && (i == i_max - 1 || (canvas->cells [i + 1].c != 0)))
        {
            c = blank_symbol;
            if (blank_symbol == 0x2588)
                invert = TRUE;
        }

        if (cell->bg_color == CHAFA_PALETTE_INDEX_FG)
        {
            invert ^= TRUE;
        }

        if (invert)
            out = chafa_term_info_emit_invert_colors (ti, out);
        else
            out = chafa_term_info_emit_reset_attributes (ti, out);

        out += g_unichar_to_utf8 (cell->c, out);
    }

    return out;
}

static gchar *
emit_ansi_fgbg (ChafaCanvas *canvas, G_GNUC_UNUSED ChafaTermInfo *ti, gchar *out, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];

        /* Wide symbols have a zero code point in the rightmost cell */
        if (cell->c == 0)
            continue;

        out += g_unichar_to_utf8 (cell->c, out);
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
    gint i, i_max, i_step, i_next;

    i = 0;
    i_max = canvas->config.width * canvas->config.height;
    i_step = canvas->config.width;

    for ( ; i < i_max; i = i_next)
    {
        gchar *out;

        i_next = i + i_step;

        prealloc_string (gs, i_step);
        out = gs->str + gs->len;

        switch (canvas->config.canvas_mode)
        {
            case CHAFA_CANVAS_MODE_TRUECOLOR:
                out = emit_ansi_truecolor (canvas, ti, out, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_INDEXED_256:
            case CHAFA_CANVAS_MODE_INDEXED_240:
                out = emit_ansi_256 (canvas, ti, out, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_INDEXED_16:
                out = emit_ansi_16 (canvas, ti, out, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_FGBG_BGFG:
                out = emit_ansi_fgbg_bgfg (canvas, ti, out, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_FGBG:
                out = emit_ansi_fgbg (canvas, ti, out, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_MAX:
                g_assert_not_reached ();
                break;
        }

        /* No control codes in FGBG mode */
        if (canvas->config.canvas_mode != CHAFA_CANVAS_MODE_FGBG)
            out = chafa_term_info_emit_reset_attributes (ti, out);

        /* Last line should not end in newline */
        if (i_next < i_max)
            *(out++) = '\n';

        *out = '\0';
        gs->len = out - gs->str;
    }

    return gs;
}

GString *
chafa_canvas_print (ChafaCanvas *canvas, ChafaTermInfo *ti)
{
    GString *str;

    if (ti)
    {
        chafa_term_info_ref (ti);
    }
    else
    {
        gchar *env_vars [] = { "TERM=xterm-256color", NULL };
        ChafaTermDb *term_db = chafa_term_db_new ();
        ti = chafa_term_db_detect (term_db, env_vars);
        chafa_term_db_unref (term_db);
    }

    str = build_ansi_gstring (canvas, ti);

    chafa_term_info_unref (ti);
    return str;
}
