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

ChafaFeatures
chafa_get_supported_features (void)
{
    chafa_init ();

    return (have_mmx ? CHAFA_FEATURE_MMX : 0)
        | (have_sse41 ? CHAFA_FEATURE_SSE41 : 0);
}
