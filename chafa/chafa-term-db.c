/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020-2025 Hans Petter Jansson
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

struct ChafaTermDb
{
    gint refs;
};

/* ------- *
 * Helpers *
 * ------- */

static gint
strcmp_wrap (const gchar *a, const gchar *b)
{
    if (a == NULL && b == NULL)
        return 0;
    if (a == NULL)
        return 1;
    if (b == NULL)
        return -1;

    return strcmp (a, b);
}

/* ------------------------------ *
 * Definitions for terminal table *
 * ------------------------------ */

/* Sequence inheritance
 * --------------------
 *
 * For inherited seqs:
 *
 * - If either inner or outer sequence is NULL, use outer sequence.
 * - Otherwise, use inner sequence.

 * The last rule is a special case that allows for using the inner term's
 * sequences while clearing them if the outer term does not support the
 * sequence at all. This is useful for muxers (e.g. fbterm supports 256 colors,
 * but with private seqs; we want to use the inner mux' corresponding seqs).
 *
 * For sequences not listed as inheritable:
 *
 * - Always pick the inner sequence.
 */

/* These make the table more readable */
#define VARIANT_NONE NULL
#define VERSION_NONE NULL
#define INHERIT_NONE NULL
#define PIXEL_PT_NONE NULL
#define QUIRKS_NONE 0

/* Common symbol capabilities */
#define LINUX_CONSOLE_SYMS (CHAFA_SYMBOL_TAG_ASCII)
#define LINUX_DESKTOP_SYMS (CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_BORDER)
#define WIN_TERMINAL_SYMS (CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_BORDER)

typedef enum
{
    TERM_TYPE_TERM,
    TERM_TYPE_MUX,
    TERM_TYPE_APP,

    TERM_TYPE_MAX
}
TermType;

typedef enum
{
    ENV_OP_INCL,  /* Include if true */
    ENV_OP_EXCL   /* Exclude if not true */
}
EnvOp;

typedef enum
{
    ENV_CMP_ISSET,
    ENV_CMP_EXACT,
    ENV_CMP_PREFIX,
    ENV_CMP_SUFFIX,
    ENV_CMP_VER_GE
}
EnvCmp;

typedef struct
{
    EnvOp op;
    EnvCmp cmp;
    const gchar *key;
    const gchar *value;
    gint priority;
}
EnvRule;

typedef struct
{
    ChafaTermSeq seq;
    const gchar *str;
}
SeqStr;

typedef struct
{
    ChafaPixelMode pixel_mode;
    gboolean need_passthrough;
}
PixelModePassthrough;

#define ENV_RULE_MAX 8
#define SEQ_LIST_MAX 12

typedef struct
{
    TermType type;
    const gchar *name;
    const gchar *variant;
    const gchar *version;
    const EnvRule env_rules [ENV_RULE_MAX];
    const SeqStr *seqs [SEQ_LIST_MAX];
    const ChafaTermSeq *inherit_seqs;
    ChafaPassthrough passthrough;
    const PixelModePassthrough *pixel_pt;
    ChafaTermQuirks quirks;
    ChafaSymbolTags safe_symbol_tags;
}
TermDef;

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

    { CHAFA_TERM_SEQ_QUERY_PRIMARY_DEVICE_ATTRIBUTES, "\033[0c" },
    { CHAFA_TERM_SEQ_PRIMARY_DEVICE_ATTRIBUTES, "\033[?%vc" },

    { CHAFA_TERM_SEQ_RESET_DEFAULT_FG, "\033]110\033\\" },
    { CHAFA_TERM_SEQ_SET_DEFAULT_FG, "\033]10;rgb:%1/%2/%3\e\\" },
    { CHAFA_TERM_SEQ_QUERY_DEFAULT_FG, "\033]10;?\033\\" },

    { CHAFA_TERM_SEQ_RESET_DEFAULT_BG, "\033]111\033\\" },
    { CHAFA_TERM_SEQ_SET_DEFAULT_BG, "\033]11;rgb:%1/%2/%3\e\\" },
    { CHAFA_TERM_SEQ_QUERY_DEFAULT_BG, "\033]11;?\033\\" },

    /* XTWINOPS */

    { CHAFA_TERM_SEQ_QUERY_TEXT_AREA_SIZE_CELLS, "\033[18t" },
    { CHAFA_TERM_SEQ_TEXT_AREA_SIZE_CELLS, "\033[8;%1;%2t" },

    { CHAFA_TERM_SEQ_QUERY_TEXT_AREA_SIZE_PX, "\033[14t" },
    { CHAFA_TERM_SEQ_TEXT_AREA_SIZE_PX, "\033[4;%1;%2t" },

    { CHAFA_TERM_SEQ_QUERY_CELL_SIZE_PX, "\033[16t" },
    { CHAFA_TERM_SEQ_CELL_SIZE_PX, "\033[6;%1;%2t" },

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

static const SeqStr kitty_seqs [] =
{
    { CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1, "\033_Ga=T,f=%1,s=%2,v=%3,c=%4,r=%5,m=1\033\\" },
    { CHAFA_TERM_SEQ_END_KITTY_IMAGE, "\033_Gm=0\033\\" },
    { CHAFA_TERM_SEQ_BEGIN_KITTY_IMAGE_CHUNK, "\033_Gm=1;" },
    { CHAFA_TERM_SEQ_END_KITTY_IMAGE_CHUNK, "\033\\" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const SeqStr kitty_virt_seqs [] =
{
    { CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_VIRT_IMAGE_V1, "\033_Ga=T,U=1,q=2,f=%1,s=%2,v=%3,c=%4,r=%5,i=%6,m=1\033\\" },

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

static const ChafaTermSeq tmux_inherit_seqs [] =
{
    CHAFA_TERM_SEQ_BEGIN_SIXELS,
    CHAFA_TERM_SEQ_END_SIXELS,

    CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1,
    CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_VIRT_IMAGE_V1,
    CHAFA_TERM_SEQ_END_KITTY_IMAGE,
    CHAFA_TERM_SEQ_BEGIN_KITTY_IMAGE_CHUNK,
    CHAFA_TERM_SEQ_END_KITTY_IMAGE_CHUNK,

    CHAFA_TERM_SEQ_MAX
};

static const PixelModePassthrough tmux_pixel_pt [] =
{
    { CHAFA_PIXEL_MODE_SIXELS, TRUE },
    { CHAFA_PIXEL_MODE_KITTY,  TRUE },
    { CHAFA_PIXEL_MODE_ITERM2, TRUE },
    { CHAFA_PIXEL_MODE_MAX,    FALSE }
};

static const PixelModePassthrough tmux_3_4_pixel_pt [] =
{
    { CHAFA_PIXEL_MODE_SIXELS, FALSE },
    { CHAFA_PIXEL_MODE_KITTY,  TRUE },
    { CHAFA_PIXEL_MODE_ITERM2, TRUE },
    { CHAFA_PIXEL_MODE_MAX,    FALSE }
};

static const SeqStr screen_seqs [] =
{
    { CHAFA_TERM_SEQ_BEGIN_SCREEN_PASSTHROUGH, "\033P" },
    { CHAFA_TERM_SEQ_END_SCREEN_PASSTHROUGH, "\033\\" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const ChafaTermSeq screen_inherit_seqs [] =
{
    CHAFA_TERM_SEQ_BEGIN_SIXELS,
    CHAFA_TERM_SEQ_END_SIXELS,

    CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1,
    CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_VIRT_IMAGE_V1,
    CHAFA_TERM_SEQ_END_KITTY_IMAGE,
    CHAFA_TERM_SEQ_BEGIN_KITTY_IMAGE_CHUNK,
    CHAFA_TERM_SEQ_END_KITTY_IMAGE_CHUNK,

    CHAFA_TERM_SEQ_MAX
};

static const PixelModePassthrough screen_pixel_pt [] =
{
    { CHAFA_PIXEL_MODE_SIXELS, TRUE },
    { CHAFA_PIXEL_MODE_KITTY,  TRUE },
    { CHAFA_PIXEL_MODE_ITERM2, TRUE },
    { CHAFA_PIXEL_MODE_MAX,    CHAFA_PASSTHROUGH_MAX }
};

static const SeqStr lf_seqs [] =
{
    { CHAFA_TERM_SEQ_ENABLE_BOLD, "\033[1m" },
    { CHAFA_TERM_SEQ_INVERT_COLORS, "\033[7m" },

    { CHAFA_TERM_SEQ_MAX, NULL }
};

static const ChafaTermSeq lf_inherit_seqs [] =
{
    CHAFA_TERM_SEQ_RESET_ATTRIBUTES,
    CHAFA_TERM_SEQ_ENABLE_BOLD,
    CHAFA_TERM_SEQ_INVERT_COLORS,

    CHAFA_TERM_SEQ_RESET_DEFAULT_FG,
    CHAFA_TERM_SEQ_SET_DEFAULT_FG,

    CHAFA_TERM_SEQ_RESET_DEFAULT_BG,
    CHAFA_TERM_SEQ_SET_DEFAULT_BG,

    CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT,
    CHAFA_TERM_SEQ_SET_COLOR_BG_DIRECT,
    CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT,

    CHAFA_TERM_SEQ_SET_COLOR_FG_256,
    CHAFA_TERM_SEQ_SET_COLOR_BG_256,
    CHAFA_TERM_SEQ_SET_COLOR_FGBG_256,

    CHAFA_TERM_SEQ_SET_COLOR_FG_16,
    CHAFA_TERM_SEQ_SET_COLOR_BG_16,
    CHAFA_TERM_SEQ_SET_COLOR_FGBG_16,

    CHAFA_TERM_SEQ_SET_COLOR_FG_8,
    CHAFA_TERM_SEQ_SET_COLOR_BG_8,
    CHAFA_TERM_SEQ_SET_COLOR_FGBG_8,

    CHAFA_TERM_SEQ_RESET_COLOR_FG,
    CHAFA_TERM_SEQ_RESET_COLOR_BG,
    CHAFA_TERM_SEQ_RESET_COLOR_FGBG,

    CHAFA_TERM_SEQ_BEGIN_SIXELS,
    CHAFA_TERM_SEQ_END_SIXELS,

    CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1,
    CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_VIRT_IMAGE_V1,
    CHAFA_TERM_SEQ_END_KITTY_IMAGE,
    CHAFA_TERM_SEQ_BEGIN_KITTY_IMAGE_CHUNK,
    CHAFA_TERM_SEQ_END_KITTY_IMAGE_CHUNK,

    CHAFA_TERM_SEQ_MAX
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
    kitty_virt_seqs,
    iterm2_seqs,
    screen_seqs,
    tmux_seqs,
    NULL
};

static const TermDef term_def [] =
{
    /* Mainline alacritty doesn't support sixels, but there's a patch for it:
     * https://github.com/alacritty/alacritty/pull/4763
     * It can only be detected interactively. It has the overshoot quirk. */
    { TERM_TYPE_TERM, "alacritty", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "alacritty", 10 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      CHAFA_TERM_QUIRK_SIXEL_OVERSHOOT, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "apple", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM_PROGRAM", "Apple_Terminal", 0 } },
      { vt220_seqs, color_256_seqs, color_16_seqs, color_8_seqs }, INHERIT_NONE,
      CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE, QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "contour", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERMINAL_NAME", "contour", 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        sixel_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "ctx", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_ISSET,  "CTX_BACKEND", NULL, 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        rep_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "eat", "truecolor", VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "eat-truecolor", 10 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        sixel_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "eat", "256color", VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "eat-256color", 10 } },
      { vt220_seqs, color_256_seqs, color_16_seqs, color_8_seqs, sixel_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "eat", "16color", VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "eat-16color", 10 } },
      { vt220_seqs, color_16_seqs, color_8_seqs, sixel_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "eat", "color", VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "eat-color", 10 } },
      { vt220_seqs, color_8_seqs, sixel_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "eat", "mono", VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "eat-mono", 10 } },
      { vt220_seqs, sixel_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "eat", "truecolor", VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_ISSET,  "EAT_SHELL_INTEGRATION_DIR", NULL, 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        sixel_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
        QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    /* FbTerm can use 256 colors through a private extension; see fbterm(1) */
    { TERM_TYPE_TERM, "fbterm", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "fbterm", 10 } },
      { vt220_seqs, color_fbterm_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "foot", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "foot", 10 },
        { ENV_OP_INCL, ENV_CMP_PREFIX, "TERM", "foot-", 10 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        sixel_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "ghostty", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "xterm-ghostty", 10 },
        { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM_PROGRAM", "ghostty", 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        kitty_seqs, kitty_virt_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE,
      PIXEL_PT_NONE, QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "iterm", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "LC_TERMINAL", "iTerm2", 0 },
        { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM_PROGRAM", "iTerm.app", 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        iterm2_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "kitty", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "xterm-kitty", 10 },
        { ENV_OP_INCL, ENV_CMP_ISSET,  "KITTY_PID", NULL, 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        kitty_seqs, kitty_virt_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE,
      PIXEL_PT_NONE, QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "konsole", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_ISSET,  "KONSOLE_VERSION", NULL, 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "konsole", VARIANT_NONE, "220370",
      { { ENV_OP_INCL, ENV_CMP_ISSET,  "KONSOLE_VERSION", NULL, 0 },
        { ENV_OP_EXCL, ENV_CMP_VER_GE, "KONSOLE_VERSION", "220370", 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        sixel_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      CHAFA_TERM_QUIRK_SIXEL_OVERSHOOT, LINUX_DESKTOP_SYMS },

    /* The 'lf' file browser will choke if there are extra sequences in front
     * of a sixel image, so we need to be polite to it. */
    { TERM_TYPE_APP,  "lf", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_ISSET,  "LF_LEVEL", NULL, 0 } },
      { lf_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs },
      lf_inherit_seqs, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    /* If TERM is "linux", we're probably on the Linux console, which supports
     * 16 colors only. It also sets COLORTERM=1.
     *
     * https://github.com/torvalds/linux/commit/cec5b2a97a11ade56a701e83044d0a2a984c67b4
     *
     * In theory we could emit directcolor codes and let the console remap,
     * but we get better results if we do the conversion ourselves, since we
     * can apply preprocessing and exotic color spaces. */
    { TERM_TYPE_TERM, "linux", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "linux", 10 } },
      { vt220_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_CONSOLE_SYMS },

    { TERM_TYPE_TERM, "mintty", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT, "TERM", "mintty", 10 },
        { ENV_OP_INCL, ENV_CMP_EXACT, "TERM_PROGRAM", "mintty", 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        iterm2_seqs, sixel_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      CHAFA_TERM_QUIRK_SIXEL_OVERSHOOT, WIN_TERMINAL_SYMS },

    /* mlterm's truecolor support seems to be broken; it looks like a color
     * allocation issue. This affects character cells, but not sixels. */
    { TERM_TYPE_TERM, "mlterm", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "mlterm", 10 },
        { ENV_OP_INCL, ENV_CMP_ISSET,  "MLTERM", NULL, 0 } },
      { vt220_seqs, color_256_seqs, color_16_seqs, color_8_seqs, iterm2_seqs,
        sixel_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      CHAFA_TERM_QUIRK_SIXEL_OVERSHOOT, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_APP,  "neovim", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_ISSET,  "NVIM", NULL, 0 } },
      { vt220_seqs, color_256_seqs, color_16_seqs, color_8_seqs }, INHERIT_NONE,
      CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE, QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_APP,  "neovim", "truecolor", VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "COLORTERM", "truecolor", 0 },
        { ENV_OP_INCL, ENV_CMP_EXACT,  "NVIM_TUI_ENABLE_TRUE_COLOR", "1", 0 },
        { ENV_OP_EXCL, ENV_CMP_ISSET,  "NVIM", NULL, 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "rio", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "rio", 10 },
        { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM_PROGRAM", "rio", 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        iterm2_seqs, sixel_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      CHAFA_TERM_QUIRK_SIXEL_OVERSHOOT, LINUX_DESKTOP_SYMS },

    /* rxvtu seems to support directcolor, but the default 256-color palette
     * appears to be broken/unusual. */
    { TERM_TYPE_TERM, "rxvt", "unicode", VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "rxvt-unicode", 10 } },
      { vt220_seqs, color_direct_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "rxvt", "unicode-256color", VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "rxvt-unicode-256color", 10 } },
      { vt220_seqs, color_direct_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    /* 'screen' does not like directcolor at all, but 256 colors works fine.
     * Sometimes we'll see the outer terminal appended to the TERM string,
     * like so: screen.xterm-256color */
    { TERM_TYPE_MUX,  "screen", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_PREFIX, "TERM", "screen", -5 } },
      { vt220_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        screen_seqs }, screen_inherit_seqs, CHAFA_PASSTHROUGH_SCREEN, screen_pixel_pt,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "st", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "st-256color", 10 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    /* 'tmux' sets TERM=screen or =screen-256color, but it supports directcolor
     * codes. You may have to add the following to .tmux.conf to prevent
     * remapping to 256 colors:
     *
     * tmux set-option -ga terminal-overrides ",screen-256color:Tc"
     *
     * tmux 3.4+ supports sixels natively. */
    { TERM_TYPE_MUX,  "tmux", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_ISSET,  "TMUX", NULL, 0 },
        { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM_PROGRAM", "tmux", 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        tmux_seqs }, tmux_inherit_seqs, CHAFA_PASSTHROUGH_TMUX, tmux_pixel_pt,
      CHAFA_TERM_QUIRK_SIXEL_OVERSHOOT, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_MUX,  "tmux", VARIANT_NONE, "3.4",
      { { ENV_OP_INCL, ENV_CMP_ISSET,  "TMUX", NULL, 0 },
        { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM_PROGRAM", "tmux", 0 },
        { ENV_OP_EXCL, ENV_CMP_VER_GE, "TERM_PROGRAM_VERSION", "3.4", 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        tmux_seqs }, tmux_inherit_seqs, CHAFA_PASSTHROUGH_TMUX, tmux_3_4_pixel_pt,
      CHAFA_TERM_QUIRK_SIXEL_OVERSHOOT, LINUX_DESKTOP_SYMS },

    /* Fallback when TERM is set but unrecognized */
    { TERM_TYPE_TERM, "vt220", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_ISSET,  "TERM", NULL, -1000 } },
      { vt220_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, CHAFA_SYMBOL_TAG_ASCII },

    { TERM_TYPE_TERM, "vte", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_ISSET,  "VTE_VERSION", NULL, 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "vte", VARIANT_NONE, "5202",
      { { ENV_OP_INCL, ENV_CMP_ISSET,  "VTE_VERSION", NULL, 0 },
        { ENV_OP_EXCL, ENV_CMP_VER_GE, "VTE_VERSION", "5202", 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        rep_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "warp", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM_PROGRAM", "WarpTerminal", 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        kitty_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, WIN_TERMINAL_SYMS },

    /* Note: WezTerm does not support Kitty virtual image placements yet.
     * See https://github.com/wez/wezterm/issues/986 */
    { TERM_TYPE_TERM, "wezterm", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM_PROGRAM", "WezTerm", 0 },
        { ENV_OP_INCL, ENV_CMP_ISSET,  "WEZTERM_EXECUTABLE", NULL, 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        sixel_seqs, iterm2_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    /* The MS Windows 10 TH2 (v1511+) console supports ANSI escape codes,
     * including AIX and DirectColor sequences. */
    { TERM_TYPE_TERM, "windows-console", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_SUFFIX, "ComSpec", "\\cmd.exe", -5 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE, QUIRKS_NONE, WIN_TERMINAL_SYMS },

    { TERM_TYPE_TERM, "xterm", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_ISSET, "XTERM_VERSION", NULL, 0 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE, QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    /* Terminals that advertise xterm-256color usually support truecolor too,
     * (VTE, xterm) although some (xterm) may quantize to an indexed palette
     * regardless. */
    { TERM_TYPE_TERM, "xterm", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT, "TERM", "xterm", -10 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE, QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    /* Terminals that advertise xterm-256color usually support truecolor too,
     * (VTE, xterm) although some (xterm) may quantize to an indexed palette
     * regardless. */
    { TERM_TYPE_TERM, "xterm", "256color", VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT, "TERM", "xterm-256color", -10 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE, QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { TERM_TYPE_TERM, "xterm", "direct", VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT, "TERM", "xterm-direct", -10 },
        { ENV_OP_INCL, ENV_CMP_EXACT, "TERM", "xterm-direct2", -10 },
        { ENV_OP_INCL, ENV_CMP_EXACT, "TERM", "xterm-direct16", -10 },
        { ENV_OP_INCL, ENV_CMP_EXACT, "TERM", "xterm-direct256", -10 } },
      { vt220_seqs, color_direct_seqs, color_256_seqs, color_16_seqs, color_8_seqs },
      INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE, QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    /* yaft supports sixels and directcolor escape codes, but it remaps cell
     * colors to a 256-color palette. */
    { TERM_TYPE_TERM, "yaft", VARIANT_NONE, VERSION_NONE,
      { { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "yaft-256color", 10 },
        { ENV_OP_INCL, ENV_CMP_EXACT,  "TERM", "yaft", 10 } },
      { vt220_seqs, color_256_seqs, color_16_seqs, color_8_seqs,
        sixel_seqs }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE,
      QUIRKS_NONE, LINUX_DESKTOP_SYMS },

    { 0, NULL, VARIANT_NONE, VERSION_NONE, { { 0, 0, NULL, NULL, 0 } },
      { NULL }, INHERIT_NONE, CHAFA_PASSTHROUGH_NONE, PIXEL_PT_NONE, QUIRKS_NONE, 0 }
};

/* Parse various version formats into a single integer for comparisons.
 * E.g. "1.2.3", "20240912", "XTerm(388)". */
static gint64
parse_version (const gchar *version_str)
{
    gint64 ver = 0;
    gint i;

    if (!version_str)
        return -1;

    for (i = 0; version_str [i]; i++)
    {
        gint n = version_str [i] - (gint) '0';

        if (n < 0 || n > 9)
            continue;

        ver = ver * 10 + n;
    }

    return ver;
}

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

static gboolean
match_env_rule (const EnvRule *r, const gchar *value)
{
    gboolean m = FALSE;
    
    switch (r->cmp)
    {
        case ENV_CMP_ISSET:
            m = value != NULL;
            break;
        case ENV_CMP_EXACT:
            m = value != NULL && (!g_ascii_strcasecmp (value, r->value));
            break;
        case ENV_CMP_PREFIX:
            m = value != NULL && (!g_ascii_strncasecmp (value, r->value, strlen (r->value)));
            break;
        case ENV_CMP_SUFFIX:
            m = value != NULL && strlen (r->value) <= strlen (value)
                && !g_ascii_strncasecmp (value + strlen (value) - strlen (r->value),
                                         r->value, strlen (r->value));
            break;
        case ENV_CMP_VER_GE:
            m = value != NULL && parse_version (value) >= parse_version (r->value);
            break;
    }

    return m;
}

static gint
match_term_def (const TermDef *term_def, gchar **envp)
{
    gint best_pri = G_MININT;
    gint i;

    for (i = 0; i < ENV_RULE_MAX && term_def->env_rules [i].key; i++)
    {
        const EnvRule *r = &term_def->env_rules [i];
        const gchar *value;
        gboolean m = FALSE;

        value = g_environ_getenv (envp, r->key);

        /* TERM can be a series of names separated by '.'. GNU Screen does this,
         * e.g. TERM=screen.xterm-256color */
        if (value != NULL && !g_ascii_strcasecmp (r->key, "TERM"))
        {
            gchar **subterms = g_strsplit (value, ".", -1);
            gint j;

            for (j = 0; subterms && subterms [j]; j++)
                m |= match_env_rule (r, subterms [j]);

            g_strfreev (subterms);
        }

        m |= match_env_rule (r, value);

        if (r->op == ENV_OP_EXCL)
        {
            if (!m)
            {
                best_pri = G_MININT;
                break;
            }
        }
        else if (m && r->priority >= best_pri)
        {
            best_pri = r->priority;
        }
        else
        {
            m = FALSE;
        }
    }

    return best_pri;
}

/* Terminal identifiers have three parts: name, variant and version.
 * Either or both of variant and version can be omitted. If version is
 * present but variant isn't, variant is replaced with a "*" placeholder.
 *
 * Examples of syntactically valid identifiers:
 *
 * vte
 * vte-256color
 * mlterm-*-3.9.3
 * xterm-256color-XTerm(389)
 */
static gchar *
term_def_to_name (const TermDef *def)
{
    const gchar *parts [3];

    parts [0] = def->name ? def->name : "unknown";
    parts [1] = def->variant ? def->variant : (def->version ? "*" : NULL);
    parts [2] = def->version;

    return g_strjoin ("-", parts [0], parts [1], parts [2], NULL);
}

static ChafaTermInfo *
new_term_info_from_def (const TermDef *def)
{
    const SeqStr * const *seqs = def->seqs;
    ChafaTermInfo *ti;
    gchar *name;
    gint i;

    name = term_def_to_name (def);

    ti = chafa_term_info_new ();
    chafa_term_info_set_name (ti, name);
    chafa_term_info_set_quirks (ti, def->quirks);
    chafa_term_info_set_safe_symbol_tags (ti, def->safe_symbol_tags);

    for (i = 0; i < SEQ_LIST_MAX && seqs [i]; i++)
    {
        add_seqs (ti, seqs [i]);
    }

    for (i = 0; def->pixel_pt && def->pixel_pt [i].pixel_mode < CHAFA_PIXEL_MODE_MAX; i++)
    {
        chafa_term_info_set_is_pixel_passthrough_needed (ti, def->pixel_pt [i].pixel_mode,
                                                         def->pixel_pt [i].need_passthrough);
    }

    for (i = 0; def->inherit_seqs && def->inherit_seqs [i] < CHAFA_TERM_SEQ_MAX; i++)
    {
        chafa_term_info_set_inherit_seq (ti, def->inherit_seqs [i], TRUE);
    }

    g_free (name);
    return ti;
}

static ChafaTermInfo *
detect_term_of_type (TermType term_type, gchar **envp)
{
    ChafaTermInfo *ti = NULL;
    gint best_def_i = -1;
    gint best_pri = G_MININT + 1;
    gint i;

    for (i = 0; term_def [i].name; i++)
    {
        gint pri;

        if (term_def [i].type != term_type)
            continue;

        /* Pick higher-priority solutions. If the priority is equal, allow
         * more specific and higher-version entries of the same TE to take precedence. */
        pri = match_term_def (&term_def [i], envp);
        if (pri > best_pri
            || (pri == best_pri
                && !strcmp_wrap (term_def [i].name, term_def [best_def_i].name)
                && ((term_def [i].variant && !term_def [best_def_i].variant)
                    || (!strcmp_wrap (term_def [i].variant, term_def [best_def_i].variant)
                        && parse_version (term_def [i].version) > parse_version (term_def [best_def_i].version)))))
        {
            best_pri = pri;
            best_def_i = i;
        }
    }

    if (best_def_i != -1)
    {
        ti = new_term_info_from_def (&term_def [best_def_i]);
    }

    return ti;
}

static ChafaTermInfo *
detect_capabilities (gchar **envp)
{
    ChafaTermInfo *ret_ti = NULL;
    gint i;

    for (i = 0; i < TERM_TYPE_MAX; i++)
    {
        ChafaTermInfo *ti = detect_term_of_type (i, envp);
        if (!ti)
            continue;

        if (ret_ti)
        {
            ChafaTermInfo *chained = chafa_term_info_chain (ret_ti, ti);
            chafa_term_info_unref (ret_ti);
            ret_ti = chained;
        }
        else
        {
            chafa_term_info_ref (ti);
            ret_ti = ti;
        }

        chafa_term_info_unref (ti);
    }

    if (!ret_ti)
    {
        /* Fallback for when we're completely clueless. Plain ASCII output
         * with no seqs at all. */
        ret_ti = chafa_term_info_new ();
        chafa_term_info_set_name (ret_ti, "dumb");
        chafa_term_info_set_safe_symbol_tags (ret_ti, CHAFA_SYMBOL_TAG_ASCII);
    }

    return ret_ti;
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
    g_return_val_if_fail (term_db != NULL, NULL);

    return detect_capabilities (envp);
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
