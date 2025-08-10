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

#include <string.h>
#include <glib.h>

#include "chafa.h"
#include "internal/chafa-batch.h"

static gint chafa_batch_n_threads_global;

static gint
allocate_threads (gint max_threads, gint n_batches)
{
    gint prev_n_threads;
    gint n_threads;

    /* GThreadPool supports sharing threads between pools, but there is no
     * mechanism to manage the global thread count. If the batch API is being
     * called from multiple threads, we risk creating N * M workers, which
     * can result in hundreds of threads.
     *
     * Therefore, we maintain a global count of active threads and allocate
     * each caller's allotment from that. The minimum allocation is 1 thread,
     * in which case the operation is performed in the calling thread. Single-
     * threaded tasks are allowed to overshoot the maximum, so maximum
     * concurrency will be N + M - 1, where N is the number of calling threads
     * and M is the requisition from chafa_get_n_actual_threads(). For typical
     * workloads, average concurrency will likely be close to M. */

    prev_n_threads = 0;
    n_threads = MIN (max_threads, n_batches);

    /* Geometric backoff: The first iteration adds to the global, subsequent
     * iterations subtract until we're happy. */

    for (;;)
    {
        gint next_n_global = n_threads
            + g_atomic_int_add (&chafa_batch_n_threads_global,
                                n_threads - prev_n_threads);
        if (next_n_global <= max_threads || n_threads == 1)
            break;

        prev_n_threads = n_threads;
        n_threads /= 2;
    }

    return n_threads;
}

static void
deallocate_threads (gint n_threads)
{
    g_atomic_int_add (&chafa_batch_n_threads_global, -n_threads);
}

void
chafa_process_batches (gpointer ctx, GFunc batch_func, GFunc post_func, gint n_rows, gint n_batches, gint batch_unit)
{
    GThreadPool *thread_pool = NULL;
    ChafaBatchInfo *batches;
    gint max_threads;
    gint n_threads;
    gint n_units;
    gfloat units_per_batch;
    gfloat ofs [2] = { .0f, .0f };
    gint i;

    g_assert (n_batches >= 1);
    g_assert (batch_unit >= 1);

    if (n_rows < 1)
        return;

    max_threads = chafa_get_n_actual_threads ();
    n_threads = allocate_threads (max_threads, n_batches);

    n_units = (n_rows + batch_unit - 1) / batch_unit;
    units_per_batch = (gfloat) n_units / (gfloat) n_batches;

    batches = g_new0 (ChafaBatchInfo, n_batches);

    if (n_threads >= 2)
    {
        thread_pool = g_thread_pool_new (batch_func,
                                         (gpointer) ctx,
                                         n_threads,
                                         FALSE,
                                         NULL);
    }

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
    deallocate_threads (n_threads);
}
