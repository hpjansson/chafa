/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2025 Hans Petter Jansson
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

#ifndef __CHAFA_NOISE_H__
#define __CHAFA_NOISE_H__

G_BEGIN_DECLS

#define CHAFA_NOISE_TEXTURE_DIM 64
#define CHAFA_NOISE_TEXTURE_N_CHANNELS 3
#define CHAFA_NOISE_TEXTURE_N_PIXELS (CHAFA_NOISE_TEXTURE_DIM * CHAFA_NOISE_TEXTURE_DIM)
#define CHAFA_NOISE_TEXTURE_ARRAY_LEN (CHAFA_NOISE_TEXTURE_N_PIXELS * CHAFA_NOISE_TEXTURE_N_CHANNELS)

gint *chafa_gen_noise_matrix (gfloat magnitude);

G_END_DECLS

#endif /* __CHAFA_NOISE_H__ */
