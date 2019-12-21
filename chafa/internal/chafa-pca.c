/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019 Hans Petter Jansson
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

#include "internal/chafa-pca.h"
#include <math.h>

static void
chafa_vec3i32_set (ChafaVec3i32 *out, gint32 x, gint32 y, gint32 z)
{
    out->v [0] = x;
    out->v [1] = y;
    out->v [2] = z;
}

static void
chafa_vec3i32_from_vec3 (ChafaVec3i32 *out, const ChafaVec3f32 *in)
{
    /* lrintf() rounding can be extremely slow, so use this function
     * sparingly. We use tricks to make GCC emit cvtss2si instructions
     * ("-fno-math-errno -fno-trapping-math", or simply "-ffast-math"),
     * but Clang apparently cannot be likewise persuaded.
     *
     * Clang _does_ like rounding with SSE 4.1, but that's not something
     * we can enable by default. */

    out->v [0] = lrintf (in->v [0]);
    out->v [1] = lrintf (in->v [1]);
    out->v [2] = lrintf (in->v [2]);
}

static gint32
chafa_vec3i32_dot_32 (const ChafaVec3i32 *v, const ChafaVec3i32 *u)
{
    return v->v [0] * u->v [0] + v->v [1] * u->v [1] + v->v [2] * u->v [2];
}

static gint64
chafa_vec3i32_dot_64 (const ChafaVec3i32 *v, const ChafaVec3i32 *u)
{
    return v->v [0] * u->v [0] + v->v [1] * u->v [1] + v->v [2] * u->v [2];
}

static void
chafa_vec3f32_copy (ChafaVec3f32 *dest, const ChafaVec3f32 *src)
{
    *dest = *src;
}

static void
chafa_vec3f32_add (ChafaVec3f32 *out, const ChafaVec3f32 *a, const ChafaVec3f32 *b)
{
    out->v [0] = a->v [0] + b->v [0];
    out->v [1] = a->v [1] + b->v [1];
    out->v [2] = a->v [2] + b->v [2];
}

static void
chafa_vec3f32_add_from_array (ChafaVec3f32 *accum, const ChafaVec3f32 *v, gint n)
{
    gint i;

    for (i = 0; i < n; i++)
    {
        chafa_vec3f32_add (accum, accum, &v [i]);
    }
}

static void
chafa_vec3f32_add_to_array (ChafaVec3f32 *v, const ChafaVec3f32 *in, gint n)
{
    gint i;

    for (i = 0; i < n; i++)
    {
        chafa_vec3f32_add (&v [i], &v [i], in);
    }
}

static void
chafa_vec3f32_sub (ChafaVec3f32 *out, const ChafaVec3f32 *a, const ChafaVec3f32 *b)
{
    out->v [0] = a->v [0] - b->v [0];
    out->v [1] = a->v [1] - b->v [1];
    out->v [2] = a->v [2] - b->v [2];
}

static void
chafa_vec3f32_mul_scalar (ChafaVec3f32 *out, const ChafaVec3f32 *in, gfloat s)
{
    out->v [0] = in->v [0] * s;
    out->v [1] = in->v [1] * s;
    out->v [2] = in->v [2] * s;
}

static gfloat
chafa_vec3f32_dot (const ChafaVec3f32 *v, const ChafaVec3f32 *u)
{
    return v->v [0] * u->v [0] + v->v [1] * u->v [1] + v->v [2] * u->v [2];
}

static void
chafa_vec3f32_set (ChafaVec3f32 *v, gfloat x, gfloat y, gfloat z)
{
    v->v [0] = x;
    v->v [1] = y;
    v->v [2] = z;
}

static void
chafa_vec3f32_set_zero (ChafaVec3f32 *v)
{
    chafa_vec3f32_set (v, 0.0, 0.0, 0.0);
}

static gfloat
chafa_vec3f32_get_magnitude (const ChafaVec3f32 *v)
{
    return sqrtf (v->v [0] * v->v [0] + v->v [1] * v->v [1] + v->v [2] * v->v [2]);
}

static void
chafa_vec3f32_normalize (ChafaVec3f32 *out, const ChafaVec3f32 *in)
{
    gfloat m;

    m = 1.0 / chafa_vec3f32_get_magnitude (in);
    out->v [0] = in->v [0] * m;
    out->v [1] = in->v [1] * m;
    out->v [2] = in->v [2] * m;
}

static void
chafa_vec3f32_average_array (ChafaVec3f32 *out, const ChafaVec3f32 *v, gint n)
{
    chafa_vec3f32_set_zero (out);
    chafa_vec3f32_add_from_array (out, v, n);
    chafa_vec3f32_mul_scalar (out, out, 1.0 / n);
}

#define PCA_POWER_MAX_ITERATIONS 1000
#define PCA_POWER_MIN_ERROR 0.0001

static gfloat
pca_converge (const ChafaVec3f32 *vecs_in, gint n_vecs,
              ChafaVec3f32 *eigenvector_out)
{
    ChafaVec3f32 r = CHAFA_VEC3F32_INIT (.11, .23, .51);
    gfloat eigenvalue;
    gint i, j;

    /* Power iteration */

    /* FIXME: r should probably be random, and we should try again
     * if we pick a bad one */

    chafa_vec3f32_normalize (&r, &r);

    for (j = 0; j < PCA_POWER_MAX_ITERATIONS; j++)
    {
        ChafaVec3f32 s = CHAFA_VEC3F32_INIT_ZERO;
        ChafaVec3f32 t;
        gfloat err;

        for (i = 0; i < n_vecs; i++)
        {
            gfloat u;

            u = chafa_vec3f32_dot (&vecs_in [i], &r);
            chafa_vec3f32_mul_scalar (&t, &vecs_in [i], u);
            chafa_vec3f32_add (&s, &s, &t);
        }

        eigenvalue = chafa_vec3f32_dot (&r, &s);

        chafa_vec3f32_mul_scalar (&t, &r, eigenvalue);
        chafa_vec3f32_sub (&t, &t, &s);
        err = chafa_vec3f32_get_magnitude (&t);

        chafa_vec3f32_copy (&r, &s);
        chafa_vec3f32_normalize (&r, &r);

        if (err < PCA_POWER_MIN_ERROR)
            break;
    }

    chafa_vec3f32_copy (eigenvector_out, &r);
    return eigenvalue;
}

static void
pca_deflate (ChafaVec3f32 *vecs, gint n_vecs, const ChafaVec3f32 *eigenvector)
{
    gint i;

    /* Calculate scores, reconstruct with scores and eigenvector,
     * then subtract from original vectors to generate residuals.
     * We should be able to get the next component from those. */

    for (i = 0; i < n_vecs; i++)
    {
        ChafaVec3f32 t;
        gfloat score;

        score = chafa_vec3f32_dot (&vecs [i], eigenvector);
        chafa_vec3f32_mul_scalar (&t, eigenvector, score);
        chafa_vec3f32_sub (&vecs [i], &vecs [i], &t);
    }
}

/**
 * chafa_vec3f32_array_compute_pca:
 * @vecs_in: Input vector array
 * @n_vecs: Number of vectors in array
 * @n_components: Number of components to compute (1 or 2)
 * @eigenvectors_out: Pointer to array to store n_components eigenvectors in, or NULL
 * @eigenvalues_out: Pointer to array to store n_components eigenvalues in, or NULL
 * @average_out: Pointer to a vector to store array average (for centering), or NULL
 *
 * Compute principal components from an array of 3D vectors. This
 * implementation is naive and probably not that fast, but it should
 * be good enough for our purposes.
 **/
void
chafa_vec3f32_array_compute_pca (const ChafaVec3f32 *vecs_in, gint n_vecs,
                                 gint n_components,
                                 ChafaVec3f32 *eigenvectors_out,
                                 gfloat *eigenvalues_out,
                                 ChafaVec3f32 *average_out)
{
    ChafaVec3f32 *v;
    ChafaVec3f32 average;
    ChafaVec3f32 t;
    gfloat eigenvalue;
    gint i;

    v = alloca (n_vecs * sizeof (ChafaVec3f32));
    memcpy (v, vecs_in, n_vecs * sizeof (ChafaVec3f32));

    /* Calculate average */

    chafa_vec3f32_average_array (&average, v, n_vecs);

    /* Recenter around average */

    chafa_vec3f32_set_zero (&t);
    chafa_vec3f32_sub (&t, &t, &average);
    chafa_vec3f32_add_to_array (v, &t, n_vecs);

    /* Compute principal components */

    for (i = 0; ; )
    {
        eigenvalue = pca_converge (v, n_vecs, &t);

        if (eigenvectors_out)
        {
            chafa_vec3f32_copy (eigenvectors_out, &t);
            eigenvectors_out++;
        }

        if (eigenvalues_out)
        {
            *eigenvalues_out = eigenvalue;
            eigenvalues_out++;
        }

        if (++i >= n_components)
            break;

        pca_deflate (v, n_vecs, &t);
    }

    if (average_out)
        chafa_vec3f32_copy (average_out, &average);
}
