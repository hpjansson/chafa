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

#include "config.h"

#include <string.h>  /* memcpy */
#include "internal/chafa-pca.h"

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

    v = g_malloc (n_vecs * sizeof (ChafaVec3f32));
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

    g_free (v);
}
