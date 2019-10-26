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

#include <stdlib.h>  /* abs */
#include <math.h>  /* pow, cbrt, log, sqrt, atan2, cos, sin */
#include "chafa.h"
#include "internal/chafa-private.h"

#define DEBUG(x)

/* FIXME: Refactor old color selection code into a common palette framework */

static gint
find_dominant_channel (gconstpointer pixels, gint n_pixels)
{
    const guint8 *p = pixels;
    guint8 min [4] = { G_MAXUINT8, G_MAXUINT8, G_MAXUINT8, G_MAXUINT8 };
    guint8 max [4] = { 0, 0, 0, 0 };
    guint16 diff [4];
    gint best;
    gint i;

    for (i = 0; i < n_pixels; i++)
    {
        /* This should yield branch-free code where possible */
        min [0] = MIN (min [0], *p);
        max [0] = MAX (max [0], *p);
        p++;
        min [1] = MIN (min [1], *p);
        max [1] = MAX (max [1], *p);
        p++;
        min [2] = MIN (min [2], *p);
        max [2] = MAX (max [2], *p);

        /* Skip alpha */
        p += 2;
    }

#if 1
    /* Multipliers for luminance */
    diff [0] = (max [0] - min [0]) * 30;
    diff [1] = (max [1] - min [1]) * 59;
    diff [2] = (max [2] - min [2]) * 11;
#else
    diff [0] = (max [0] - min [0]);
    diff [1] = (max [1] - min [1]);
    diff [2] = (max [2] - min [2]);
#endif

    /* If there are ties, prioritize thusly: G, R, B */

    best = 1;
    if (diff [0] > diff [best])
        best = 0;
    if (diff [2] > diff [best])
        best = 2;

    return best;
}

static int
compare_rgba_0 (gconstpointer a, gconstpointer b)
{
    const guint8 *ab = a;
    const guint8 *bb = b;
    gint ai = ab [0];
    gint bi = bb [0];

    return ai - bi;
}

static int
compare_rgba_1 (gconstpointer a, gconstpointer b)
{
    const guint8 *ab = a;
    const guint8 *bb = b;
    gint ai = ab [1];
    gint bi = bb [1];

    return ai - bi;
}

static int
compare_rgba_2 (gconstpointer a, gconstpointer b)
{
    const guint8 *ab = a;
    const guint8 *bb = b;
    gint ai = ab [2];
    gint bi = bb [2];

    return ai - bi;
}

static int
compare_rgba_3 (gconstpointer a, gconstpointer b)
{
    const guint8 *ab = a;
    const guint8 *bb = b;
    gint ai = ab [3];
    gint bi = bb [3];

    return ai - bi;
}

static void
sort_by_channel (gpointer pixels, gint n_pixels, gint ch)
{
    switch (ch)
    {
        case 0:
            qsort (pixels, n_pixels, sizeof (guint32), compare_rgba_0);
            break;
        case 1:
            qsort (pixels, n_pixels, sizeof (guint32), compare_rgba_1);
            break;
        case 2:
            qsort (pixels, n_pixels, sizeof (guint32), compare_rgba_2);
            break;
        case 3:
            qsort (pixels, n_pixels, sizeof (guint32), compare_rgba_3);
            break;
        default:
            g_assert_not_reached ();
    }
}

static void
average_pixels (guint8 *pixels, gint first_ofs, gint n_pixels, ChafaColor *col_out)
{
    guint8 *p = pixels + first_ofs * sizeof (guint32);
    guint8 *pixels_end;
    gint ch [3] = { 0 };

    pixels_end = p + n_pixels * sizeof (guint32);

    for ( ; p < pixels_end; p += 4)
    {
        ch [0] += p [0];
        ch [1] += p [1];
        ch [2] += p [2];
    }

    col_out->ch [0] = (ch [0] + n_pixels / 2) / n_pixels;
    col_out->ch [1] = (ch [1] + n_pixels / 2) / n_pixels;
    col_out->ch [2] = (ch [2] + n_pixels / 2) / n_pixels;
}

static void
median_pixels (guint8 *pixels, gint first_ofs, gint n_pixels, ChafaColor *col_out)
{
    guint8 *p = pixels + (first_ofs + n_pixels / 2) * sizeof (guint32);
    col_out->ch [0] = p [0];
    col_out->ch [1] = p [1];
    col_out->ch [2] = p [2];
}

static void
average_pixels_weighted_by_deviation (guint8 *pixels, gint first_ofs, gint n_pixels,
                                      ChafaColor *col_out)
{
    guint8 *p = pixels + first_ofs * sizeof (guint32);
    guint8 *pixels_end;
    gint ch [3] = { 0 };
    ChafaColor median;
    guint sum = 0;

    median_pixels (pixels, first_ofs, n_pixels, &median);

    pixels_end = p + n_pixels * sizeof (guint32);

    for ( ; p < pixels_end; p += 4)
    {
        ChafaColor t;
        guint diff;

        t.ch [0] = p [0];
        t.ch [1] = p [1];
        t.ch [2] = p [2];

        diff = chafa_color_diff_fast (&median, &t);
        diff /= 256;
        diff += 1;

        ch [0] += p [0] * diff;
        ch [1] += p [1] * diff;
        ch [2] += p [2] * diff;

        sum += diff;
    }

    col_out->ch [0] = (ch [0] + sum / 2) / sum;
    col_out->ch [1] = (ch [1] + sum / 2) / sum;
    col_out->ch [2] = (ch [2] + sum / 2) / sum;
}

static void
pick_box_color (gpointer pixels, gint first_ofs, gint n_pixels, ChafaColor *color_out)
{
    average_pixels_weighted_by_deviation (pixels, first_ofs, n_pixels, color_out);
}

static void
median_cut_once (gpointer pixels,
                 gint first_ofs, gint n_pixels,
                 ChafaColor *color_out)
{
    guint8 *p = pixels;
    gint dominant_ch;

    g_assert (n_pixels > 0);

    dominant_ch = find_dominant_channel (p + first_ofs * sizeof (guint32), n_pixels);
    sort_by_channel (p + first_ofs * sizeof (guint32), n_pixels, dominant_ch);

    pick_box_color (pixels, first_ofs, n_pixels, color_out);
}

static void
median_cut (ChafaPalette *pal, gpointer pixels,
            gint first_ofs, gint n_pixels,
            gint first_col, gint n_cols)
{
    guint8 *p = pixels;
    gint dominant_ch;

    g_assert (n_pixels > 0);
    g_assert (n_cols > 0);

    dominant_ch = find_dominant_channel (p + first_ofs * sizeof (guint32), n_pixels);
    sort_by_channel (p + first_ofs * sizeof (guint32), n_pixels, dominant_ch);

    if (n_cols == 1)
    {
        pick_box_color (pixels, first_ofs, n_pixels, &pal->colors [first_col].col [CHAFA_COLOR_SPACE_RGB]);
#if 0
        g_printerr ("mofs=%d+%d  ", first_ofs, n_pixels);
#endif
        return;
    }

    median_cut (pal, pixels,
                first_ofs,
                n_pixels / 2,
                first_col,
                n_cols / 2);

    median_cut (pal, pixels,
                first_ofs + (n_pixels / 2),
                n_pixels - (n_pixels / 2),
                first_col + (n_cols / 2),
                n_cols - (n_cols / 2));
}

static gint
dominant_diff (guint8 *p1, guint8 *p2)
{
    gint diff [3];

    diff [0] = abs (p2 [0] - (gint) p1 [0]);
    diff [1] = abs (p2 [1] - (gint) p1 [1]);
    diff [2] = abs (p2 [2] - (gint) p1 [2]);

    return MAX (diff [0], MAX (diff [1], diff [2]));
}

static void
diversity_pass (ChafaPalette *pal, gpointer pixels,
                gint n_pixels,
                gint first_col, gint n_cols)
{
    guint8 *p = pixels;
    gint step = n_pixels / 128;
    gint i, n, c;
    guint8 done [128] = { 0 };

    for (c = 0; c < n_cols; c++)
    {
        gint best_box = 0;
        gint best_diff = 0;

        for (i = 0, n = 0; i < 128; i++)
        {
            gint diff = dominant_diff (p + 4 * n, p + 4 * (n + step - 1));

#if 0
            g_printerr ("ofs=%d,%d  ", n, diff);
#endif
            if (diff > best_diff && !done [i])
            {
                best_diff = diff;
                best_box = i;
            }

            n += step;
        }
#if 1
        median_cut_once (pixels, best_box * step, step / 2,
                         &pal->colors [first_col + c].col [CHAFA_COLOR_SPACE_RGB]);
        c++;
        if (c >= n_cols)
            break;
        median_cut_once (pixels, best_box * step + step / 2, step / 2,
                         &pal->colors [first_col + c].col [CHAFA_COLOR_SPACE_RGB]);
#else
        median_cut_once (pixels, best_box * step, step,
                         &pal->colors [first_col + c].col [CHAFA_COLOR_SPACE_RGB]);
#endif

        done [best_box] = 1;
    }
}

static void
gen_din99d_color_space (ChafaPalette *palette)
{
    gint i;

    for (i = 0; i < palette->n_colors; i++)
    {
        chafa_color_rgb_to_din99d (&palette->colors [i].col [CHAFA_COLOR_SPACE_RGB],
                                   &palette->colors [i].col [CHAFA_COLOR_SPACE_DIN99D]);
    }
}

/* bit_index is in the range [0..15]. MSB is 15. */
static guint8
get_color_branch (const ChafaColor *col, gint8 bit_index)
{
    return ((col->ch [0] >> bit_index) & 1)
        | (((col->ch [1] >> bit_index) & 1) << 1)
        | (((col->ch [2] >> bit_index) & 1) << 2);
}

/* bit_index is in the range [0..15]. MSB is 15. */
static guint8
get_prefix_branch (const ChafaPaletteOctNode *node, gint8 bit_index)
{
    return ((node->prefix [0] >> bit_index) & 1)
        | (((node->prefix [1] >> bit_index) & 1) << 1)
        | (((node->prefix [2] >> bit_index) & 1) << 2);
}

static guint16
branch_bit_to_prefix_mask (gint8 branch_bit)
{
    return 0xffffU << (branch_bit + 1);
}

static gboolean
prefix_match (const ChafaPaletteOctNode *node, const ChafaColor *col)
{
    guint16 mask;

    /* FIXME: Use bitwise ops to reduce branching */

    mask = branch_bit_to_prefix_mask (node->branch_bit);
    return (node->prefix [0] & mask) == (col->ch [0] & mask)
        && (node->prefix [1] & mask) == (col->ch [1] & mask)
        && (node->prefix [2] & mask) == (col->ch [2] & mask);
}

static gint16
find_colors_branch_bit (const ChafaColor *col_a, const ChafaColor *col_b)
{
    gint i, j;

    for (i = 15; i >= 0; i--)
    {
        for (j = 0; j < 3; j++)
        {
            if (((col_a->ch [j] ^ col_b->ch [j]) >> i) & 1)
                goto found;
        }
    }

found:
    return i;
}

static gint16
find_prefix_color_branch_bit (ChafaPaletteOctNode *node, const ChafaColor *col, guint16 mask)
{
    guint col_prefix [3];
    gint i, j;

    for (i = 0; i < 3; i++)
        col_prefix [i] = col->ch [i] & mask;

    for (i = 15; i >= 0; i--)
    {
        for (j = 0; j < 3; j++)
        {
            if (((node->prefix [j] ^ col_prefix [j]) >> i) & 1)
                goto found;
        }
    }

found:
    return i;
}

static void
oct_tree_clear_node (ChafaPaletteOctNode *node)
{
    gint i;

    node->branch_bit = 15;
    node->n_children = 0;

    for (i = 0; i < 8; i++)
        node->child_index [i] = CHAFA_OCT_TREE_INDEX_NULL;
}

static gint
oct_tree_insert_color (ChafaPalette *palette, ChafaColorSpace color_space, gint16 color_index,
                       gint16 parent_index, gint16 node_index)
{
    ChafaColor *col;
    ChafaPaletteOctNode *node;
    guint16 prefix_mask;

    DEBUG (g_printerr ("%d, %d\n", parent_index, node_index));

    g_assert (color_index >= 0 && color_index < 256);
    g_assert (parent_index == CHAFA_OCT_TREE_INDEX_NULL || (parent_index >= 256 && parent_index < 512));
    g_assert (node_index >= 256 && node_index < 512);
    g_assert (parent_index != node_index);

    col = &palette->colors [color_index].col [color_space];
    node = &palette->oct_tree [color_space] [node_index - 256];
    prefix_mask = branch_bit_to_prefix_mask (node->branch_bit);

    /* FIXME: Use bitwise ops to reduce branching */
    if (((col->ch [0] & prefix_mask) != node->prefix [0])
        || ((col->ch [1] & prefix_mask) != node->prefix [1])
        || ((col->ch [2] & prefix_mask) != node->prefix [2]))
    {
        gint16 new_index;
        ChafaPaletteOctNode *new_node;
        guint16 new_mask;

        /* Insert a new node between parent and this one */

        DEBUG (g_printerr ("1\n"));

        new_index = palette->oct_tree_first_free [color_space]++;
        new_node = &palette->oct_tree [color_space] [new_index - 256];
        oct_tree_clear_node (new_node);

        new_node->branch_bit = find_prefix_color_branch_bit (node, col, prefix_mask);
        g_assert (new_node->branch_bit >= 0);
        g_assert (new_node->branch_bit < 16);
        g_assert (new_node->branch_bit > node->branch_bit);

        new_mask = branch_bit_to_prefix_mask (new_node->branch_bit);
        new_node->prefix [0] = col->ch [0] & new_mask;
        new_node->prefix [1] = col->ch [1] & new_mask;
        new_node->prefix [2] = col->ch [2] & new_mask;

        new_node->child_index [get_prefix_branch (node, new_node->branch_bit)] = node_index;
        new_node->child_index [get_color_branch (col, new_node->branch_bit)] = color_index;

        new_node->n_children = 2;

        if (parent_index == CHAFA_OCT_TREE_INDEX_NULL)
        {
            DEBUG (g_printerr ("2\n"));
            palette->oct_tree_root [color_space] = new_index;
        }
        else
        {
            ChafaPaletteOctNode *parent_node = &palette->oct_tree [color_space] [parent_index - 256];
            gint i;

            DEBUG (g_printerr ("3\n"));

            /* FIXME: Could use direct index here */
            for (i = 0; i < 8; i++)
            {
                if (parent_node->child_index [i] == node_index)
                    break;
            }

            /* Make sure we found the index to replace */
            g_assert (i < 8);

            parent_node->child_index [i] = new_index;
        }
    }
    else
    {
        gint16 child_index;
        guint8 branch;

        /* Matching prefix */

        DEBUG (g_printerr ("4\n"));

        branch = get_color_branch (col, node->branch_bit);
        child_index = node->child_index [branch];

        if (child_index == CHAFA_OCT_TREE_INDEX_NULL)
        {
            DEBUG (g_printerr ("5\n"));
            node->child_index [branch] = color_index;
            node->n_children++;
        }
        else if (child_index < 256)
        {
            gint16 new_index;
            ChafaPaletteOctNode *new_node;
            guint16 new_mask;
            const ChafaColor *old_col;
            gint8 old_branch_bit = node->branch_bit;

            DEBUG (g_printerr ("6\n"));

            old_col = &palette->colors [child_index].col [color_space];

            /* Does the color already exist? */

            if (col->ch [0] == old_col->ch [0]
                && col->ch [1] == old_col->ch [1]
                && col->ch [2] == old_col->ch [2])
            {
                DEBUG (g_printerr ("7\n"));
                return 0;
            }

            if (node->n_children == 1)
            {
                /* Root node went from one to two children */

                DEBUG (g_printerr ("8\n"));
                new_index = node_index;
                new_node = node;

                /* Branch bit may change; reinsert both children */
                node->child_index [branch] = CHAFA_OCT_TREE_INDEX_NULL;
            }
            else
            {
                /* Create a new leaf node */

                DEBUG (g_printerr ("9\n"));
                new_index = palette->oct_tree_first_free [color_space]++;
                new_node = &palette->oct_tree [color_space] [new_index - 256];
                oct_tree_clear_node (new_node);

                node->child_index [branch] = new_index;
            }

            new_node->branch_bit = find_colors_branch_bit (old_col, col);
            g_assert (get_color_branch (old_col, new_node->branch_bit) != get_color_branch (col, new_node->branch_bit));
            g_assert (new_node->branch_bit >= 0);
            g_assert (new_node->branch_bit < 16);
            g_assert (new_node->n_children < 2 || new_node->branch_bit < old_branch_bit);

            new_mask = branch_bit_to_prefix_mask (new_node->branch_bit);
            new_node->prefix [0] = col->ch [0] & new_mask;
            new_node->prefix [1] = col->ch [1] & new_mask;
            new_node->prefix [2] = col->ch [2] & new_mask;

            new_node->child_index [get_color_branch (old_col, new_node->branch_bit)] = child_index;
            new_node->child_index [get_color_branch (col, new_node->branch_bit)] = color_index;

            new_node->n_children = 2;
        }
        else
        {
            DEBUG (g_printerr ("10\n"));
            /* Recurse into existing subtree */
            return oct_tree_insert_color (palette, color_space, color_index,
                                          node_index, child_index);
        }
    }

    return 1;
}

static void
gen_oct_tree (ChafaPalette *palette, ChafaColorSpace color_space)
{
    ChafaPaletteOctNode *node;
    gint n_colors = 0;
    gint i;

    g_assert (palette->n_colors > 0);

    palette->oct_tree_root [color_space] = 256;
    palette->oct_tree_first_free [color_space] = 257;

    node = &palette->oct_tree [color_space] [0];

    oct_tree_clear_node (node);
    node->prefix [0] = 0;
    node->prefix [1] = 0;
    node->prefix [2] = 0;

    for (i = 1; i < palette->n_colors; i++)
    {
        n_colors += oct_tree_insert_color (palette, color_space, i, CHAFA_OCT_TREE_INDEX_NULL,
                                           palette->oct_tree_root [color_space]);
    }

    DEBUG (g_printerr ("Indexed %d colors. Root branch bit = %d.\n",
                       n_colors,
                       palette->oct_tree [color_space] [palette->oct_tree_root [color_space] - 256].branch_bit));
}

#define N_SAMPLES 32768

static gint
extract_samples (gconstpointer pixels, gpointer pixels_out, gint n_pixels, gint n_samples,
                 gint alpha_threshold)
{
    const guint32 *p = pixels;
    guint32 *p_out = pixels_out;
    gint step;
    gint i;

    step = (n_pixels / n_samples) + 1;
    step = MAX (step, 1);

    for (i = 0; i < n_pixels; i += step)
    {
        gint alpha = p [i] >> 24;
        if (alpha < alpha_threshold)
            continue;
        *(p_out++) = p [i];
    }

    return ((ptrdiff_t) p_out - (ptrdiff_t) pixels_out) / 4;
}

static void
clean_up (ChafaPalette *palette_out)
{
    gint i, j;
    gint best_diff = G_MAXINT;
    gint best_pair = 1;

    /* Reserve 0th pen for transparency and move colors up.
     * Eliminate duplicates and colors that would be the same in
     * sixel representation (0..100). */

#if 0
    g_printerr ("Colors before: %d\n", palette_out->n_colors);
#endif

    for (i = 1, j = 1; i < palette_out->n_colors; i++)
    {
        ChafaColor *a, *b;
        gint diff, t;

        a = &palette_out->colors [j - 1].col [CHAFA_COLOR_SPACE_RGB];
        b = &palette_out->colors [i].col [CHAFA_COLOR_SPACE_RGB];

        /* Dividing by 256 is strictly not correct, but it's close enough for
         * comparison purposes, and a lot faster too. */
        t = (gint) (a->ch [0] * 100) / 256 - (gint) (b->ch [0] * 100) / 256;
        diff = t * t;
        t = (gint) (a->ch [1] * 100) / 256 - (gint) (b->ch [1] * 100) / 256;
        diff += t * t;
        t = (gint) (a->ch [2] * 100) / 256 - (gint) (b->ch [2] * 100) / 256;
        diff += t * t;

        if (diff == 0)
        {
#if 0
            g_printerr ("%d and %d are the same\n", j - 1, i);
#endif
            continue;
        }
        else if (diff < best_diff)
        {
            best_pair = j - 1;
            best_diff = diff;
        }

        palette_out->colors [j++] = palette_out->colors [i];
    }

    palette_out->n_colors = j;

#if 0
    g_printerr ("Colors after: %d\n", palette_out->n_colors);
#endif

    g_assert (palette_out->n_colors >= 0 && palette_out->n_colors <= 256);

    if (palette_out->n_colors < 256)
    {
#if 0
        g_printerr ("Color 0 moved to end (%d)\n", palette_out->n_colors);
#endif
        palette_out->colors [palette_out->n_colors] = palette_out->colors [0];
        palette_out->n_colors++;
    }
    else
    {
        /* Delete one color to make room for transparency */
        palette_out->colors [best_pair] = palette_out->colors [0];
#if 0
        g_printerr ("Color 0 replaced %d\n", best_pair);
#endif
    }
}

static void
dump_octree (const ChafaPalette *palette,
             const ChafaPaletteOctNode *node, ChafaColorSpace color_space)
{
    gint i;

    g_printerr ("{ ");

    for (i = 0; i < 8; i++)
    {
        g_printerr ("%d ", node->child_index [i]);
    }

    g_printerr ("}\n");

    for (i = 0; i < 8; i++)
    {
        gint16 index;

        index = node->child_index [i];
        if (index == CHAFA_OCT_TREE_INDEX_NULL)
            continue;

        if (index < 256)
        {
        }
        else
        {
            const ChafaPaletteOctNode *child_node = &palette->oct_tree [color_space] [index - 256];

            g_printerr ("-> (%d) ", index);
            dump_octree (palette, child_node, color_space);
        }
    }

    g_printerr ("<- ");
}

void
chafa_palette_init (ChafaPalette *palette_out, ChafaPaletteType type)
{
    gint i;

    chafa_init_palette ();
    palette_out->type = type;

    for (i = 0; i < CHAFA_PALETTE_INDEX_MAX; i++)
    {
        palette_out->colors [i].col [CHAFA_COLOR_SPACE_RGB] = *chafa_get_palette_color_256 (i, CHAFA_COLOR_SPACE_RGB);
        palette_out->colors [i].col [CHAFA_COLOR_SPACE_DIN99D] = *chafa_get_palette_color_256 (i, CHAFA_COLOR_SPACE_DIN99D);
    }
}

/* pixels must point to RGBA8888 data to sample */
void
chafa_palette_generate (ChafaPalette *palette_out, gconstpointer pixels, gint n_pixels,
                        ChafaColorSpace color_space, gint alpha_threshold)
{
    guint32 *pixels_copy;
    gint copy_n_pixels;

    palette_out->type = CHAFA_PALETTE_TYPE_DYNAMIC_256;
    palette_out->alpha_threshold = alpha_threshold;

    pixels_copy = g_malloc (N_SAMPLES * sizeof (guint32));
    copy_n_pixels = extract_samples (pixels, pixels_copy, n_pixels, N_SAMPLES, alpha_threshold);

    DEBUG (g_printerr ("Extracted %d samples.\n", copy_n_pixels));

    if (copy_n_pixels < 1)
    {
        palette_out->n_colors = 0;
        goto out;
    }

    median_cut (palette_out, pixels_copy, 0, copy_n_pixels, 0, 128);
    palette_out->n_colors = 128;
    clean_up (palette_out);

#if 1
    diversity_pass (palette_out, pixels_copy, copy_n_pixels, palette_out->n_colors, 256 - palette_out->n_colors);
    palette_out->n_colors = 256;
    clean_up (palette_out);
#endif

    gen_oct_tree (palette_out, CHAFA_COLOR_SPACE_RGB);

    if (color_space == CHAFA_COLOR_SPACE_DIN99D)
    {
        gen_din99d_color_space (palette_out);
        gen_oct_tree (palette_out, CHAFA_COLOR_SPACE_DIN99D);
    }

out:
    g_free (pixels_copy);
}

static void
linear_subtree_nearest_color (const ChafaPalette *palette,
                              const ChafaPaletteOctNode *node, ChafaColorSpace color_space,
                              const ChafaColor *color, gint16 *best_index, gint *best_error)
{
    gint i;

    for (i = 0; i < 8; i++)
    {
        gint16 index;

        index = node->child_index [i];
        if (index == CHAFA_OCT_TREE_INDEX_NULL)
            continue;

        if (index < 256)
        {
            const ChafaColor *try_color = &palette->colors [index].col [color_space];
            gint error = chafa_color_diff_fast (color, try_color);

            if (error < *best_error)
            {
                *best_index = index;
                *best_error = error;
            }
        }
    }

    for (i = 0; i < 8; i++)
    {
        gint16 index;

        if (*best_error < 3 * (10 * 10))
            break;

        index = node->child_index [i];
        if (index == CHAFA_OCT_TREE_INDEX_NULL)
            continue;

        if (index >= 256)
        {
            const ChafaPaletteOctNode *child_node = &palette->oct_tree [color_space] [index - 256];
            linear_subtree_nearest_color (palette, child_node, color_space, color, best_index, best_error);
        }
    }
}

static gint16
linear_nearest_color (const ChafaPalette *palette, ChafaColorSpace color_space, const ChafaColor *color)
{
    gint i;
    gint16 best_index = 1;
    gint best_error = G_MAXINT;

    for (i = 1; i < palette->n_colors; i++)
    {
        const ChafaColor *try_color = &palette->colors [i].col [color_space];
        gint error = chafa_color_diff_fast (color, try_color);

        if (error < best_error)
        {
            best_index = i;
            best_error = error;
        }
    }

    return best_index;
}

static gint16
oct_tree_lookup_nearest_color (const ChafaPalette *palette, ChafaColorSpace color_space,
                               const ChafaColor *color)
{
    const ChafaPaletteOctNode *node = NULL, *parent_node;
    gint16 best_index = CHAFA_OCT_TREE_INDEX_NULL;
    gint best_error = G_MAXINT;
    gint16 index;

    index = palette->oct_tree_root [color_space];
#if 0
    g_printerr ("Root: %d\n", index);
#endif

    for (;;)
    {
        parent_node = node;
        node = &palette->oct_tree [color_space] [index - 256];
#if 0
        g_printerr ("Branch: %d  Bit: %d  Index: %d\n",
                    get_color_branch (color, node->branch_bit),
                    node->branch_bit,
                    index);
#endif

        index = node->child_index [get_color_branch (color, node->branch_bit)];

        if (index == CHAFA_OCT_TREE_INDEX_NULL || index < 256 || !prefix_match (node, color))
            break;
    }

    linear_subtree_nearest_color (palette, /* parent_node ? parent_node : */ node,
                                  color_space, color, &best_index, &best_error);
    return best_index;
}

typedef struct
{
    ChafaPalette *palette;
    ChafaColorSpace color_space;
    ChafaPaletteOctNode *found_node;
    gint16 found_index;
}
OctTreeSearchCtx;

static void
find_deep_node_r (OctTreeSearchCtx *ctx, gint16 index)
{
    ChafaPaletteOctNode *node;
    gint i;

    node = &ctx->palette->oct_tree [ctx->color_space] [index - 256];

    if (!ctx->found_node || node->branch_bit < ctx->found_node->branch_bit)
    {
        ctx->found_node = node;
        ctx->found_index = index;
    }

    for (i = 0; i < 8; i++)
    {
        gint16 child_index = node->child_index [i];

        if (ctx->found_node->branch_bit == 0)
            return;

        if (child_index != CHAFA_OCT_TREE_INDEX_NULL && child_index >= 256)
        {
            find_deep_node_r (ctx, child_index);
        }
    }
}

static gint16
find_deep_node (const ChafaPalette *palette, ChafaColorSpace color_space)
{
    OctTreeSearchCtx ctx = { NULL };

    ctx.palette = (ChafaPalette *) palette;
    ctx.color_space = color_space;

    find_deep_node_r (&ctx, palette->oct_tree_root [color_space]);
    return ctx.found_index;
}

gint
chafa_palette_lookup_nearest (const ChafaPalette *palette, ChafaColorSpace color_space,
                              const ChafaColor *color, ChafaColorCandidates *candidates)
{
    if (palette->type == CHAFA_PALETTE_TYPE_DYNAMIC_256)
    {
        gint result;

        /* Transparency */
        if (color->ch [3] < palette->alpha_threshold)
            return 0;

#if 0
        result = linear_nearest_color (palette, color_space, color);
#else
        result = oct_tree_lookup_nearest_color (palette, color_space, color);
#endif

        if (candidates)
        {
            /* The only consumer of multiple candidates is the cell canvas, and that
             * supports fixed palettes only. Therefore, in practice we'll never end up here.
             * Let's not leave a loose end, though... */

            candidates->index [0] = result;
            candidates->index [1] = result;
            candidates->error [0] = 0;
            candidates->error [1] = 0;
        }

        return result;
    }
    else
    {
        ChafaColorCandidates candidates_temp;

        if (!candidates)
            candidates = &candidates_temp;

#if 0
        /* Transparency */

        /* NOTE: Disabled because chafa_pick_color_*() deal
         * with transparency */
        if (color->ch [3] < palette->alpha_threshold)
            return CHAFA_PALETTE_INDEX_TRANSPARENT;
#endif

        if (palette->type == CHAFA_PALETTE_TYPE_FIXED_256)
            chafa_pick_color_256 (color, color_space, candidates);
        else if (palette->type == CHAFA_PALETTE_TYPE_FIXED_240)
            chafa_pick_color_240 (color, color_space, candidates);
        else if (palette->type == CHAFA_PALETTE_TYPE_FIXED_16)
            chafa_pick_color_16 (color, color_space, candidates);
        else /* CHAFA_PALETTE_TYPE_FIXED_FGBG */
            chafa_pick_color_fgbg (color, color_space,
                                   &palette->colors [CHAFA_PALETTE_INDEX_FG].col [color_space],
                                   &palette->colors [CHAFA_PALETTE_INDEX_BG].col [color_space],
                                   candidates);

        return candidates->index [0];
    }

    g_assert_not_reached ();
}

const ChafaColor *
chafa_palette_get_color (const ChafaPalette *palette, ChafaColorSpace color_space,
                         gint index)
{
    return &palette->colors [index].col [color_space];
}

void
chafa_palette_set_color (ChafaPalette *palette, gint index, const ChafaColor *color)
{
    palette->colors [index].col [CHAFA_COLOR_SPACE_RGB] = *color;
    chafa_color_rgb_to_din99d (&palette->colors [index].col [CHAFA_COLOR_SPACE_RGB],
                               &palette->colors [index].col [CHAFA_COLOR_SPACE_DIN99D]);
}
