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

#ifndef __CHAFA_PCA_H__
#define __CHAFA_PCA_H__

#include <glib.h>

G_BEGIN_DECLS

#define CHAFA_VEC3_INIT(x, y, z) { { (x), (y), (z) } }
#define CHAFA_VEC3_INIT_ZERO CHAFA_VEC3_INIT (0.0, 0.0, 0.0)

typedef struct
{
    gfloat v [3];
}
ChafaVec3;

void chafa_vec3_array_compute_pca (const ChafaVec3 *vecs_in, gint n_vecs,
                                   gint n_components,
                                   ChafaVec3 *eigenvectors_out,
                                   gfloat *eigenvalues_out,
                                   ChafaVec3 *average_out);

G_END_DECLS

#endif /* __CHAFA_PCA_H__ */
