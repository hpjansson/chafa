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

#ifndef __CHAFA_PASSTHROUGH_ENCODER_H__
#define __CHAFA_PASSTHROUGH_ENCODER_H__

#include "chafa.h"

G_BEGIN_DECLS

typedef struct
{
    ChafaPassthrough mode;
    ChafaTermInfo *term_info;
    GString *out;
    gint packet_size;
}
ChafaPassthroughEncoder;


void chafa_passthrough_encoder_begin (ChafaPassthroughEncoder *ptenc,
                                      ChafaPassthrough passthrough,
                                      ChafaTermInfo *term_info,
                                      GString *out_str);
void chafa_passthrough_encoder_end (ChafaPassthroughEncoder *ptenc);

void chafa_passthrough_encoder_append (ChafaPassthroughEncoder *ptenc,
                                       const gchar *in);
void chafa_passthrough_encoder_append_len (ChafaPassthroughEncoder *ptenc,
                                           const gchar *in,
                                           gint len);
void chafa_passthrough_encoder_flush (ChafaPassthroughEncoder *ptenc);
void chafa_passthrough_encoder_reset (ChafaPassthroughEncoder *ptenc);

G_END_DECLS

#endif /* __CHAFA_PASSTHROUGH_ENCODER_H__ */
