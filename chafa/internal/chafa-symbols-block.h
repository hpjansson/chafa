/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2021 Hans Petter Jansson
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

/* Block and border characters
 * ---------------------------
 *
 * This is meant to be #included in the symbol definition table of
 * chafa-symbols.c. It's kept in a separate file due to its size. */

    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_VHALF | CHAFA_SYMBOL_TAG_INVERTED,
        0x2580,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2581,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2582,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2583,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_VHALF,
        0x2584,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2585,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2586,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2587,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        /* Full block */
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_SOLID,
        0x2588,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2589,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXXXXX "
            "XXXXXXX "
            "XXXXXXX "
            "XXXXXXX "
            "XXXXXXX "
            "XXXXXXX "
            "XXXXXXX "
            "XXXXXXX ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x258a,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXXXX  "
            "XXXXXX  "
            "XXXXXX  "
            "XXXXXX  "
            "XXXXXX  "
            "XXXXXX  "
            "XXXXXX  "
            "XXXXXX  ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x258b,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXXX   "
            "XXXXX   "
            "XXXXX   "
            "XXXXX   "
            "XXXXX   "
            "XXXXX   "
            "XXXXX   "
            "XXXXX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_HHALF,
        0x258c,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXX    ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x258d,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXX     "
            "XXX     "
            "XXX     "
            "XXX     "
            "XXX     "
            "XXX     "
            "XXX     "
            "XXX     ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x258e,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XX      "
            "XX      "
            "XX      "
            "XX      "
            "XX      "
            "XX      "
            "XX      "
            "XX      ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x258f,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "X       "
            "X       "
            "X       "
            "X       "
            "X       "
            "X       "
            "X       "
            "X       ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_HHALF | CHAFA_SYMBOL_TAG_INVERTED,
        0x2590,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_INVERTED,
        0x2594,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXXXXXX"
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_INVERTED,
        0x2595,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "       X"
            "       X"
            "       X"
            "       X"
            "       X"
            "       X"
            "       X"
            "       X")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD,
        0x2596,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXX    ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD,
        0x2597,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD,
        0x2598,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_INVERTED,
        0x2599,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD,
        0x259a,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_INVERTED,
        0x259b,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXX    ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_INVERTED,
        0x259c,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD,
        0x259d,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_INVERTED,
        0x259e,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "XXXX    "
            "XXXX    "
            "XXXX    "
            "XXXX    ")
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_INVERTED,
        0x259f,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "    XXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    /* Begin box drawing characters */
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2500,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2501,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2502,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2503,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2504,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "XX XX XX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2505,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XX XX XX"
            "XX XX XX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2506,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "        "
            "    X   "
            "    X   "
            "        "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2507,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "        "
            "   XX   "
            "   XX   "
            "        "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2508,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "X X X X "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2509,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "X X X X "
            "X X X X "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x250a,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "        "
            "    X   "
            "        "
            "    X   "
            "        "
            "    X   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x250b,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "        "
            "   XX   "
            "        "
            "   XX   "
            "        "
            "   XX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x250c,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "    XXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x250d,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "    XXXX"
            "    XXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x250e,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "   XXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x250f,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "   XXXXX"
            "   XXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2510,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "XXXXX   "
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2511,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XXXXX   "
            "XXXXX   "
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2512,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "XXXXX   "
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2513,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XXXXX   "
            "XXXXX   "
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2514,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "    XXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2515,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    XXXX"
            "    XXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2516,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "   XXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2517,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XXXXX"
            "   XXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2518,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "XXXXX   "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2519,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "XXXXX   "
            "XXXXX   "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251a,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXX   "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251b,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXX   "
            "XXXXX   "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251c,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "    XXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251d,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    XXXX"
            "    XXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251e,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "   XXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251f,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "   XXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2520,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "   XXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2521,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XXXXX"
            "   XXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2522,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "   XXXXX"
            "   XXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2523,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XXXXX"
            "   XXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2524,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "XXXXX   "
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2525,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "XXXXX   "
            "XXXXX   "
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2526,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXX   "
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2527,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "XXXXX   "
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2528,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXX   "
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2529,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXX   "
            "XXXXX   "
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252a,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "XXXXX   "
            "XXXXX   "
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252b,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXX   "
            "XXXXX   "
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252c,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252d,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XXXXX   "
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252e,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "    XXXX"
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252f,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2530,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2531,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XXXXX   "
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2532,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "   XXXXX"
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2533,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2534,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2535,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "XXXXX   "
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2536,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    XXXX"
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2537,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "XXXXXXXX"
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2538,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2539,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXX   "
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253a,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XXXXX"
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253b,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXXXXX"
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253c,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253d,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "XXXXX   "
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253e,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    XXXX"
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253f,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "XXXXXXXX"
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2540,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2541,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2542,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2543,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXX   "
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2544,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XXXXX"
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2545,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "XXXXX   "
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2546,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "   XXXXX"
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2547,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXXXXX"
            "XXXXXXXX"
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2548,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "XXXXXXXX"
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2549,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXX   "
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x254a,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XXXXX"
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x254b,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "XXXXXXXX"
            "XXXXXXXX"
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x254c,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "XXX  XXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x254d,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XXX  XXX"
            "XXX  XXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x254e,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "        "
            "        "
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x254f,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "        "
            "        "
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DIAGONAL,
        0x2571,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "       X"
            "      X "
            "     X  "
            "    X   "
            "   X    "
            "  X     "
            " X      "
            "X       ")
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DIAGONAL,
        0x2571,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "      XX"
            "     XXX"
            "    XXX "
            "   XXX  "
            "  XXX   "
            " XXX    "
            "XXX     "
            "XX      ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DIAGONAL,
        0x2572,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "X       "
            " X      "
            "  X     "
            "   X    "
            "    X   "
            "     X  "
            "      X "
            "       X")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DIAGONAL,
        0x2572,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "XX      "
            "XXX     "
            " XXX    "
            "  XXX   "
            "   XXX  "
            "    XXX "
            "     XXX"
            "      XX")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DIAGONAL,
        0x2573,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "X      X"
            " X    X "
            "  X  X  "
            "   XX   "
            "   XX   "
            "  X  X  "
            " X    X "
            "X      X")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2574,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "XXXX    "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2575,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2576,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "    XXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2577,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "    X   "
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2578,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XXXX    "
            "XXXX    "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2579,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x257a,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "    XXXX"
            "    XXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x257b,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "        "
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x257c,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "    XXXX"
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x257d,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x257e,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "        "
            "        "
            "        "
            "XXXX    "
            "XXXXXXXX"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x257f,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "    X   "
            "    X   "
            "    X   "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_STIPPLE,
        0x2591,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "X   X   "
            "  X   X "
            "X   X   "
            "  X   X "
            "X   X   "
            "  X   X "
            "X   X   "
            "  X   X ")
    },
    {
        CHAFA_SYMBOL_TAG_STIPPLE,
        0x2592,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            "X X X X "
            " X X X X"
            "X X X X "
            " X X X X"
            "X X X X "
            " X X X X"
            "X X X X "
            " X X X X")
    },
    {
        CHAFA_SYMBOL_TAG_STIPPLE,
        0x2593,
        CHAFA_OUTLINE_TO_BITMAP_8X8 (
            " XXX XXX"
            "XX XXX X"
            " XXX XXX"
            "XX XXX X"
            " XXX XXX"
            "XX XXX X"
            " XXX XXX"
            "XX XXX X")
    },
