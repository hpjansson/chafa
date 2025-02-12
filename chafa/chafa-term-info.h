/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020-2024 Hans Petter Jansson
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

#ifndef __CHAFA_TERM_INFO_H__
#define __CHAFA_TERM_INFO_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

G_BEGIN_DECLS

#define CHAFA_TERM_SEQ_LENGTH_MAX 96

/* Maximum number of arguments + 1 for sentinel */
#define CHAFA_TERM_SEQ_ARGS_MAX 24

#ifndef __GTK_DOC_IGNORE__
/* This declares the enum for CHAFA_TERM_SEQ_*. See chafa-term-seq-def.h
 * for more information, or look up the canonical documentation at
 * https://hpjansson.org/chafa/ref/ for verbose definitions. */
typedef enum
{
#define CHAFA_TERM_SEQ_DEF(name, NAME, n_args, arg_proc, arg_type, ...) CHAFA_TERM_SEQ_##NAME,
#define CHAFA_TERM_SEQ_DEF_VARARGS(name, NAME, arg_type) CHAFA_TERM_SEQ_##NAME,
#include <chafa-term-seq-def.h>
#undef CHAFA_TERM_SEQ_DEF
#undef CHAFA_TERM_SEQ_DEF_VARARGS

    CHAFA_TERM_SEQ_MAX
}
ChafaTermSeq;
#endif /* __GTK_DOC_IGNORE__ */

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

/**
 * ChafaParseResult:
 * @CHAFA_PARSE_SUCCESS: Parsed successfully
 * @CHAFA_PARSE_FAILURE: Data mismatch
 * @CHAFA_PARSE_AGAIN: Partial success, but not enough input
 *
 * An enumeration of the possible return values from the parsing function.
 **/
typedef enum
{
    CHAFA_PARSE_SUCCESS,
    CHAFA_PARSE_FAILURE,
    CHAFA_PARSE_AGAIN
}
ChafaParseResult;

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

CHAFA_AVAILABLE_IN_1_16
const gchar *chafa_term_info_get_name (ChafaTermInfo *term_info);
CHAFA_AVAILABLE_IN_1_16
void chafa_term_info_set_name (ChafaTermInfo *term_info, const gchar *name);

CHAFA_AVAILABLE_IN_1_16
gboolean chafa_term_info_is_canvas_mode_supported (ChafaTermInfo *term_info,
                                                   ChafaCanvasMode canvas_mode);
CHAFA_AVAILABLE_IN_1_16
ChafaCanvasMode chafa_term_info_get_best_canvas_mode (ChafaTermInfo *term_info);

CHAFA_AVAILABLE_IN_1_16
gboolean chafa_term_info_is_pixel_mode_supported (ChafaTermInfo *term_info,
                                                  ChafaPixelMode pixel_mode);
CHAFA_AVAILABLE_IN_1_16
ChafaPixelMode chafa_term_info_get_best_pixel_mode (ChafaTermInfo *term_info);

CHAFA_AVAILABLE_IN_1_16
ChafaPassthrough chafa_term_info_get_passthrough_type (ChafaTermInfo *term_info);

CHAFA_AVAILABLE_IN_1_16
gboolean chafa_term_info_get_is_pixel_passthrough_needed (ChafaTermInfo *term_info,
                                                          ChafaPixelMode pixel_mode);
CHAFA_AVAILABLE_IN_1_16
void chafa_term_info_set_is_pixel_passthrough_needed (ChafaTermInfo *term_info,
                                                      ChafaPixelMode pixel_mode,
                                                      gboolean pixel_passthrough_needed);

CHAFA_AVAILABLE_IN_1_16
ChafaSymbolTags chafa_term_info_get_safe_symbol_tags (ChafaTermInfo *term_info);
CHAFA_AVAILABLE_IN_1_16
void chafa_term_info_set_safe_symbol_tags (ChafaTermInfo *term_info, ChafaSymbolTags tags);

CHAFA_AVAILABLE_IN_1_6
const gchar *chafa_term_info_get_seq (ChafaTermInfo *term_info, ChafaTermSeq seq);
CHAFA_AVAILABLE_IN_1_6
gint chafa_term_info_set_seq (ChafaTermInfo *term_info, ChafaTermSeq seq, const gchar *str,
                              GError **error);
CHAFA_AVAILABLE_IN_1_6
gboolean chafa_term_info_have_seq (const ChafaTermInfo *term_info, ChafaTermSeq seq);
CHAFA_AVAILABLE_IN_1_14
gchar *chafa_term_info_emit_seq (ChafaTermInfo *term_info, ChafaTermSeq seq, ...);
CHAFA_AVAILABLE_IN_1_16
gchar *chafa_term_info_emit_seq_valist (ChafaTermInfo *term_info, ChafaTermSeq seq, va_list *args);
CHAFA_DEPRECATED_IN_1_16
ChafaParseResult chafa_term_info_parse_seq (ChafaTermInfo *term_info, ChafaTermSeq seq,
                                            gchar **input, gint *input_len,
                                            guint *args_out);
CHAFA_AVAILABLE_IN_1_16
ChafaParseResult chafa_term_info_parse_seq_varargs (ChafaTermInfo *term_info, ChafaTermSeq seq,
                                                    gchar **input, gint *input_len,
                                                    guint *args_out, gint *n_args_out);

CHAFA_AVAILABLE_IN_1_16
gboolean chafa_term_info_get_inherit_seq (ChafaTermInfo *term_info, ChafaTermSeq seq);
CHAFA_AVAILABLE_IN_1_16
void chafa_term_info_set_inherit_seq (ChafaTermInfo *term_info, ChafaTermSeq seq,
                                      gboolean inherit);

CHAFA_AVAILABLE_IN_1_6
void chafa_term_info_supplement (ChafaTermInfo *term_info, ChafaTermInfo *source);
CHAFA_AVAILABLE_IN_1_16
ChafaTermInfo *chafa_term_info_chain (ChafaTermInfo *outer, ChafaTermInfo *inner);

/* This declares the prototypes for chafa_term_info_emit_*(). See
 * chafa-term-seq-def.h for more information, or look up the canonical
 * documentation at https://hpjansson.org/chafa/ref/ for verbose
 * function prototypes. */
#define CHAFA_TERM_SEQ_DEF(name, NAME, n_args, arg_proc, arg_type, ...)  \
    CHAFA_TERM_SEQ_AVAILABILITY gchar * chafa_term_info_emit_##name(const ChafaTermInfo *term_info, gchar *dest __VA_ARGS__);
#define CHAFA_TERM_SEQ_DEF_VARARGS(name, NAME, arg_type)  \
    CHAFA_TERM_SEQ_AVAILABILITY gchar * chafa_term_info_emit_##name(const ChafaTermInfo *term_info, gchar *dest, arg_type *args, gint n_args);
#include <chafa-term-seq-def.h>
#undef CHAFA_TERM_SEQ_DEF
#undef CHAFA_TERM_SEQ_DEF_VARARGS

G_END_DECLS

#endif /* __CHAFA_TERM_INFO_H__ */
