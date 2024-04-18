/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020-2023 Hans Petter Jansson
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
#include <stdlib.h>  /* strtoul */
#include <string.h>  /* strlen, strncmp, strcmp, memcpy */

/**
 * SECTION:chafa-term-db
 * @title: ChafaTermDb
 * @short_description: A database of terminal information
 *
 * A #ChafaTermDb contains information on terminals, and can be used to obtain
 * a suitable #ChafaTermInfo for a terminal environment.
 **/

/* This is a very naïve implementation, but perhaps good enough for most
 * contemporary terminal emulators. I've kept the API minimal so actual
 * termcap/terminfo subset parsing can be added later if needed without
 * breaking existing applications. */

struct ChafaTermDb
{
    gint refs;
};

typedef struct
{
    ChafaTermSeq seq;
    const gchar *str;
}
SeqStr;

static const SeqStr vt220_seqs [] =
{
    { CHAFA_TERM_SEQ_RESET_TERMINAL_SOFT, "\033[!p" },
    { CHAFA_TERM_SEQ_RESET_TERMINAL_HARD, "\033c" },
    { CHAFA_TERM_SEQ_RESET_ATTRIBUTES, "\033[0m" },
    { CHAFA_TERM_SEQ_CLEAR, "\033[2J" },
    { CHAFA_TERM_SEQ_ENABLE_BOLD, "\033[1m" },
    { CHAFA_TERM_SEQ_INVERT_COLORS, "\033[7m" },
    { CHAFA_TERM_SEQ_CURSOR_TO_TOP_LEFT, "\033[0H" },
    { CHAFA_TERM_SEQ_CURSOR_TO_BOTTOM_LEFT, "\033[9999;1H" },
    { CHAFA_TERM_SEQ_CURSOR_TO_POS, "\033[%2;%1H" },
    { CHAFA_TERM_SEQ_CURSOR_UP, "\033[%1A" },
    { CHAFA_TERM_SEQ_CURSOR_UP_1, "\033[A" },
    { CHAFA_TERM_SEQ_CURSOR_DOWN, "\033[%1B" },
    { CHAFA_TERM_SEQ_CURSOR_DOWN_1, "\033[B" },
    { CHAFA_TERM_SEQ_CURSOR_LEFT, "\033[%1D" },
    { CHAFA_TERM_SEQ_CURSOR_LEFT_1, "\033[D" },
    { CHAFA_TERM_SEQ_CURSOR_RIGHT, "\033[%1C" },
    { CHAFA_TERM_SEQ_CURSOR_RIGHT_1, "\033[C" },
    { CHAFA_TERM_SEQ_CURSOR_UP_SCROLL, "\033M" },
    { CHAFA_TERM_SEQ_CURSOR_DOWN_SCROLL, "\033D" },
    { CHAFA_TERM_SEQ_INSERT_CELLS, "\033[%1@" },
    { CHAFA_TERM_SEQ_DELETE_CELLS, "\033[%1P" },
    { CHAFA_TERM_SEQ_INSERT_ROWS, "\033[%1L" },
    { CHAFA_TERM_SEQ_DELETE_ROWS, "\033[%1M" },
    { CHAFA_TERM_SEQ_SET_SCROLLING_ROWS, "\033[%1;%2r" },
    { CHAFA_TERM_SEQ_ENABLE_INSERT, "\033[4h" },
    { CHAFA_TERM_SEQ_DISABLE_INSERT,"\033[4l" },
    { CHAFA_TERM_SEQ_ENABLE_CURSOR, "\033[?25h" },
    { CHAFA_TERM_SEQ_DISABLE_CURSOR, "\033[?25l" },
    { CHAFA_TERM_SEQ_ENABLE_ECHO, "\033[12l" },
    { CHAFA_TERM_SEQ_DISABLE_ECHO, "\033[12h" },
    { CHAFA_TERM_SEQ_ENABLE_WRAP, "\033[?7h" },
    { CHAFA_TERM_SEQ_DISABLE_WRAP, "\033[?7l" },
    { CHAFA_TERM_SEQ_RESET_SCROLLING_ROWS, "\033[r" },
    { CHAFA_TERM_SEQ_SAVE_CURSOR_POS, "\033[s" },
    { CHAFA_TERM_SEQ_RESTORE_CURSOR_POS, "\033[u" },

    /* These are actually xterm seqs, but we'll allow it */
    { CHAFA_TERM_SEQ_ENABLE_ALT_SCREEN, "\033[1049h" },
    { CHAFA_TERM_SEQ_DISABLE_ALT_SCREEN, "\033[1049l" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr rep_seqs [] =
{
    { CHAFA_TERM_SEQ_REPEAT_CHAR, "\033[%1b" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr sixel_seqs [] =
{
    { CHAFA_TERM_SEQ_BEGIN_SIXELS, "\033P%1;%2;%3q" },
    { CHAFA_TERM_SEQ_END_SIXELS, "\033\\" },
    { CHAFA_TERM_SEQ_ENABLE_SIXEL_SCROLLING, "\033[?80l" },
    { CHAFA_TERM_SEQ_DISABLE_SIXEL_SCROLLING, "\033[?80h" },
    { CHAFA_TERM_SEQ_SET_SIXEL_ADVANCE_DOWN, "\033[?8452l" },
    { CHAFA_TERM_SEQ_SET_SIXEL_ADVANCE_RIGHT, "\033[?8452h" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr default_color_seqs [] =
{
    { CHAFA_TERM_SEQ_RESET_DEFAULT_FG, "\033]110\033\\" },
    { CHAFA_TERM_SEQ_SET_DEFAULT_FG, "\033]10;rgb:%1/%2/%3\e\\" },
    { CHAFA_TERM_SEQ_QUERY_DEFAULT_FG, "\033]10;?\033\\" },

    { CHAFA_TERM_SEQ_RESET_DEFAULT_BG, "\033]111\033\\" },
    { CHAFA_TERM_SEQ_SET_DEFAULT_BG, "\033]11;rgb:%1/%2/%3\e\\" },
    { CHAFA_TERM_SEQ_QUERY_DEFAULT_BG, "\033]11;?\033\\" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr default_key_seqs [] =
{
    { CHAFA_TERM_SEQ_RETURN_KEY, "\x0d" },  /* ASCII CR */
    { CHAFA_TERM_SEQ_BACKSPACE_KEY, "\x7f" },  /* ASCII DEL */
    { CHAFA_TERM_SEQ_TAB_KEY, "\x09" },  /* ASCII HT */
    { CHAFA_TERM_SEQ_TAB_SHIFT_KEY, "\033[Z" },
    { CHAFA_TERM_SEQ_UP_KEY, "\033[A" },
    { CHAFA_TERM_SEQ_UP_CTRL_KEY, "\033[1;5A" },
    { CHAFA_TERM_SEQ_UP_SHIFT_KEY, "\033[1;2A" },
    { CHAFA_TERM_SEQ_DOWN_KEY, "\033[B" },
    { CHAFA_TERM_SEQ_DOWN_CTRL_KEY, "\033[1;5B" },
    { CHAFA_TERM_SEQ_DOWN_SHIFT_KEY, "\033[1;2B" },
    { CHAFA_TERM_SEQ_LEFT_KEY, "\033[D" },
    { CHAFA_TERM_SEQ_LEFT_CTRL_KEY, "\033[1;5D" },
    { CHAFA_TERM_SEQ_LEFT_SHIFT_KEY, "\033[1;2D" },
    { CHAFA_TERM_SEQ_RIGHT_KEY, "\033[C" },
    { CHAFA_TERM_SEQ_RIGHT_CTRL_KEY, "\033[1;5C" },
    { CHAFA_TERM_SEQ_RIGHT_SHIFT_KEY, "\033[1;2C" },
    { CHAFA_TERM_SEQ_PAGE_UP_KEY, "\033[5~" },
    { CHAFA_TERM_SEQ_PAGE_UP_CTRL_KEY, "\033[5;5~" },
    { CHAFA_TERM_SEQ_PAGE_UP_SHIFT_KEY, "\033[5;2~" },
    { CHAFA_TERM_SEQ_PAGE_DOWN_KEY, "\033[6~" },
    { CHAFA_TERM_SEQ_PAGE_DOWN_CTRL_KEY, "\033[6;5~" },
    { CHAFA_TERM_SEQ_PAGE_DOWN_SHIFT_KEY, "\033[6;2~" },
    { CHAFA_TERM_SEQ_HOME_KEY, "\033[H" },
    { CHAFA_TERM_SEQ_HOME_CTRL_KEY, "\033[1;5H" },
    { CHAFA_TERM_SEQ_HOME_SHIFT_KEY, "\033[1;2H" },
    { CHAFA_TERM_SEQ_END_KEY, "\033[F" },
    { CHAFA_TERM_SEQ_END_CTRL_KEY, "\033[1;5F" },
    { CHAFA_TERM_SEQ_END_SHIFT_KEY, "\033[1;2F" },
    { CHAFA_TERM_SEQ_INSERT_KEY, "\033[2~" },
    { CHAFA_TERM_SEQ_INSERT_CTRL_KEY, "\033[2;5~" },
    { CHAFA_TERM_SEQ_INSERT_SHIFT_KEY, "\033[2;2~" },
    { CHAFA_TERM_SEQ_DELETE_KEY, "\033[3~" },
    { CHAFA_TERM_SEQ_DELETE_CTRL_KEY, "\033[3;5~" },
    { CHAFA_TERM_SEQ_DELETE_SHIFT_KEY, "\033[3;2~" },
    { CHAFA_TERM_SEQ_F1_KEY, "\033OP" },
    { CHAFA_TERM_SEQ_F1_CTRL_KEY, "\033[1;5P" },
    { CHAFA_TERM_SEQ_F1_SHIFT_KEY, "\033[1;2P" },
    { CHAFA_TERM_SEQ_F2_KEY, "\033OQ" },
    { CHAFA_TERM_SEQ_F2_CTRL_KEY, "\033[1;5Q" },
    { CHAFA_TERM_SEQ_F2_SHIFT_KEY, "\033[1;2Q" },
    { CHAFA_TERM_SEQ_F3_KEY, "\033R" },
    { CHAFA_TERM_SEQ_F3_CTRL_KEY, "\033[1;5R" },
    { CHAFA_TERM_SEQ_F3_SHIFT_KEY, "\033[1;2R" },
    { CHAFA_TERM_SEQ_F4_KEY, "\033OS" },
    { CHAFA_TERM_SEQ_F4_CTRL_KEY, "\033[1;5S" },
    { CHAFA_TERM_SEQ_F4_SHIFT_KEY, "\033[1;2S" },
    { CHAFA_TERM_SEQ_F5_KEY, "\033[15~" },
    { CHAFA_TERM_SEQ_F5_CTRL_KEY, "\033[15;5~" },
    { CHAFA_TERM_SEQ_F5_SHIFT_KEY, "\033[15;2~" },
    { CHAFA_TERM_SEQ_F6_KEY, "\033[17~" },
    { CHAFA_TERM_SEQ_F6_CTRL_KEY, "\033[17;5~" },
    { CHAFA_TERM_SEQ_F6_SHIFT_KEY, "\033[17;2~" },
    { CHAFA_TERM_SEQ_F7_KEY, "\033[18~" },
    { CHAFA_TERM_SEQ_F7_CTRL_KEY, "\033[18;5~" },
    { CHAFA_TERM_SEQ_F7_SHIFT_KEY, "\033[18;2~" },
    { CHAFA_TERM_SEQ_F8_KEY, "\033[19~" },
    { CHAFA_TERM_SEQ_F8_CTRL_KEY, "\033[19;5~" },
    { CHAFA_TERM_SEQ_F8_SHIFT_KEY, "\033[19;2~" },
    { CHAFA_TERM_SEQ_F9_KEY, "\033[20~" },
    { CHAFA_TERM_SEQ_F9_CTRL_KEY, "\033[20;5~" },
    { CHAFA_TERM_SEQ_F9_SHIFT_KEY, "\033[20;2~" },
    { CHAFA_TERM_SEQ_F10_KEY, "\033[21~" },
    { CHAFA_TERM_SEQ_F10_CTRL_KEY, "\033[21;5~" },
    { CHAFA_TERM_SEQ_F10_SHIFT_KEY, "\033[21;2~" },
    { CHAFA_TERM_SEQ_F11_KEY, "\033[23~" },
    { CHAFA_TERM_SEQ_F11_CTRL_KEY, "\033[23;5~" },
    { CHAFA_TERM_SEQ_F11_SHIFT_KEY, "\033[23;2~" },
    { CHAFA_TERM_SEQ_F12_KEY, "\033[24~" },
    { CHAFA_TERM_SEQ_F12_CTRL_KEY, "\033[24;5~" },
    { CHAFA_TERM_SEQ_F12_SHIFT_KEY, "\033[24;2~" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr color_direct_seqs [] =
{
    /* ISO 8613-6 */
    { CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT, "\033[38;2;%1;%2;%3m" },
    { CHAFA_TERM_SEQ_SET_COLOR_BG_DIRECT, "\033[48;2;%1;%2;%3m" },
    { CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT, "\033[38;2;%1;%2;%3;48;2;%4;%5;%6m" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr color_256_seqs [] =
{
    { CHAFA_TERM_SEQ_SET_COLOR_FG_256, "\033[38;5;%1m" },
    { CHAFA_TERM_SEQ_SET_COLOR_BG_256, "\033[48;5;%1m" },
    { CHAFA_TERM_SEQ_SET_COLOR_FGBG_256, "\033[38;5;%1;48;5;%2m" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr color_16_seqs [] =
{
    { CHAFA_TERM_SEQ_SET_COLOR_FG_16, "\033[%1m" },
    { CHAFA_TERM_SEQ_SET_COLOR_BG_16, "\033[%1m" },
    { CHAFA_TERM_SEQ_SET_COLOR_FGBG_16, "\033[%1;%2m" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr color_8_seqs [] =
{
    { CHAFA_TERM_SEQ_SET_COLOR_FG_8, "\033[%1m" },
    { CHAFA_TERM_SEQ_SET_COLOR_BG_8, "\033[%1m" },
    { CHAFA_TERM_SEQ_SET_COLOR_FGBG_8, "\033[%1;%2m" },

    /* ECMA-48 3rd ed. March 1984 */
    { CHAFA_TERM_SEQ_RESET_COLOR_FG, "\033[39m" },
    { CHAFA_TERM_SEQ_RESET_COLOR_BG, "\033[49m" },
    { CHAFA_TERM_SEQ_RESET_COLOR_FGBG, "\033[39;49m" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr *color_direct_list [] =
{
    color_direct_seqs,
    color_256_seqs,
    color_16_seqs,
    color_8_seqs,
    NULL
};

static const SeqStr *color_256_list [] =
{
    color_256_seqs,
    color_16_seqs,
    color_8_seqs,
    NULL
};

static const SeqStr *color_16_list [] =
{
    color_16_seqs,
    color_8_seqs,
    NULL
};

static const SeqStr color_fbterm_seqs [] =
{
    { CHAFA_TERM_SEQ_SET_COLOR_FG_16, "\033[1;%1}" },
    { CHAFA_TERM_SEQ_SET_COLOR_BG_16, "\033[2;%1}" },
    { CHAFA_TERM_SEQ_SET_COLOR_FGBG_16, "\033[1;%1}\033[2;%2}" },
    { CHAFA_TERM_SEQ_SET_COLOR_FG_256, "\033[1;%1}" },
    { CHAFA_TERM_SEQ_SET_COLOR_BG_256, "\033[2;%1}" },
    { CHAFA_TERM_SEQ_SET_COLOR_FGBG_256, "\033[1;%1}\033[2;%2}" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr *color_fbterm_list [] =
{
    color_fbterm_seqs,
    color_8_seqs,
    NULL
};

static const SeqStr kitty_seqs [] =
{
    { CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1, "\033_Ga=T,f=%1,s=%2,v=%3,c=%4,r=%5,m=1\033\\" },
    { CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_VIRT_IMAGE_V1, "\033_Ga=T,U=1,q=2,f=%1,s=%2,v=%3,c=%4,r=%5,i=%6,m=1\033\\" },
    { CHAFA_TERM_SEQ_END_KITTY_IMAGE, "\033_Gm=0\033\\" },
    { CHAFA_TERM_SEQ_BEGIN_KITTY_IMAGE_CHUNK, "\033_Gm=1;" },
    { CHAFA_TERM_SEQ_END_KITTY_IMAGE_CHUNK, "\033\\" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr iterm2_seqs [] =
{
    { CHAFA_TERM_SEQ_BEGIN_ITERM2_IMAGE, "\033]1337;File=inline=1;width=%1;height=%2;preserveAspectRatio=0:" },
    { CHAFA_TERM_SEQ_END_ITERM2_IMAGE, "\a" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr tmux_seqs [] =
{
    { CHAFA_TERM_SEQ_BEGIN_TMUX_PASSTHROUGH, "\033Ptmux;" },
    { CHAFA_TERM_SEQ_END_TMUX_PASSTHROUGH, "\033\\" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr screen_seqs [] =
{
    { CHAFA_TERM_SEQ_BEGIN_SCREEN_PASSTHROUGH, "\033P" },
    { CHAFA_TERM_SEQ_END_SCREEN_PASSTHROUGH, "\033\\" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr *fallback_list [] =
{
    vt220_seqs,
    color_direct_seqs,
    color_256_seqs,
    color_16_seqs,
    color_8_seqs,
    sixel_seqs,
    kitty_seqs,
    iterm2_seqs,
    screen_seqs,
    tmux_seqs,
    NULL
};

static void
add_seqs (ChafaTermInfo *ti, const SeqStr *seqstr)
{
    gint i;

    if (!seqstr)
        return;

    for (i = 0; seqstr [i].str; i++)
    {
        chafa_term_info_set_seq (ti, seqstr [i].seq, seqstr [i].str, NULL);
    }
}

static void
add_seq_list (ChafaTermInfo *ti, const SeqStr **seqlist)
{
    gint i;

    if (!seqlist)
        return;

    for (i = 0; seqlist [i]; i++)
    {
        add_seqs (ti, seqlist [i]);
    }
}

static const gchar *
getenv_or_blank (gchar **envp, const gchar *key)
{
    const gchar *value;

    value = g_environ_getenv (envp, key);
    if (!value)
        value = "";

    return value;
}

static void
detect_capabilities (ChafaTermInfo *ti, gchar **envp)
{
    const gchar *term;
    const gchar *colorterm;
    const gchar *konsole_version;
    const gchar *vte_version;
    const gchar *term_program;
    const gchar *term_name;
    const gchar *tmux;
    const gchar *ctx_backend;
    const gchar *lc_terminal;
    const gchar *kitty_pid;
    const gchar *mlterm;
    const gchar *nvim;
    const gchar *nvim_tui_enable_true_color;
    const gchar *eat_shell_integration_dir;
    gchar *comspec = NULL;
    const SeqStr **color_seq_list = color_256_list;
    const SeqStr *gfx_seqs = NULL;
    const SeqStr *rep_seqs_local = NULL;
    const SeqStr *inner_seqs = NULL;

    term = getenv_or_blank (envp, "TERM");
    colorterm = getenv_or_blank (envp, "COLORTERM");
    konsole_version = getenv_or_blank (envp, "KONSOLE_VERSION");
    vte_version = getenv_or_blank (envp, "VTE_VERSION");
    term_program = getenv_or_blank (envp, "TERM_PROGRAM");
    term_name = getenv_or_blank (envp, "TERMINAL_NAME");
    tmux = getenv_or_blank (envp, "TMUX");
    ctx_backend = getenv_or_blank (envp, "CTX_BACKEND");
    lc_terminal = getenv_or_blank (envp, "LC_TERMINAL");
    kitty_pid = getenv_or_blank (envp, "KITTY_PID");
    mlterm = getenv_or_blank (envp, "MLTERM");
    nvim = getenv_or_blank (envp, "NVIM");
    nvim_tui_enable_true_color = getenv_or_blank (envp, "NVIM_TUI_ENABLE_TRUE_COLOR");
    eat_shell_integration_dir = getenv_or_blank (envp, "EAT_SHELL_INTEGRATION_DIR");

    /* The MS Windows 10 TH2 (v1511+) console supports ANSI escape codes,
     * including AIX and DirectColor sequences. We detect this early and allow
     * TERM to override, if present. */
    comspec = (gchar *) g_environ_getenv (envp, "ComSpec");
    if (comspec)
    {
        comspec = g_ascii_strdown (comspec, -1);
        if (g_str_has_suffix (comspec, "\\cmd.exe"))
            color_seq_list = color_direct_list;
        g_free (comspec);
        comspec = NULL;
    }

    /* Some terminals set COLORTERM=truecolor. However, this env var can
     * make its way into environments where truecolor is not desired
     * (e.g. screen sessions), so check it early on and override it later. */
    if (!g_ascii_strcasecmp (colorterm, "truecolor")
        || !g_ascii_strcasecmp (colorterm, "gnome-terminal")
        || !g_ascii_strcasecmp (colorterm, "xfce-terminal"))
        color_seq_list = color_direct_list;

    /* In a modern VTE we can rely on VTE_VERSION. It's a great terminal emulator
     * which supports truecolor. */
    if (strlen (vte_version) > 0)
    {
        color_seq_list = color_direct_list;

        /* Newer VTE versions understand REP */
        if (g_ascii_strtoull (vte_version, NULL, 10) >= 5202
            && !strcmp (term, "xterm-256color"))
            rep_seqs_local = rep_seqs;
    }

    /* Konsole exports KONSOLE_VERSION */
    if (strtoul (konsole_version, NULL, 10) >= 220370)
    {
        /* Konsole version 22.03.70+ supports sixel graphics */
        gfx_seqs = sixel_seqs;
    }

    /* The ctx terminal (https://ctx.graphics/) understands REP */
    if (strlen (ctx_backend) > 0)
        rep_seqs_local = rep_seqs;

    /* Terminals that advertise 256 colors usually support truecolor too,
     * (VTE, xterm) although some (xterm) may quantize to an indexed palette
     * regardless. */
    if (!strcmp (term, "xterm-256color")
        || !strcmp (term, "xterm-direct")
        || !strcmp (term, "xterm-direct2")
        || !strcmp (term, "xterm-direct16")
        || !strcmp (term, "xterm-direct256")
        || !strcmp (term, "xterm-kitty")
        || !strcmp (term, "st-256color"))
        color_seq_list = color_direct_list;

    /* Kitty has a unique graphics protocol */
    if (!strcmp (term, "xterm-kitty")
        || strlen (kitty_pid) > 0)
        gfx_seqs = kitty_seqs;

    /* iTerm2 supports truecolor and has a unique graphics protocol */
    if (!g_ascii_strcasecmp (lc_terminal, "iTerm2")
        || !g_ascii_strcasecmp (term_program, "iTerm.app"))
    {
        color_seq_list = color_direct_list;
        gfx_seqs = iterm2_seqs;
    }

    if (!g_ascii_strcasecmp (term_program, "WezTerm"))
    {
        gfx_seqs = sixel_seqs;
    }

    if (!g_ascii_strcasecmp (term_name, "contour"))
    {
        gfx_seqs = sixel_seqs;
    }

    /* Check for Neovim early. It pretends to be xterm-256color, and may or
     * may not support directcolor. */
    if (strlen (nvim) > 0)
    {
        /* The Neovim terminal defaults to 256 colors unless termguicolors has
         * been set to true. */
        color_seq_list = color_256_list;

        /* If COLORTERM was explicitly set to truecolor, honor it. Neovim may do
         * this when termguicolors has been set to true *and* COLORTERM was
         * previously set. See Neovim commit d8963c434f01e6a7316 (Nov 26, 2020).
         *
         * The user may also set NVIM_TUI_ENABLE_TRUE_COLOR=1 in older Neovim
         * versions. We'll honor that one blindly, since it's specific and there
         * seems to be no better option. */
        if (!g_ascii_strcasecmp (colorterm, "truecolor")
            || !g_ascii_strcasecmp (nvim_tui_enable_true_color, "1"))
        {
            color_seq_list = color_direct_list;
        }
    }

    /* Apple Terminal sets TERM=xterm-256color, and does not support truecolor */
    if (!g_ascii_strcasecmp (term_program, "Apple_Terminal"))
        color_seq_list = color_256_list;

    /* mlterm's truecolor support seems to be broken; it looks like a color
     * allocation issue. This affects character cells, but not sixels.
     *
     * yaft supports sixels and truecolor escape codes, but it remaps cell
     * colors to a 256-color palette. */
    if (!strcmp (term, "mlterm")
        || strlen (mlterm) > 0
        || !strcmp (term, "yaft")
        || !strcmp (term, "yaft-256color"))
    {
        /* The default canvas mode is truecolor for sixels. 240 colors is
         * the default for symbols. */
        color_seq_list = color_256_list;
        gfx_seqs = sixel_seqs;
    }

    if (!strcmp (term, "foot") || !strncmp (term, "foot-", 5))
        gfx_seqs = sixel_seqs;

    /* rxvt 256-color really is 256 colors only */
    if (!strcmp (term, "rxvt-unicode-256color"))
        color_seq_list = color_256_list;

    /* Regular rxvt supports 16 colors at most */
    if (!strcmp (term, "rxvt-unicode"))
        color_seq_list = color_16_list;

    /* Eat uses the "eat-" prefix for TERM.
     * Eat also sets EAT_SHELL_INTEGRATION_DIR in the environment. */
    if (!strncmp (term, "eat-", 4) || strcmp (eat_shell_integration_dir, ""))
        gfx_seqs = sixel_seqs;

    /* 'screen' does not like truecolor at all, but 256 colors works fine.
     * Sometimes we'll see the outer terminal appended to the TERM string,
     * like so: screen.xterm-256color */
    if (!strncmp (term, "screen", 6))
    {
        /* 'tmux' also sets TERM=screen, but it supports truecolor codes.
         * You may have to add the following to .tmux.conf to prevent
         * remapping to 256 colors:
         *
         * tmux set-option -ga terminal-overrides ",screen-256color:Tc" */
        if (strlen (tmux) > 0)
        {
            color_seq_list = color_direct_list;
            inner_seqs = tmux_seqs;
        }
        else
        {
            color_seq_list = color_256_list;
            inner_seqs = screen_seqs;
        }

        /* screen and older tmux do not support REP. Newer tmux does,
         * but there's no reliable way to tell which version we're dealing with. */
        rep_seqs_local = NULL;

        /* Graphics is allowed in screen and tmux, with passthrough. */
        /* gfx_seqs = NULL; */
    }

    /* If TERM is "linux", we're probably on the Linux console, which supports
     * 16 colors only. It also sets COLORTERM=1.
     *
     * https://github.com/torvalds/linux/commit/cec5b2a97a11ade56a701e83044d0a2a984c67b4
     *
     * In theory we could emit truecolor codes and let the console remap,
     * but we get better results if we do the conversion ourselves, since we
     * can apply preprocessing and exotic color spaces. */
    if (!strcmp (term, "linux"))
        color_seq_list = color_16_list;

    /* FbTerm can use 256 colors through a private extension; see fbterm(1) */
    if (!strcmp (term, "fbterm"))
        color_seq_list = color_fbterm_list;

    add_seqs (ti, vt220_seqs);
    add_seq_list (ti, color_seq_list);
    add_seqs (ti, gfx_seqs);
    add_seqs (ti, rep_seqs_local);
    add_seqs (ti, inner_seqs);
}

static ChafaTermDb *
instantiate_singleton (G_GNUC_UNUSED gpointer data)
{
    return chafa_term_db_new ();
}

/* Public */

/**
 * chafa_term_db_new:
 *
 * Creates a new, blank #ChafaTermDb.
 *
 * Returns: The new #ChafaTermDb
 *
 * Since: 1.6
 **/
ChafaTermDb *
chafa_term_db_new (void)
{
    ChafaTermDb *term_db;

    term_db = g_new0 (ChafaTermDb, 1);
    term_db->refs = 1;

    return term_db;
}

/**
 * chafa_term_db_copy:
 * @term_db: A #ChafaTermDb to copy.
 *
 * Creates a new #ChafaTermDb that's a copy of @term_db.
 *
 * Returns: The new #ChafaTermDb
 *
 * Since: 1.6
 **/
ChafaTermDb *
chafa_term_db_copy (const ChafaTermDb *term_db)
{
    ChafaTermDb *new_term_db;

    new_term_db = g_new (ChafaTermDb, 1);
    memcpy (new_term_db, term_db, sizeof (ChafaTermDb));
    new_term_db->refs = 1;

    return new_term_db;
}

/**
 * chafa_term_db_ref:
 * @term_db: #ChafaTermDb to add a reference to.
 *
 * Adds a reference to @term_db.
 *
 * Since: 1.6
 **/
void
chafa_term_db_ref (ChafaTermDb *term_db)
{
    gint refs;

    g_return_if_fail (term_db != NULL);
    refs = g_atomic_int_get (&term_db->refs);
    g_return_if_fail (refs > 0);

    g_atomic_int_inc (&term_db->refs);
}

/**
 * chafa_term_db_unref:
 * @term_db: #ChafaTermDb to remove a reference from.
 *
 * Removes a reference from @term_db.
 *
 * Since: 1.6
 **/
void
chafa_term_db_unref (ChafaTermDb *term_db)
{
    gint refs;

    g_return_if_fail (term_db != NULL);
    refs = g_atomic_int_get (&term_db->refs);
    g_return_if_fail (refs > 0);

    if (g_atomic_int_dec_and_test (&term_db->refs))
    {
        g_free (term_db);
    }
}

/**
 * chafa_term_db_get_default:
 *
 * Gets the global #ChafaTermDb. This can normally be used safely
 * in a read-only capacity. The caller should not unref the
 * returned object.
 *
 * Returns: The global #ChafaTermDb
 *
 * Since: 1.6
 **/
ChafaTermDb *
chafa_term_db_get_default (void)
{
  static GOnce my_once = G_ONCE_INIT;

  g_once (&my_once, (GThreadFunc) instantiate_singleton, NULL);
  return my_once.retval;
}

/**
 * chafa_term_db_detect:
 * @term_db: A #ChafaTermDb.
 * @envp: A strv of environment variables.
 *
 * Builds a new #ChafaTermInfo with capabilities implied by the provided
 * environment variables (principally the TERM variable, but also others).
 *
 * @envp can be gotten from g_get_environ().
 *
 * Returns: A new #ChafaTermInfo.
 *
 * Since: 1.6
 **/
ChafaTermInfo *
chafa_term_db_detect (ChafaTermDb *term_db, gchar **envp)
{
    ChafaTermInfo *ti;

    g_return_val_if_fail (term_db != NULL, NULL);

    ti = chafa_term_info_new ();
    detect_capabilities (ti, envp);
    return ti;
}

/**
 * chafa_term_db_get_fallback_info:
 * @term_db: A #ChafaTermDb.
 *
 * Builds a new #ChafaTermInfo with fallback control sequences. This
 * can be used with unknown but presumably modern terminals, or to
 * supplement missing capabilities in a detected terminal.
 *
 * Fallback control sequences may cause unpredictable behavior and
 * should only be used as a last resort.
 *
 * Returns: A new #ChafaTermInfo.
 *
 * Since: 1.6
 **/
ChafaTermInfo *
chafa_term_db_get_fallback_info (ChafaTermDb *term_db)
{
    ChafaTermInfo *ti;

    g_return_val_if_fail (term_db != NULL, NULL);

    ti = chafa_term_info_new ();
    add_seq_list (ti, fallback_list);

    return ti;
}
