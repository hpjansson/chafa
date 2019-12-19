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
chafa_vec3_copy (ChafaVec3 *dest, const ChafaVec3 *src)
{
    *dest = *src;
}

static void
chafa_vec3_add (ChafaVec3 *out, const ChafaVec3 *a, const ChafaVec3 *b)
{
    out->v [0] = a->v [0] + b->v [0];
    out->v [1] = a->v [1] + b->v [1];
    out->v [2] = a->v [2] + b->v [2];
}

static void
chafa_vec3_add_from_array (ChafaVec3 *accum, const ChafaVec3 *v, gint n)
{
    gint i;

    for (i = 0; i < n; i++)
    {
        chafa_vec3_add (accum, accum, &v [i]);
    }
}

static void
chafa_vec3_add_to_array (ChafaVec3 *v, const ChafaVec3 *in, gint n)
{
    gint i;

    for (i = 0; i < n; i++)
    {
        chafa_vec3_add (&v [i], &v [i], in);
    }
}

static void
chafa_vec3_sub (ChafaVec3 *out, const ChafaVec3 *a, const ChafaVec3 *b)
{
    out->v [0] = a->v [0] - b->v [0];
    out->v [1] = a->v [1] - b->v [1];
    out->v [2] = a->v [2] - b->v [2];
}

static void
chafa_vec3_mul_scalar (ChafaVec3 *out, const ChafaVec3 *in, gfloat s)
{
    out->v [0] = in->v [0] * s;
    out->v [1] = in->v [1] * s;
    out->v [2] = in->v [2] * s;
}

static gfloat
chafa_vec3_dot (const ChafaVec3 *v, const ChafaVec3 *u)
{
    return v->v [0] * u->v [0] + v->v [1] * u->v [1] + v->v [2] * u->v [2];
}

static void
chafa_vec3_set (ChafaVec3 *v, gfloat x, gfloat y, gfloat z)
{
    v->v [0] = x;
    v->v [1] = y;
    v->v [2] = z;
}

static void
chafa_vec3_set_zero (ChafaVec3 *v)
{
    chafa_vec3_set (v, 0.0, 0.0, 0.0);
}

static gfloat
chafa_vec3_get_magnitude (const ChafaVec3 *v)
{
    return sqrtf (v->v [0] * v->v [0] + v->v [1] * v->v [1] + v->v [2] * v->v [2]);
}

static void
chafa_vec3_normalize (ChafaVec3 *out, const ChafaVec3 *in)
{
    gfloat m;

    m = 1.0 / chafa_vec3_get_magnitude (in);
    out->v [0] = in->v [0] * m;
    out->v [1] = in->v [1] * m;
    out->v [2] = in->v [2] * m;
}

static void
chafa_vec3_average_array (ChafaVec3 *out, const ChafaVec3 *v, gint n)
{
    chafa_vec3_set_zero (out);
    chafa_vec3_add_from_array (out, v, n);
    chafa_vec3_mul_scalar (out, out, 1.0 / n);
}

#define PCA_POWER_MAX_ITERATIONS 1000
#define PCA_POWER_MIN_ERROR 0.0001

static gfloat
pca_converge (const ChafaVec3 *vecs_in, gint n_vecs,
              ChafaVec3 *eigenvector_out)
{
    ChafaVec3 r = CHAFA_VEC3_INIT (.11, .23, .51);
    gfloat eigenvalue;
    gint i, j;

    /* Power iteration */

    /* FIXME: r should probably be random, and we should try again
     * if we pick a bad one */

    chafa_vec3_normalize (&r, &r);

    for (j = 0; j < PCA_POWER_MAX_ITERATIONS; j++)
    {
        ChafaVec3 s = CHAFA_VEC3_INIT_ZERO;
        ChafaVec3 t;
        gfloat err;

        for (i = 0; i < n_vecs; i++)
        {
            gfloat u;

            u = chafa_vec3_dot (&vecs_in [i], &r);
            chafa_vec3_mul_scalar (&t, &vecs_in [i], u);
            chafa_vec3_add (&s, &s, &t);
        }

        eigenvalue = chafa_vec3_dot (&r, &s);

        chafa_vec3_mul_scalar (&t, &r, eigenvalue);
        chafa_vec3_sub (&t, &t, &s);
        err = chafa_vec3_get_magnitude (&t);

        chafa_vec3_copy (&r, &s);
        chafa_vec3_normalize (&r, &r);

        if (err < PCA_POWER_MIN_ERROR)
            break;
    }

    chafa_vec3_copy (eigenvector_out, &r);
    return eigenvalue;
}

static void
pca_deflate (ChafaVec3 *vecs, gint n_vecs, const ChafaVec3 *eigenvector)
{
    gint i;

    /* Calculate scores, reconstruct with scores and eigenvector,
     * then subtract from original vectors to generate residuals.
     * We should be able to get the next component from those. */

    for (i = 0; i < n_vecs; i++)
    {
        ChafaVec3 t;
        gfloat score;

        score = chafa_vec3_dot (&vecs [i], eigenvector);
        chafa_vec3_mul_scalar (&t, eigenvector, score);
        chafa_vec3_sub (&vecs [i], &vecs [i], &t);
    }
}

/**
 * chafa_vec3_array_compute_pca:
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
chafa_vec3_array_compute_pca (const ChafaVec3 *vecs_in, gint n_vecs,
                              gint n_components,
                              ChafaVec3 *eigenvectors_out,
                              gfloat *eigenvalues_out,
                              ChafaVec3 *average_out)
{
    ChafaVec3 *v;
    ChafaVec3 average;
    ChafaVec3 t;
    gfloat eigenvalue;
    gint i;

    v = alloca (n_vecs * sizeof (ChafaVec3));
    memcpy (v, vecs_in, n_vecs * sizeof (ChafaVec3));

    /* Calculate average */

    chafa_vec3_average_array (&average, v, n_vecs);

    /* Recenter around average */

    chafa_vec3_set_zero (&t);
    chafa_vec3_sub (&t, &t, &average);
    chafa_vec3_add_to_array (v, &t, n_vecs);

    /* Compute principal components */

    for (i = 0; ; )
    {
        eigenvalue = pca_converge (v, n_vecs, &t);

        if (eigenvectors_out)
        {
            chafa_vec3_copy (eigenvectors_out, &t);
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
        chafa_vec3_copy (average_out, &average);
}
