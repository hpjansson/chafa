/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2022 Hans Petter Jansson
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

#ifndef __CHAFA_VERSION_MACROS_H__
#define __CHAFA_VERSION_MACROS_H__

#if !defined (__CHAFA_H_INSIDE__) && !defined (CHAFA_COMPILATION)
# error "Only <chafa.h> can be included directly."
#endif

/* Our current version is defined here */
#include <chafaconfig.h>

G_BEGIN_DECLS

/* Exported symbol versioning/visibility. Similar to the versioning macros
 * used by GLib. */

#define CHAFA_VERSION_1_0 (G_ENCODE_VERSION (1, 0))
#define CHAFA_VERSION_1_2 (G_ENCODE_VERSION (1, 2))
#define CHAFA_VERSION_1_4 (G_ENCODE_VERSION (1, 4))
#define CHAFA_VERSION_1_6 (G_ENCODE_VERSION (1, 6))
#define CHAFA_VERSION_1_8 (G_ENCODE_VERSION (1, 8))
#define CHAFA_VERSION_1_10 (G_ENCODE_VERSION (1, 10))
#define CHAFA_VERSION_1_12 (G_ENCODE_VERSION (1, 12))

/* Evaluates to the current stable version; for development cycles,
 * this means the next stable target. */
#if (CHAFA_MINOR_VERSION % 2)
#define CHAFA_VERSION_CUR_STABLE         (G_ENCODE_VERSION (CHAFA_MAJOR_VERSION, CHAFA_MINOR_VERSION + 1))
#else
#define CHAFA_VERSION_CUR_STABLE         (G_ENCODE_VERSION (CHAFA_MAJOR_VERSION, CHAFA_MINOR_VERSION))
#endif

/* Evaluates to the previous stable version */
#if (CHAFA_MINOR_VERSION % 2)
#define CHAFA_VERSION_PREV_STABLE        (G_ENCODE_VERSION (CHAFA_MAJOR_VERSION, CHAFA_MINOR_VERSION - 1))
#else
#define CHAFA_VERSION_PREV_STABLE        (G_ENCODE_VERSION (CHAFA_MAJOR_VERSION, CHAFA_MINOR_VERSION - 2))
#endif

/**
 * CHAFA_VERSION_MIN_REQUIRED:
 *
 * A macro that can be defined by the user prior to including
 * the chafa.h header.
 * The definition should be one of the predefined Chafa version
 * macros: %CHAFA_VERSION_1_0, %CHAFA_VERSION_1_2,...
 *
 * This macro defines the earliest version of Chafa that the package is
 * required to be able to compile against.
 *
 * If the compiler is configured to warn about the use of deprecated
 * functions, then using functions that were deprecated in version
 * %CHAFA_VERSION_MIN_REQUIRED or earlier will cause warnings (but
 * using functions deprecated in later releases will not).
 *
 * Since: 1.2
 */

/* Make sure all exportable symbols are made visible, even
 * deprecated ones. */
#ifdef CHAFA_COMPILATION
# define CHAFA_VERSION_MIN_REQUIRED      (CHAFA_VERSION_1_0)
#endif

/* If the package sets CHAFA_VERSION_MIN_REQUIRED to some future
 * CHAFA_VERSION_X_Y value that we don't know about, it will compare as
 * 0 in preprocessor tests. */
#ifndef CHAFA_VERSION_MIN_REQUIRED
# define CHAFA_VERSION_MIN_REQUIRED      (CHAFA_VERSION_CUR_STABLE)
#elif CHAFA_VERSION_MIN_REQUIRED == 0
# undef  CHAFA_VERSION_MIN_REQUIRED
# define CHAFA_VERSION_MIN_REQUIRED      (CHAFA_VERSION_CUR_STABLE + 2)
#endif

/**
 * CHAFA_VERSION_MAX_ALLOWED:
 *
 * A macro that can be defined by the user prior to including
 * the chafa.h header.
 * The definition should be one of the predefined Chafa version
 * macros: %CHAFA_VERSION_1_0, %CHAFA_VERSION_1_2,...
 *
 * This macro defines the latest version of the Chafa API that the
 * package is allowed to make use of.
 *
 * If the compiler is configured to warn about the use of deprecated
 * functions, then using functions added after version
 * %CHAFA_VERSION_MAX_ALLOWED will cause warnings.
 *
 * This should normally be set to the same value as
 * %CHAFA_VERSION_MIN_REQUIRED.
 *
 * Since: 1.2
 */
#if !defined (CHAFA_VERSION_MAX_ALLOWED) || (CHAFA_VERSION_MAX_ALLOWED == 0)
# undef CHAFA_VERSION_MAX_ALLOWED
# define CHAFA_VERSION_MAX_ALLOWED      (CHAFA_VERSION_CUR_STABLE)
#endif

/* Sanity checks */
#if CHAFA_VERSION_MIN_REQUIRED > CHAFA_VERSION_CUR_STABLE
#error "CHAFA_VERSION_MIN_REQUIRED must be <= CHAFA_VERSION_CUR_STABLE"
#endif
#if CHAFA_VERSION_MAX_ALLOWED < CHAFA_VERSION_MIN_REQUIRED
#error "CHAFA_VERSION_MAX_ALLOWED must be >= CHAFA_VERSION_MIN_REQUIRED"
#endif
#if CHAFA_VERSION_MIN_REQUIRED < CHAFA_VERSION_1_0
#error "CHAFA_VERSION_MIN_REQUIRED must be >= CHAFA_VERSION_1_0"
#endif

#ifndef _CHAFA_EXTERN
# define _CHAFA_EXTERN extern
#endif

#define CHAFA_AVAILABLE_IN_ALL _CHAFA_EXTERN

#if CHAFA_VERSION_MIN_REQUIRED >= CHAFA_VERSION_1_2
# define CHAFA_DEPRECATED_IN_1_2                G_DEPRECATED
# define CHAFA_DEPRECATED_IN_1_2_FOR(f)         G_DEPRECATED_FOR(f)
#else
# define CHAFA_DEPRECATED_IN_1_2                _CHAFA_EXTERN
# define CHAFA_DEPRECATED_IN_1_2_FOR(f)         _CHAFA_EXTERN
#endif

#if CHAFA_VERSION_MAX_ALLOWED < CHAFA_VERSION_1_2
# define CHAFA_AVAILABLE_IN_1_2                 G_UNAVAILABLE(1, 2)
#else
# define CHAFA_AVAILABLE_IN_1_2                 _CHAFA_EXTERN
#endif

#if CHAFA_VERSION_MIN_REQUIRED >= CHAFA_VERSION_1_4
# define CHAFA_DEPRECATED_IN_1_4                G_DEPRECATED
# define CHAFA_DEPRECATED_IN_1_4_FOR(f)         G_DEPRECATED_FOR(f)
#else
# define CHAFA_DEPRECATED_IN_1_4                _CHAFA_EXTERN
# define CHAFA_DEPRECATED_IN_1_4_FOR(f)         _CHAFA_EXTERN
#endif

#if CHAFA_VERSION_MAX_ALLOWED < CHAFA_VERSION_1_4
# define CHAFA_AVAILABLE_IN_1_4                 G_UNAVAILABLE(1, 4)
#else
# define CHAFA_AVAILABLE_IN_1_4                 _CHAFA_EXTERN
#endif

#if CHAFA_VERSION_MIN_REQUIRED >= CHAFA_VERSION_1_6
# define CHAFA_DEPRECATED_IN_1_6                G_DEPRECATED
# define CHAFA_DEPRECATED_IN_1_6_FOR(f)         G_DEPRECATED_FOR(f)
#else
# define CHAFA_DEPRECATED_IN_1_6                _CHAFA_EXTERN
# define CHAFA_DEPRECATED_IN_1_6_FOR(f)         _CHAFA_EXTERN
#endif

#if CHAFA_VERSION_MAX_ALLOWED < CHAFA_VERSION_1_6
# define CHAFA_AVAILABLE_IN_1_6                 G_UNAVAILABLE(1, 6)
#else
# define CHAFA_AVAILABLE_IN_1_6                 _CHAFA_EXTERN
#endif

#if CHAFA_VERSION_MIN_REQUIRED >= CHAFA_VERSION_1_8
# define CHAFA_DEPRECATED_IN_1_8                G_DEPRECATED
# define CHAFA_DEPRECATED_IN_1_8_FOR(f)         G_DEPRECATED_FOR(f)
#else
# define CHAFA_DEPRECATED_IN_1_8                _CHAFA_EXTERN
# define CHAFA_DEPRECATED_IN_1_8_FOR(f)         _CHAFA_EXTERN
#endif

#if CHAFA_VERSION_MAX_ALLOWED < CHAFA_VERSION_1_8
# define CHAFA_AVAILABLE_IN_1_8                 G_UNAVAILABLE(1, 8)
#else
# define CHAFA_AVAILABLE_IN_1_8                 _CHAFA_EXTERN
#endif

#if CHAFA_VERSION_MIN_REQUIRED >= CHAFA_VERSION_1_10
# define CHAFA_DEPRECATED_IN_1_10               G_DEPRECATED
# define CHAFA_DEPRECATED_IN_1_10_FOR(f)        G_DEPRECATED_FOR(f)
#else
# define CHAFA_DEPRECATED_IN_1_10               _CHAFA_EXTERN
# define CHAFA_DEPRECATED_IN_1_10_FOR(f)        _CHAFA_EXTERN
#endif

#if CHAFA_VERSION_MAX_ALLOWED < CHAFA_VERSION_1_10
# define CHAFA_AVAILABLE_IN_1_10                G_UNAVAILABLE(1, 10)
#else
# define CHAFA_AVAILABLE_IN_1_10                _CHAFA_EXTERN
#endif

#if CHAFA_VERSION_MIN_REQUIRED >= CHAFA_VERSION_1_12
# define CHAFA_DEPRECATED_IN_1_12               G_DEPRECATED
# define CHAFA_DEPRECATED_IN_1_12_FOR(f)        G_DEPRECATED_FOR(f)
#else
# define CHAFA_DEPRECATED_IN_1_12               _CHAFA_EXTERN
# define CHAFA_DEPRECATED_IN_1_12_FOR(f)        _CHAFA_EXTERN
#endif

#if CHAFA_VERSION_MAX_ALLOWED < CHAFA_VERSION_1_12
# define CHAFA_AVAILABLE_IN_1_12                G_UNAVAILABLE(1, 12)
#else
# define CHAFA_AVAILABLE_IN_1_12                _CHAFA_EXTERN
#endif

G_END_DECLS

#endif /* __CHAFA_VERSION_MACROS_H__ */
