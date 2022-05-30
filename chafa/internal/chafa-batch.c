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

#include <string.h>
#include <glib.h>

#include "chafa.h"
#include "internal/chafa-batch.h"

void
chafa_process_batches (gpointer ctx, GFunc batch_func, GFunc post_func, gint n_rows, gint n_batches, gint batch_unit)
{
    GThreadPool *thread_pool = NULL;
    ChafaBatchInfo *batches;
    gint n_threads;
    gint n_units;
    gfloat units_per_batch;
    gfloat ofs [2] = { .0f, .0f };
    gint i;

    g_assert (n_batches >= 1);
    g_assert (batch_unit >= 1);

    if (n_rows < 1)
        return;

    n_threads = MIN (chafa_get_n_actual_threads (), n_batches);
    n_units = (n_rows + batch_unit - 1) / batch_unit;
    units_per_batch = (gfloat) n_units / (gfloat) n_batches;

    batches = g_new0 (ChafaBatchInfo, n_batches);

    if (n_threads >= 2)
        thread_pool = g_thread_pool_new (batch_func,
                                         (gpointer) ctx,
                                         n_threads,
                                         FALSE,
                                         NULL);

    /* Divide work up into batches that are multiples of batch_unit, except
     * for the last one (if n_rows is not itself a multiple) */

    for (i = 0; i < n_batches; )
    {
        ChafaBatchInfo *batch;
        gint row_ofs [2];

        row_ofs [0] = ofs [0];

        do
        {
            ofs [1] += units_per_batch;
            row_ofs [1] = ofs [1];
        }
        while (row_ofs [0] == row_ofs [1]);

        row_ofs [0] *= batch_unit;
        row_ofs [1] *= batch_unit;

        if (row_ofs [1] > n_rows || i == n_batches - 1)
        {
            ofs [1] = n_rows + 0.5;
            row_ofs [1] = n_rows;
        }

        if (row_ofs [0] >= row_ofs [1])
        {
            /* Save the number of batches actually produced to use in
             * post_func loop later. */
            n_batches = i;
            break;
        }

        batch = &batches [i++];
        batch->first_row = row_ofs [0];
        batch->n_rows = row_ofs [1] - row_ofs [0];

#if 0
        g_printerr ("Batch %d: %04d rows\n", i, batch->n_rows);
#endif

        if (n_threads >= 2)
        {
            g_thread_pool_push (thread_pool, batch, NULL);
        }
        else
        {
            batch_func (batch, ctx);
        }

        ofs [0] = ofs [1];
    }

    if (n_threads >= 2)
    {
        /* Wait for threads to finish */
        g_thread_pool_free (thread_pool, FALSE, TRUE);
    }

    if (post_func)
    {
        for (i = 0; i < n_batches; i++)
        {
            ((void (*)(ChafaBatchInfo *, gpointer)) post_func) (&batches [i], ctx);
        }
    }

    g_free (batches);
}
