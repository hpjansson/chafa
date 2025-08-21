/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2025 Hans Petter Jansson
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
#include "internal/chafa-private.h"

/**
 * SECTION:chafa-features
 * @title: Features
 * @short_description: Platform-specific feature support
 *
 * Chafa supports a few platform-specific acceleration features. These
 * will be built in and used automatically when available. You can get
 * information about the available features through the function calls
 * documented in this section.
 **/

/**
 * ChafaFeatures:
 * @CHAFA_FEATURE_MMX: Flag indicating MMX support.
 * @CHAFA_FEATURE_SSE41: Flag indicating SSE 4.1 support.
 * @CHAFA_FEATURE_POPCNT: Flag indicating popcnt support.
 * @CHAFA_FEATURE_AVX2: Flag indicating AVX2 support.
 **/

static gboolean chafa_initialized;

static gboolean have_mmx;
static gboolean have_sse41;
static gboolean have_popcnt;
static gboolean have_avx2;

static gint n_threads = -1;

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

    /* For popcnt, AMD does not appear to need the SSE 4.2 check, but the Intel
     * documentation[1] says the following in section 12.12.3:
     *
     * > Before an application attempts to use the POPCNT instruction, it
     * > must check that the processor supports Intel SSE4.2 (if
     * > CPUID.01H:ECX.SSE4_2[bit 20] = 1) and POPCNT (if
     * > CPUID.01H:ECX.POPCNT[bit 23] = 1).
     *
     * [1] https://software.intel.com/en-us/download/intel-64-and-ia-32-architectures-sdm-combined-volumes-1-2a-2b-2c-2d-3a-3b-3c-3d-and-4
     *
     * So we check both. */

# ifdef HAVE_POPCNT_INTRINSICS
    if (__builtin_cpu_supports ("sse4.2")
        && __builtin_cpu_supports ("popcnt"))
        have_popcnt = TRUE;
# endif

# ifdef HAVE_AVX2_INTRINSICS
    if (__builtin_cpu_supports ("avx2"))
        have_avx2 = TRUE;
# endif
#endif
}

static gpointer
init_once (G_GNUC_UNUSED gpointer data)
{
    init_features ();
    chafa_init_palette ();
    chafa_init_symbols ();

    chafa_initialized = TRUE;
    return NULL;
}

void
chafa_init (void)
{
    static GOnce once = G_ONCE_INIT;

    g_once (&once, init_once, NULL);
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

gboolean
chafa_have_popcnt (void)
{
    return have_popcnt;
}

gboolean
chafa_have_avx2 (void)
{
    return have_avx2;
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

#ifdef HAVE_POPCNT_INTRINSICS
    features |= CHAFA_FEATURE_POPCNT;
#endif

#ifdef HAVE_AVX2_INTRINSICS
    features |= CHAFA_FEATURE_AVX2;
#endif

    return features;
}

/**
 * chafa_get_supported_features:
 *
 * Gets a list of the platform-specific features that are built in and usable
 * on the runtime platform.
 *
 * Returns: A set of flags indicating usable features
 **/
ChafaFeatures
chafa_get_supported_features (void)
{
    chafa_init ();

    return (have_mmx ? CHAFA_FEATURE_MMX : 0)
      | (have_sse41 ? CHAFA_FEATURE_SSE41 : 0)
      | (have_popcnt ? CHAFA_FEATURE_POPCNT : 0)
      | (have_avx2 ? CHAFA_FEATURE_AVX2 : 0);
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
    if (features & CHAFA_FEATURE_POPCNT)
        g_string_append (features_gstr, "popcnt ");
    if (features & CHAFA_FEATURE_AVX2)
        g_string_append (features_gstr, "avx2 ");

    if (features_gstr->len > 0 && features_gstr->str [features_gstr->len - 1] == ' ')
        g_string_truncate (features_gstr, features_gstr->len - 1);

    return g_string_free (features_gstr, FALSE);
}

/**
 * chafa_get_n_threads:
 *
 * Queries the maximum number of worker threads to use for parallel processing.
 *
 * Returns: The number of threads, or -1 if determined automatically
 **/
gint
chafa_get_n_threads (void)
{
    return g_atomic_int_get (&n_threads);
}

/**
 * chafa_set_n_threads:
 * @n: Number of threads
 *
 * Sets the maximum number of worker threads to use for parallel processing,
 * or -1 to determine this automatically. The default is -1.
 *
 * Setting this to 0 or 1 will avoid using thread pools and instead perform
 * all processing in the main thread.
 **/
void
chafa_set_n_threads (gint n)
{
    g_return_if_fail (n >= -1);

    g_atomic_int_set (&n_threads, n);
}

/**
 * chafa_get_n_actual_threads:
 *
 * Queries the number of worker threads that will actually be used for
 * parallel processing.
 *
 * Returns: Number of threads, always >= 1
 **/
gint
chafa_get_n_actual_threads (void)
{
    gint n_actual_threads;

    n_actual_threads = chafa_get_n_threads ();
    if (n_actual_threads < 0)
        n_actual_threads = g_get_num_processors ();
    if (n_actual_threads <= 0)
        n_actual_threads = 1;

    return n_actual_threads;
}
