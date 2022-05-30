/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2022 Hans Petter Jansson
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_SOLID | CHAFA_SYMBOL_TAG_SEXTANT,
        0x2588,
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_HHALF | CHAFA_SYMBOL_TAG_SEXTANT,
        0x258c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_HHALF | CHAFA_SYMBOL_TAG_INVERTED | CHAFA_SYMBOL_TAG_SEXTANT,
        0x2590,
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXX XXX"
            "XX XXX X"
            " XXX XXX"
            "XX XXX X"
            " XXX XXX"
            "XX XXX X"
            " XXX XXX"
            "XX XXX X")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb3c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "X       "
            "XXX     "
            "XXXX    ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb3d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "XX      "
            "XXXXX   "
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb3e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "X       "
            "XX      "
            "XX      "
            "XXX     "
            "XXXX    ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb3f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "XX      "
            "XXX     "
            "XXXXX   "
            "XXXXXXX "
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb40,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "X       "
            "X       "
            "XX      "
            "XX      "
            "XXX     "
            "XXX     "
            "XXXX    ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb41,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    XXXX"
            "   XXXXX"
            " XXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb42,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "     XXX"
            "  XXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },

    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb43,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    XXXX"
            "   XXXXX"
            "   XXXXX"
            "  XXXXXX"
            " XXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb44,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "       X"
            "      XX"
            "    XXXX"
            "  XXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb45,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    XXXX"
            "    XXXX"
            "   XXXXX"
            "   XXXXX"
            "  XXXXXX"
            "  XXXXXX"
            " XXXXXXX"
            " XXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb46,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "     XXX"
            "   XXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb47,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "       X"
            "     XXX"
            "    XXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb48,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "      XX"
            "   XXXXX"
            " XXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb49,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "       X"
            "      XX"
            "      XX"
            "     XXX"
            "    XXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb4a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "       X"
            "     XXX"
            "   XXXXX"
            " XXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb4b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "       X"
            "       X"
            "      XX"
            "      XX"
            "     XXX"
            "     XXX"
            "    XXXX"
            "    XXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb4c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXX    "
            "XXXXXX  "
            "XXXXXXX "
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb4d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "X       "
            "XXX     "
            "XXXXXX  "
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb4e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXX    "
            "XXXXX   "
            "XXXXXX  "
            "XXXXXX  "
            "XXXXXXX "
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb4f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "X       "
            "XX      "
            "XXXX    "
            "XXXXXX  "
            "XXXXXXX "
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb50,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXX    "
            "XXXX    "
            "XXXXX   "
            "XXXXX   "
            "XXXXXX  "
            "XXXXXX  "
            "XXXXXXX "
            "XXXXXXX ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb51,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "XXX     "
            "XXXXX   "
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb52,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            " XXXXXXX"
            "   XXXXX"
            "    XXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb53,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "  XXXXXX"
            "     XXX"
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb54,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            " XXXXXXX"
            "  XXXXXX"
            "  XXXXXX"
            "   XXXXX"
            "   XXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb55,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            " XXXXXXX"
            "  XXXXXX"
            "    XXXX"
            "     XXX"
            "       X")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb56,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXXXXX"
            " XXXXXXX"
            "  XXXXXX"
            "  XXXXXX"
            "   XXXXX"
            "   XXXXX"
            "    XXXX"
            "    XXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb57,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXX    "
            "XXX     "
            "X       "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb58,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXX   "
            "XXX     "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb59,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXX    "
            "XXX     "
            "XXX     "
            "X       "
            "X       "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb5a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXX "
            "XXXXX   "
            "XXX     "
            "XX      "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb5b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXX    "
            "XXXX    "
            "XXX     "
            "XXX     "
            "XX      "
            "X       "
            "X       "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb5c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXX   "
            "XXX     "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb5d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXX "
            "XXXXXX  "
            "XXXX    ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb5e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXX  "
            "XXX     "
            "X       ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb5f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXX "
            "XXXXXXX "
            "XXXXXX  "
            "XXXXX   "
            "XXXX    ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb60,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXX "
            "XXXXXX  "
            "XXXX    "
            "XXX     "
            "X       ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb61,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXX "
            "XXXXXXX "
            "XXXXXX  "
            "XXXXXX  "
            "XXXXX   "
            "XXXXX   "
            "XXXX    "
            "XXXX    ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb62,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    XXXX"
            "      XX"
            "       X"
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb63,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "   XXXXX"
            "      XX"
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb64,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    XXXX"
            "     XXX"
            "      XX"
            "      XX"
            "       X"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb65,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXXXXX"
            "  XXXXXX"
            "    XXXX"
            "     XXX"
            "       X"
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb66,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    XXXX"
            "    XXXX"
            "     XXX"
            "     XXX"
            "      XX"
            "      XX"
            "       X"
            "       X")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb67,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "   XXXXX"
            "     XXX"
            "        "
            "        "
            "        ")
    },

    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb68,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            " XXXXXXX"
            "  XXXXXX"
            "   XXXXX"
            "    XXXX"
            "   XXXXX"
            "  XXXXXX"
            " XXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb69,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "X      X"
            "XX    XX"
            "XXX  XXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb6a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXX "
            "XXXXXX  "
            "XXXXX   "
            "XXXX    "
            "XXXXX   "
            "XXXXXX  "
            "XXXXXXX ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb6b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXXXXXXX"
            "XXX  XXX"
            "XX    XX"
            "X      X"
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb6c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "X       "
            "XX      "
            "XXX     "
            "XXXX    "
            "XXX     "
            "XX      "
            "X       ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb6d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            " XXXXXX "
            "  XXXX  "
            "   XX   "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb6e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "       X"
            "      XX"
            "     XXX"
            "    XXXX"
            "     XXX"
            "      XX"
            "       X")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb6f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "   XX   "
            "  XXXX  "
            " XXXXXX "
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb9a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXX "
            " XXXXX  "
            "  XXX   "
            "   X    "
            "    X   "
            "   XXX  "
            "  XXXXX "
            " XXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY | CHAFA_SYMBOL_TAG_WEDGE,
        0x1fb9b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "       X"
            "X     XX"
            "XX   XXX"
            "XXX XXXX"
            "XXXX XXX"
            "XXX   XX"
            "XX     X"
            "X       ")
    },

    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb98,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "X  X  X "
            " X  X  X"
            "  X  X  "
            "X  X  X "
            " X  X  X"
            "  X  X  "
            "X  X  X "
            " X  X  X")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb99,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " X  X  X"
            "X  X  X "
            "  X  X  "
            " X  X  X"
            "X  X  X "
            "  X  X  "
            " X  X  X"
            "X  X  X ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb7c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "X       "
            "X       "
            "X       "
            "X       "
            "X       "
            "X       "
            "X       "
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb7d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "X       "
            "X       "
            "X       "
            "X       "
            "X       "
            "X       "
            "X       ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb7e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "       X"
            "       X"
            "       X"
            "       X"
            "       X"
            "       X"
            "       X")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb7f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "       X"
            "       X"
            "       X"
            "       X"
            "       X"
            "       X"
            "       X"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb80,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXXX"
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb70,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " X      "
            " X      "
            " X      "
            " X      "
            " X      "
            " X      "
            " X      "
            " X      ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb71,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X     "
            "  X     "
            "  X     "
            "  X     "
            "  X     "
            "  X     "
            "  X     "
            "  X     ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb72,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "   X    ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb73,
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb74,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "     X  "
            "     X  "
            "     X  "
            "     X  "
            "     X  "
            "     X  "
            "     X  "
            "     X  ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb75,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "      X "
            "      X "
            "      X "
            "      X "
            "      X "
            "      X "
            "      X "
            "      X ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb76,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "XXXXXXXX"
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb77,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XXXXXXXX"
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb78,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb79,
        CHAFA_SYMBOL_OUTLINE_8X8 (
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
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb7a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb7b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "XXXXXXXX"
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb95,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX  XX  "
            "XX  XX  "
            "  XX  XX"
            "  XX  XX"
            "XX  XX  "
            "XX  XX  "
            "  XX  XX"
            "  XX  XX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb96,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX  XX"
            "  XX  XX"
            "XX  XX  "
            "XX  XX  "
            "  XX  XX"
            "  XX  XX"
            "XX  XX  "
            "XX  XX  ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fb97,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XXXXXXXX"
            "XXXXXXXX"
            "        "
            "        "
            "XXXXXXXX"
            "XXXXXXXX")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fba0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "  X     "
            " X      "
            "X       "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fba1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    X   "
            "     X  "
            "      X "
            "       X"
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fba2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "X       "
            " X      "
            "  X     "
            "   X    ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fba3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "       X"
            "      X "
            "     X  "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fba4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "  X     "
            " X      "
            "X       "
            "X       "
            " X      "
            "  X     "
            "   X    ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fba5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    X   "
            "     X  "
            "      X "
            "       X"
            "       X"
            "      X "
            "     X  "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fba6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "X      X"
            " X    X "
            "  X  X  "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fba7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  X  X  "
            " X    X "
            "X      X"
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fba8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "  X     "
            " X      "
            "X       "
            "       X"
            "      X "
            "     X  "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fba9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    X   "
            "     X  "
            "      X "
            "       X"
            "X       "
            " X      "
            "  X     "
            "   X    ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fbaa,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    X   "
            "     X  "
            "      X "
            "       X"
            "X      X"
            " X    X "
            "  X  X  "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fbab,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "  X     "
            " X      "
            "X       "
            "X      X"
            " X    X "
            "  X  X  "
            "   XX   ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fbac,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  X  X  "
            " X    X "
            "X      X"
            "       X"
            "      X "
            "     X  "
            "    X   ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fbad,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  X  X  "
            " X    X "
            "X      X"
            "X       "
            " X      "
            "  X     "
            "   X    ")
    },
    {
        CHAFA_SYMBOL_TAG_LEGACY,
        0x1fbae,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  X  X  "
            " X    X "
            "X      X"
            "X      X"
            " X    X "
            "  X  X  "
            "   XX   ")
    },
