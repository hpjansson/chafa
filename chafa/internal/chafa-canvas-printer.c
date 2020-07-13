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

static void
emit_ansi_truecolor (ChafaCanvas *canvas, GString *gs, gint i, gint i_max)
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
                /* FIXME: Respect include/exclude for space */
                if (i < i_max - 1 && canvas->cells [i + 1].c == 0)
                    g_string_append (gs, "\x1b[0m  ");
                else
                    g_string_append (gs, "\x1b[0m ");
            }
            else
            {
                g_string_append_printf (gs, "\x1b[0m\x1b[7m\x1b[38;2;%d;%d;%dm",
                                        bg.ch [0], bg.ch [1], bg.ch [2]);
                g_string_append_unichar (gs, cell->c);
            }
        }
        else if (bg.ch [3] < canvas->config.alpha_threshold)
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[38;2;%d;%d;%dm",
                                    fg.ch [0], fg.ch [1], fg.ch [2]);
            g_string_append_unichar (gs, cell->c);
        }
        else
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[38;2;%d;%d;%dm\x1b[48;2;%d;%d;%dm",
                                    fg.ch [0], fg.ch [1], fg.ch [2],
                                    bg.ch [0], bg.ch [1], bg.ch [2]);
            g_string_append_unichar (gs, cell->c);
        }
    }
}

static void
emit_ansi_256 (ChafaCanvas *canvas, GString *gs, gint i, gint i_max)
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
                if (i < i_max - 1 && canvas->cells [i + 1].c == 0)
                    g_string_append (gs, "\x1b[0m  ");
                else
                    g_string_append (gs, "\x1b[0m ");
            }
            else
            {
                g_string_append_printf (gs, "\x1b[0m\x1b[7m\x1b[38;5;%dm",
                                        cell->bg_color);
                g_string_append_unichar (gs, cell->c);
            }
        }
        else if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[38;5;%dm",
                                    cell->fg_color);
            g_string_append_unichar (gs, cell->c);
        }
        else
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[38;5;%dm\x1b[48;5;%dm",
                                    cell->fg_color,
                                    cell->bg_color);
            g_string_append_unichar (gs, cell->c);
        }
    }
}

/* Uses aixterm control codes for bright colors */
static void
emit_ansi_16 (ChafaCanvas *canvas, GString *gs, gint i, gint i_max)
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
                if (i < i_max - 1 && canvas->cells [i + 1].c == 0)
                    g_string_append (gs, "\x1b[0m  ");
                else
                    g_string_append (gs, "\x1b[0m ");
            }
            else
            {
                g_string_append_printf (gs, "\x1b[0m\x1b[7m\x1b[%dm",
                                        cell->bg_color < 8 ? cell->bg_color + 30
                                                           : cell->bg_color + 90 - 8);
                g_string_append_unichar (gs, cell->c);
            }
        }
        else if (cell->bg_color == CHAFA_PALETTE_INDEX_TRANSPARENT)
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[%dm",
                                    cell->fg_color < 8 ? cell->fg_color + 30
                                                       : cell->fg_color + 90 - 8);
            g_string_append_unichar (gs, cell->c);
        }
        else
        {
            g_string_append_printf (gs, "\x1b[0m\x1b[%dm\x1b[%dm",
                                    cell->fg_color < 8 ? cell->fg_color + 30
                                                       : cell->fg_color + 90 - 8,
                                    cell->bg_color < 8 ? cell->bg_color + 40
                                                       : cell->bg_color + 100 - 8);
            g_string_append_unichar (gs, cell->c);
        }
    }
}

static void
emit_ansi_fgbg_bgfg (ChafaCanvas *canvas, GString *gs, gint i, gint i_max)
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

        g_string_append_printf (gs, "\x1b[%dm",
                                invert ? 7 : 0);
        g_string_append_unichar (gs, c);
    }
}

static void
emit_ansi_fgbg (ChafaCanvas *canvas, GString *gs, gint i, gint i_max)
{
    for ( ; i < i_max; i++)
    {
        ChafaCanvasCell *cell = &canvas->cells [i];

        /* Wide symbols have a zero code point in the rightmost cell */
        if (cell->c == 0)
            continue;

        g_string_append_unichar (gs, cell->c);
    }
}

static GString *
build_ansi_gstring (ChafaCanvas *canvas)
{
    GString *gs = g_string_new ("");
    gint i, i_max, i_step, i_next;

    i = 0;
    i_max = canvas->config.width * canvas->config.height;
    i_step = canvas->config.width;

    for ( ; i < i_max; i = i_next)
    {
        i_next = i + i_step;

        switch (canvas->config.canvas_mode)
        {
            case CHAFA_CANVAS_MODE_TRUECOLOR:
                emit_ansi_truecolor (canvas, gs, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_INDEXED_256:
            case CHAFA_CANVAS_MODE_INDEXED_240:
                emit_ansi_256 (canvas, gs, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_INDEXED_16:
                emit_ansi_16 (canvas, gs, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_FGBG_BGFG:
                emit_ansi_fgbg_bgfg (canvas, gs, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_FGBG:
                emit_ansi_fgbg (canvas, gs, i, i_next);
                break;
            case CHAFA_CANVAS_MODE_MAX:
                g_assert_not_reached ();
                break;
        }

        /* No control codes in FGBG mode */
        if (canvas->config.canvas_mode != CHAFA_CANVAS_MODE_FGBG)
            g_string_append (gs, "\x1b[0m");

        /* Last line should not end in newline */
        if (i_next < i_max)
            g_string_append_c (gs, '\n');
    }

    return gs;
}

GString *
chafa_canvas_print (ChafaCanvas *canvas, ChafaTermInfo *ti)
{
    return build_ansi_gstring (canvas);
}
