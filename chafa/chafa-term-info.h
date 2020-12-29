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

#ifndef __CHAFA_TERM_INFO_H__
#define __CHAFA_TERM_INFO_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

G_BEGIN_DECLS

#define CHAFA_TERM_SEQ_LENGTH_MAX 48

/* This declares the enum for CHAFA_TERM_SEQ_*. See chafa-term-seq-def.h
 * for more information, or look up the canonical documentation at
 * https://hpjansson.org/chafa/ref/ for verbose definitions.
 *
 * NOTE: I'd prefer to do this...
 *
 *     typedef enum
 *     {
 *     #define CHAFA_TERM_SEQ_DEF(name, NAME, n_args, arg_proc, arg_type, ...) CHAFA_TERM_SEQ_##NAME,
 *     #include <chafa-term-seq-def.h>
 *     #undef CHAFA_TERM_SEQ_DEF
 * 
 *         CHAFA_TERM_SEQ_MAX
 *     }
 *     ChafaTermSeq;
 *
 * ...but it breaks gtk-doc. */
typedef enum
{
    CHAFA_TERM_SEQ_RESET_TERMINAL_SOFT,
    CHAFA_TERM_SEQ_RESET_TERMINAL_HARD,
    CHAFA_TERM_SEQ_RESET_ATTRIBUTES,
    CHAFA_TERM_SEQ_CLEAR,
    CHAFA_TERM_SEQ_INVERT_COLORS,
    CHAFA_TERM_SEQ_CURSOR_TO_TOP_LEFT,
    CHAFA_TERM_SEQ_CURSOR_TO_BOTTOM_LEFT,
    CHAFA_TERM_SEQ_CURSOR_TO_POS,
    CHAFA_TERM_SEQ_CURSOR_UP_1,
    CHAFA_TERM_SEQ_CURSOR_UP,
    CHAFA_TERM_SEQ_CURSOR_DOWN_1,
    CHAFA_TERM_SEQ_CURSOR_DOWN,
    CHAFA_TERM_SEQ_CURSOR_LEFT_1,
    CHAFA_TERM_SEQ_CURSOR_LEFT,
    CHAFA_TERM_SEQ_CURSOR_RIGHT_1,
    CHAFA_TERM_SEQ_CURSOR_RIGHT,
    CHAFA_TERM_SEQ_CURSOR_UP_SCROLL,
    CHAFA_TERM_SEQ_CURSOR_DOWN_SCROLL,
    CHAFA_TERM_SEQ_INSERT_CELLS,
    CHAFA_TERM_SEQ_DELETE_CELLS,
    CHAFA_TERM_SEQ_INSERT_ROWS,
    CHAFA_TERM_SEQ_DELETE_ROWS,
    CHAFA_TERM_SEQ_SET_SCROLLING_ROWS,
    CHAFA_TERM_SEQ_ENABLE_INSERT,
    CHAFA_TERM_SEQ_DISABLE_INSERT,
    CHAFA_TERM_SEQ_ENABLE_CURSOR,
    CHAFA_TERM_SEQ_DISABLE_CURSOR,
    CHAFA_TERM_SEQ_ENABLE_ECHO,
    CHAFA_TERM_SEQ_DISABLE_ECHO,
    CHAFA_TERM_SEQ_ENABLE_WRAP,
    CHAFA_TERM_SEQ_DISABLE_WRAP,
    CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT,
    CHAFA_TERM_SEQ_SET_COLOR_BG_DIRECT,
    CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT,
    CHAFA_TERM_SEQ_SET_COLOR_FG_256,
    CHAFA_TERM_SEQ_SET_COLOR_BG_256,
    CHAFA_TERM_SEQ_SET_COLOR_FGBG_256,
    CHAFA_TERM_SEQ_SET_COLOR_FG_16,
    CHAFA_TERM_SEQ_SET_COLOR_BG_16,
    CHAFA_TERM_SEQ_SET_COLOR_FGBG_16,
    CHAFA_TERM_SEQ_BEGIN_SIXELS,
    CHAFA_TERM_SEQ_END_SIXELS,
    CHAFA_TERM_SEQ_REPEAT_CHAR,
    CHAFA_TERM_SEQ_MAX
}
ChafaTermSeq;

typedef struct ChafaTermInfo ChafaTermInfo;

/**
 * CHAFA_TERM_INFO_ERROR:
 * 
 * Error domain for #ChafaTermInfo. Errors in this domain will
 * be from the #ChafaTermInfoError enumeration. See #GError for information on 
 * error domains.
 **/
#define CHAFA_TERM_INFO_ERROR (chafa_term_info_error_quark ())

/**
 * ChafaTermInfoError:
 * @CHAFA_TERM_INFO_ERROR_SEQ_TOO_LONG: A control sequence could exceed
 *  #CHAFA_TERM_SEQ_LENGTH_MAX bytes if formatted with maximum argument lengths.
 * @CHAFA_TERM_INFO_ERROR_BAD_ESCAPE: An illegal escape sequence was used.
 * @CHAFA_TERM_INFO_ERROR_BAD_ARGUMENTS: A control sequence specified
 *  more than the maximum number of arguments, or an argument index was out
 *  of range.
 * 
 * Error codes returned by control sequence parsing.
 **/
typedef enum
{
    CHAFA_TERM_INFO_ERROR_SEQ_TOO_LONG,
    CHAFA_TERM_INFO_ERROR_BAD_ESCAPE,
    CHAFA_TERM_INFO_ERROR_BAD_ARGUMENTS
}
ChafaTermInfoError;

CHAFA_AVAILABLE_IN_1_6
GQuark chafa_term_info_error_quark (void);

CHAFA_AVAILABLE_IN_1_6
ChafaTermInfo *chafa_term_info_new (void);
CHAFA_AVAILABLE_IN_1_6
ChafaTermInfo *chafa_term_info_copy (const ChafaTermInfo *term_info);
CHAFA_AVAILABLE_IN_1_6
void chafa_term_info_ref (ChafaTermInfo *term_info);
CHAFA_AVAILABLE_IN_1_6
void chafa_term_info_unref (ChafaTermInfo *term_info);

CHAFA_AVAILABLE_IN_1_6
const gchar *chafa_term_info_get_seq (ChafaTermInfo *term_info, ChafaTermSeq seq);
CHAFA_AVAILABLE_IN_1_6
gint chafa_term_info_set_seq (ChafaTermInfo *term_info, ChafaTermSeq seq, const gchar *str,
                              GError **error);
CHAFA_AVAILABLE_IN_1_6
gboolean chafa_term_info_have_seq (const ChafaTermInfo *term_info, ChafaTermSeq seq);

CHAFA_AVAILABLE_IN_1_6
void chafa_term_info_supplement (ChafaTermInfo *term_info, ChafaTermInfo *source);

/* This declares the prototypes for chafa_term_info_emit_*(). See
 * chafa-term-seq-def.h for more information, or look up the canonical
 * documentation at https://hpjansson.org/chafa/ref/ for verbose
 * function prototypes. */
#define CHAFA_TERM_SEQ_DEF(name, NAME, n_args, arg_proc, arg_type, ...)  \
    CHAFA_TERM_SEQ_AVAILABILITY gchar * chafa_term_info_emit_##name(const ChafaTermInfo *term_info, gchar *dest __VA_ARGS__);
#include <chafa-term-seq-def.h>
#undef CHAFA_TERM_SEQ_DEF

G_END_DECLS

#endif /* __CHAFA_TERM_INFO_H__ */
