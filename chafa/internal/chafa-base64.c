/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2021-2022 Hans Petter Jansson
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

#include <string.h>
#include <glib.h>

#include "chafa.h"
#include "internal/chafa-base64.h"

static const gchar base64_dict [] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

void
chafa_base64_init (ChafaBase64 *base64)
{
    memset (base64, 0, sizeof (*base64));
}

void
chafa_base64_deinit (ChafaBase64 *base64)
{
    memset (base64, 0, sizeof (*base64));
    base64->buf_len = -1;
}

static void
encode_3_bytes (GString *gs_out, guint32 bytes)
{
    g_string_append_c (gs_out, base64_dict [(bytes >> (3 * 6)) & 0x3f]);
    g_string_append_c (gs_out, base64_dict [(bytes >> (2 * 6)) & 0x3f]);
    g_string_append_c (gs_out, base64_dict [(bytes >> (1 * 6)) & 0x3f]);
    g_string_append_c (gs_out, base64_dict [bytes & 0x3f]);
}

void
chafa_base64_encode (ChafaBase64 *base64, GString *gs_out, gconstpointer in, gint in_len)
{
    const guint8 *in_u8 = in;
    const guint8 *end_u8 = in_u8 + in_len;
    guint32 r;

    if (base64->buf_len + in_len < 3)
    {
        memcpy (base64->buf + base64->buf_len, in_u8, in_len);
        base64->buf_len += in_len;
        return;
    }

    if (base64->buf_len == 1)
    {
        r = (base64->buf [0] << 16) | (in_u8 [0] << 8) | in_u8 [1];
        in_u8 += 2;
        encode_3_bytes (gs_out, r);
    }
    else if (base64->buf_len == 2)
    {
        r = (base64->buf [0] << 16) | (base64->buf [1] << 8) | in_u8 [0];
        in_u8++;
        encode_3_bytes (gs_out, r);
    }

    base64->buf_len = 0;

    while (end_u8 - in_u8 >= 3)
    {
        r = (in_u8 [0] << 16) | (in_u8 [1] << 8) | in_u8 [2];
        encode_3_bytes (gs_out, r);
        in_u8 += 3;
    }

    while (end_u8 - in_u8 > 0)
    {
        base64->buf [base64->buf_len++] = *(in_u8++);
    }
}

void
chafa_base64_encode_end (ChafaBase64 *base64, GString *gs_out)
{
    if (base64->buf_len == 1)
    {
        g_string_append_c (gs_out, base64_dict [base64->buf [0] >> 2]);
        g_string_append_c (gs_out, base64_dict [(base64->buf [0] << 4) & 0x30]);
        g_string_append (gs_out, "==");
    }
    else if (base64->buf_len == 2)
    {
        g_string_append_c (gs_out, base64_dict [base64->buf [0] >> 2]);
        g_string_append_c (gs_out, base64_dict [((base64->buf [0] << 4) | (base64->buf [1] >> 4)) & 0x3f]);
        g_string_append_c (gs_out, base64_dict [base64->buf [1] & 0x0f]);
        g_string_append_c (gs_out, '=');
    }
}
