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
#include <stdlib.h>
#include <string.h>

#include <chafa.h>
#include "chicle-util.h"

#define CHAR_BUF_SIZE 1024
#define ROWSTRIDE_ALIGN 16

#define PAD_TO_N(p, n) (((p) + ((n) - 1)) & ~((unsigned) (n) - 1))
#define ROWSTRIDE_PAD(rowstride) (PAD_TO_N ((rowstride), (ROWSTRIDE_ALIGN)))

static void
transform (const guchar *src, gint src_pixstride, gint src_rowstride,
           guchar *dest, gint dest_pixstride, gint dest_rowstride,
           gint src_width, gint src_height, gint pixsize)
{
    const guchar *src_row = src;
    const guchar *src_row_end = src + src_pixstride * src_width;
    const guchar *src_end = src + src_rowstride * src_height;
    guchar *dest_row = dest;

    while (src_row != src_end)
    {
        src = src_row;
        dest = dest_row;

        while (src != src_row_end)
        {
            memcpy (dest, src, pixsize);
            src += src_pixstride;
            dest += dest_pixstride;
        }

        src_row += src_rowstride;
        src_row_end += src_rowstride;
        dest_row += dest_rowstride;
    }
}

ChicleRotationType
chicle_invert_rotation (ChicleRotationType rot)
{
    switch (rot)
    {
        case CHICLE_ROTATION_90:
            rot = CHICLE_ROTATION_270;
            break;
        case CHICLE_ROTATION_270:
            rot = CHICLE_ROTATION_90;
            break;
        default:
            break;
    }

    return rot;
}

void
chicle_rotate_image (gpointer *src, guint *width, guint *height, guint *rowstride,
                     guint n_channels, ChicleRotationType rot)
{
    gint src_width, src_height;
    gint src_pixstride, src_rowstride;
    guchar *dest, *dest_start;
    gint dest_width, dest_height;
    gint dest_pixstride, dest_rowstride, dest_trans_rowstride;

    g_assert (n_channels == 3 || n_channels == 4);

    if (rot <= CHICLE_ROTATION_NONE
        || rot == CHICLE_ROTATION_0
        || rot >= CHICLE_ROTATION_UNDEFINED)
        return;

    src_width = *width;
    src_height = *height;
    src_pixstride = n_channels;
    src_rowstride = *rowstride;

    switch (rot)
    {
        case CHICLE_ROTATION_90:
        case CHICLE_ROTATION_90_MIRROR:
        case CHICLE_ROTATION_270:
        case CHICLE_ROTATION_270_MIRROR:
            dest_width = src_height;
            dest_height = src_width;
            break;
        case CHICLE_ROTATION_0_MIRROR:
        case CHICLE_ROTATION_180:
        case CHICLE_ROTATION_180_MIRROR:
            dest_width = src_width;
            dest_height = src_height;
            break;
        default:
            g_assert_not_reached ();
    }

    dest_rowstride = ROWSTRIDE_PAD (dest_width * n_channels);
    dest = g_malloc (dest_rowstride * dest_height);

    switch (rot)
    {
        case CHICLE_ROTATION_0_MIRROR:
            dest_pixstride = -n_channels;
            dest_trans_rowstride = dest_rowstride;
            dest_start = dest + ((dest_width - 1) * n_channels);
            break;
        case CHICLE_ROTATION_90:
            dest_pixstride = dest_rowstride;
            dest_trans_rowstride = -n_channels;
            dest_start = dest + ((dest_width - 1) * n_channels);
            break;
        case CHICLE_ROTATION_90_MIRROR:
            dest_pixstride = -dest_rowstride;
            dest_trans_rowstride = -n_channels;
            dest_start = dest + ((dest_height - 1) * dest_rowstride) + ((dest_width - 1) * n_channels);
            break;
        case CHICLE_ROTATION_180:
            dest_pixstride = -n_channels;
            dest_trans_rowstride = -dest_rowstride;
            dest_start = dest + ((dest_height - 1) * dest_rowstride) + ((dest_width - 1) * n_channels);
            break;
        case CHICLE_ROTATION_180_MIRROR:
            dest_pixstride = n_channels;
            dest_trans_rowstride = -dest_rowstride;
            dest_start = dest + ((dest_height - 1) * dest_rowstride);
            break;
        case CHICLE_ROTATION_270:
            dest_pixstride = -dest_rowstride;
            dest_trans_rowstride = n_channels;
            dest_start = dest + ((dest_height - 1) * dest_rowstride);
            break;
        case CHICLE_ROTATION_270_MIRROR:
            dest_pixstride = dest_rowstride;
            dest_trans_rowstride = n_channels;
            dest_start = dest;
            break;
        default:
            g_assert_not_reached ();
    }

    transform (*src, src_pixstride, src_rowstride,
               dest_start, dest_pixstride, dest_trans_rowstride,
               src_width, src_height, n_channels);

    g_free (*src);

    *src = dest;
    *width = dest_width;
    *height = dest_height;
    *rowstride = dest_rowstride;
}

void
chicle_flatten_cntrl_inplace (gchar *str)
{
    gchar *p;

    for (p = str; *p; p = g_utf8_next_char (p))
    {
        gunichar c = g_utf8_get_char (p);
        if (g_unichar_iscntrl (c))
            *p = '?';
    }
}

gchar *
chicle_ellipsize_string (const gchar *str, gint len_max, gboolean use_unicode)
{
    const gchar *p;
    gchar *ellipsized;
    gint i, j;

    if (!str || len_max <= 0)
        return g_strdup ("");

    for (p = str, i = 0; *p && i < len_max; i++)
    {
        p = g_utf8_next_char (p);
    }

    j = p - str;
    if (*p && !*g_utf8_next_char (p))
        return g_strdup (str);

    ellipsized = g_malloc (j + 7);
    memcpy (ellipsized, str, j);

    if (*p)
    {
        if (use_unicode)
        {
            /* U+2026, Unicode ellipsis */
            j += g_unichar_to_utf8 (0x2026, ellipsized + j);
        }
        else
        {
            *(ellipsized + (j++)) = '>';
        }
    }
    *(ellipsized + j) = '\0';

    return ellipsized;
}

gchar *
chicle_path_get_ellipsized_basename (const gchar *path, gint len_max, gboolean use_unicode)
{
    gchar *basename;
    gchar *ellipsized;

    if (len_max <= 0)
        return g_strdup ("");
    if (!path)
        return g_strdup ("?");

    basename = g_path_get_basename (path);
    chicle_flatten_cntrl_inplace (basename);
    ellipsized = chicle_ellipsize_string (basename, len_max, use_unicode);

    g_free (basename);
    return ellipsized;
}

void
chicle_print_rep_char (ChafaTerm *term, gchar c, gint n)
{
    gchar buf_s [CHAR_BUF_SIZE];
    gchar *buf_m = NULL;
    gchar *buf = buf_s;
    gint i;

    if (n > CHAR_BUF_SIZE)
    {
        buf_m = g_malloc (n);
        buf = buf_m;
    }

    for (i = 0; i < n; i++)
        buf [i] = c;

    chafa_term_write (term, buf, n);
    g_free (buf_m);
}

static void
print_linked_label (ChafaTerm *term, const gchar *path, const gchar *label,
                    gboolean link_label)
{
    gchar *link_str = NULL;

    if (link_label)
    {
        gchar *abs_path = NULL;

        if (!g_path_is_absolute (path))
            abs_path = g_canonicalize_filename (path, NULL);

        link_str = g_strdup_printf ("file://%s%s",
                                    g_get_host_name (),
                                    abs_path ? abs_path : path);
        g_free (abs_path);
    }

    if (link_str)
    {
        chafa_term_print_seq (term, CHAFA_TERM_SEQ_BEGIN_HYPERLINK, -1);
        chafa_term_write (term, link_str, strlen (link_str));
        chafa_term_print_seq (term, CHAFA_TERM_SEQ_BEGIN_HYPERLINK_ANCHOR, -1);
        chafa_term_write (term, label, strlen (label));
        chafa_term_print_seq (term, CHAFA_TERM_SEQ_END_HYPERLINK, -1);
        g_free (link_str);
    }
    else
    {
        chafa_term_write (term, label, strlen (label));
    }
}

void
chicle_path_print_label (ChafaTerm *term, const gchar *path, ChafaAlign halign,
                         gint field_width, gboolean use_unicode, gboolean link_label)
{
    gchar *label = chicle_path_get_ellipsized_basename (path, field_width - 1, use_unicode);
    gchar *sanitized_path;
    gint label_len = g_utf8_strlen (label, -1);

    sanitized_path = g_strdup (path);
    chicle_flatten_cntrl_inplace (sanitized_path);

    if (halign == CHAFA_ALIGN_START)
    {
        print_linked_label (term, sanitized_path, label, link_label);
        chicle_print_rep_char (term, ' ', field_width - label_len);
    }
    else if (halign == CHAFA_ALIGN_CENTER)
    {
        chicle_print_rep_char (term, ' ', (field_width - label_len) / 2);
        print_linked_label (term, sanitized_path, label, link_label);
        chicle_print_rep_char (term, ' ', (field_width - label_len + 1) / 2);
    }
    else
    {
        chicle_print_rep_char (term, ' ', field_width - label_len);
        print_linked_label (term, sanitized_path, label, link_label);
    }

    g_free (label);
    g_free (sanitized_path);
}
