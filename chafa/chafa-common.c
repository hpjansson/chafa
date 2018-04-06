/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018 Hans Petter Jansson
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

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <glib.h>
#include "chafa/chafa.h"
#include "chafa/chafa-private.h"

/**
 * ChafaFeatures:
 * @CHAFA_FEATURE_MMX: Flag indicating MMX support.
 * @CHAFA_FEATURE_SSE41: Flag indicating SSE 4.1 support.
 **/

static gboolean chafa_initialized;

static gboolean have_mmx;
static gboolean have_sse41;

static void
init_features (void)
{
#ifdef HAVE_GCC_X86_FEATURE_BUILTINS
    __builtin_cpu_init ();

# ifdef HAVE_MMX_INTRINSICS
    if (__builtin_cpu_supports ("mmx"))
        have_mmx = TRUE;
# endif

# ifdef HAVE_SSE41_INTRINSICS
    if (__builtin_cpu_supports ("sse4.1"))
        have_sse41 = TRUE;
# endif
#endif
}

void
chafa_init (void)
{
    if (chafa_initialized)
        return;

    chafa_initialized = TRUE;

    init_features ();
    chafa_init_palette ();
    chafa_init_symbols ();
}

gboolean
chafa_have_mmx (void)
{
    return have_mmx;
}

gboolean
chafa_have_sse41 (void)
{
    return have_sse41;
}

/* Public API */

/**
 * chafa_get_builtin_features:
 *
 * Gets a list of the platform-specific features this library was built with.
 *
 * Returns: A set of flags indicating features present.
 **/
ChafaFeatures
chafa_get_builtin_features (void)
{
    ChafaFeatures features = 0;

#ifdef HAVE_MMX_INTRINSICS
    features |= CHAFA_FEATURE_MMX;
#endif

#ifdef HAVE_SSE41_INTRINSICS
    features |= CHAFA_FEATURE_SSE41;
#endif

    return features;
}

/**
 * chafa_get_supported_features:
 *
 * Gets a list of the platform-specific features that are built in and usable
 * on the runtime platform.
 *
 * Returns: 
 **/
ChafaFeatures
chafa_get_supported_features (void)
{
    chafa_init ();

    return (have_mmx ? CHAFA_FEATURE_MMX : 0)
        | (have_sse41 ? CHAFA_FEATURE_SSE41 : 0);
}

/**
 * chafa_describe_features:
 * @features: A set of flags representing features
 *
 * Takes a set of flags potentially returned from chafa_get_builtin_features ()
 * or chafa_get_supported_features () and generates a human-readable ASCII
 * string descriptor.
 *
 * Returns: A string describing the features. This must be freed by caller.
 **/
gchar *
chafa_describe_features (ChafaFeatures features)
{
    GString *features_gstr = g_string_new ("");

    if (features & CHAFA_FEATURE_MMX)
        g_string_append (features_gstr, "mmx ");
    if (features & CHAFA_FEATURE_SSE41)
        g_string_append (features_gstr, "sse4.1 ");

    if (features_gstr->len > 0 && features_gstr->str [features_gstr->len - 1] == ' ')
        g_string_truncate (features_gstr, features_gstr->len - 1);

    return g_string_free (features_gstr, FALSE);
}
