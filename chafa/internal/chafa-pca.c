/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2019-2025 Hans Petter Jansson
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

#include "internal/chafa-pca.h"

#define PCA_POWER_MAX_ITERATIONS 1000

/* Stop when the residual has dropped to this fraction of the eigenvalue */
#define PCA_POWER_REL_ERROR 1e-5f

typedef struct
{
    gfloat m [3] [3];
}
Matrix33f32;

/* Symmetric matrix, mul in upper matrix triangle */
static void
matrix33_mul_vec3_sym (ChafaVec3f32 *out, const Matrix33f32 *a, const ChafaVec3f32 *v)
{
    out->v [0] = a->m [0] [0] * v->v [0] + a->m [0] [1] * v->v [1] + a->m [0] [2] * v->v [2];
    out->v [1] = a->m [0] [1] * v->v [0] + a->m [1] [1] * v->v [1] + a->m [1] [2] * v->v [2];
    out->v [2] = a->m [0] [2] * v->v [0] + a->m [1] [2] * v->v [1] + a->m [2] [2] * v->v [2];
}

/* Accumulate the scatter matrix  */
static void
matrix33_scatter_vec3_sym (Matrix33f32 *out, const ChafaVec3f32 *vecs, gint n_vecs, const ChafaVec3f32 *average)
{
    gfloat xx = .0f, xy = .0f, xz = .0f, yy = .0f, yz = .0f, zz = .0f;
    gint i;

    for (i = 0; i < n_vecs; i++)
    {
        gfloat x = vecs [i].v [0] - average->v [0];
        gfloat y = vecs [i].v [1] - average->v [1];
        gfloat z = vecs [i].v [2] - average->v [2];

        xx += x * x;
        xy += x * y;
        xz += x * z;
        yy += y * y;
        yz += y * z;
        zz += z * z;
    }

    out->m [0] [0] = xx;
    out->m [0] [1] = xy;
    out->m [0] [2] = xz;
    out->m [1] [1] = yy;
    out->m [1] [2] = yz;
    out->m [2] [2] = zz;
}

/* Power iteration on the scatter matrix */
static gfloat
pca_converge (const Matrix33f32 *a, ChafaVec3f32 *eigenvector_out)
{
    ChafaVec3f32 r = CHAFA_VEC3F32_INIT (.11, .23, .51);
    gfloat eigenvalue = .0f;
    gint j;

    /* FIXME: r should probably be random, and we should try again
     * if we pick a bad one */

    chafa_vec3f32_normalize (&r, &r);

    for (j = 0; j < PCA_POWER_MAX_ITERATIONS; j++)
    {
        ChafaVec3f32 s, t;
        gfloat err;

        matrix33_mul_vec3_sym (&s, a, &r);
        eigenvalue = chafa_vec3f32_dot (&r, &s);

        if (eigenvalue <= .0f)
        {
            /* Identical vectors; bail out */
            eigenvalue = .0f;
            break;
        }

        chafa_vec3f32_mul_scalar (&t, &r, eigenvalue);
        chafa_vec3f32_sub (&t, &t, &s);
        err = chafa_vec3f32_get_magnitude (&t);

        chafa_vec3f32_normalize (&r, &s);

        if (err <= eigenvalue * PCA_POWER_REL_ERROR)
            break;
    }

    chafa_vec3f32_copy (eigenvector_out, &r);
    return eigenvalue;
}

/* Deflate the scatter matrix */
static void
pca_deflate (Matrix33f32 *a, gfloat eigenvalue, const ChafaVec3f32 *e)
{
    a->m [0] [0] -= eigenvalue * e->v [0] * e->v [0];
    a->m [0] [1] -= eigenvalue * e->v [0] * e->v [1];
    a->m [0] [2] -= eigenvalue * e->v [0] * e->v [2];
    a->m [1] [1] -= eigenvalue * e->v [1] * e->v [1];
    a->m [1] [2] -= eigenvalue * e->v [1] * e->v [2];
    a->m [2] [2] -= eigenvalue * e->v [2] * e->v [2];
}

/**
 * chafa_vec3f32_array_compute_pca:
 * @vecs_in: Input vector array
 * @n_vecs: Number of vectors in array
 * @n_components: Number of components to compute (1 to 3)
 * @eigenvectors_out: Pointer to array to store n_components eigenvectors in, or NULL
 * @eigenvalues_out: Pointer to array to store n_components eigenvalues in, or NULL
 * @average_out: Pointer to a vector to store array average (for centering), or NULL
 *
 * Compute principal components from an array of 3D vectors.
 **/
void
chafa_vec3f32_array_compute_pca (const ChafaVec3f32 *vecs_in, gint n_vecs,
                                 gint n_components,
                                 ChafaVec3f32 *eigenvectors_out,
                                 gfloat *eigenvalues_out,
                                 ChafaVec3f32 *average_out)
{
    ChafaVec3f32 average;
    Matrix33f32 a;
    gint i;

    g_assert (n_components >= 1 && n_components <= 3);

    chafa_vec3f32_average_array (&average, vecs_in, n_vecs);
    matrix33_scatter_vec3_sym (&a, vecs_in, n_vecs, &average);

    for (i = 0; ; )
    {
        ChafaVec3f32 e;
        gfloat eigenvalue;

        eigenvalue = pca_converge (&a, &e);

        if (eigenvectors_out)
            chafa_vec3f32_copy (&eigenvectors_out [i], &e);
        if (eigenvalues_out)
            eigenvalues_out [i] = eigenvalue;

        if (++i >= n_components)
            break;

        pca_deflate (&a, eigenvalue, &e);
    }

    if (average_out)
        chafa_vec3f32_copy (average_out, &average);
}
