/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018 Hans Petter Jansson
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
#include "chafa/chafa.h"
#include "chafa/chafa-private.h"

ChafaSymbol *chafa_symbols;
ChafaSymbol *chafa_fill_symbols;

static const ChafaSymbol symbol_defs [] =
{
    {
        CHAFA_SYMBOL_TAG_SPACE,
        0x20,
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_VHALF | CHAFA_SYMBOL_TAG_INVERTED,
        0x2580,
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "        "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2581,
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
        "XXXXXXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2582,
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
        "XXXXXXXX"
        "XXXXXXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2583,
        "        "
        "        "
        "        "
        "        "
        "        "
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_VHALF,
        0x2584,
        "        "
        "        "
        "        "
        "        "
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2585,
        "        "
        "        "
        "        "
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2586,
        "        "
        "        "
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2587,
        "        "
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX",
        0, 0, FALSE
    },
    {
        /* Full block */
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_SOLID,
        0x2588,
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x2589,
        "XXXXXXX "
        "XXXXXXX "
        "XXXXXXX "
        "XXXXXXX "
        "XXXXXXX "
        "XXXXXXX "
        "XXXXXXX "
        "XXXXXXX ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x258a,
        "XXXXXX  "
        "XXXXXX  "
        "XXXXXX  "
        "XXXXXX  "
        "XXXXXX  "
        "XXXXXX  "
        "XXXXXX  "
        "XXXXXX  ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x258b,
        "XXXXX   "
        "XXXXX   "
        "XXXXX   "
        "XXXXX   "
        "XXXXX   "
        "XXXXX   "
        "XXXXX   "
        "XXXXX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_HHALF,
        0x258c,
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXX    ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x258d,
        "XXX     "
        "XXX     "
        "XXX     "
        "XXX     "
        "XXX     "
        "XXX     "
        "XXX     "
        "XXX     ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x258e,
        "XX      "
        "XX      "
        "XX      "
        "XX      "
        "XX      "
        "XX      "
        "XX      "
        "XX      ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK,
        0x258f,
        "X       "
        "X       "
        "X       "
        "X       "
        "X       "
        "X       "
        "X       "
        "X       ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_HHALF | CHAFA_SYMBOL_TAG_INVERTED,
        0x2590,
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_INVERTED,
        0x2594,
        "XXXXXXXX"
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_INVERTED,
        0x2595,
        "       X"
        "       X"
        "       X"
        "       X"
        "       X"
        "       X"
        "       X"
        "       X",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD,
        0x2596,
        "        "
        "        "
        "        "
        "        "
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXX    ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD,
        0x2597,
        "        "
        "        "
        "        "
        "        "
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD,
        0x2598,
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "        "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_INVERTED,
        0x2599,
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD,
        0x259a,
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_INVERTED,
        0x259b,
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXX    ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_INVERTED,
        0x259c,
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD,
        0x259d,
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "        "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_INVERTED,
        0x259e,
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "XXXX    "
        "XXXX    "
        "XXXX    "
        "XXXX    ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_QUAD | CHAFA_SYMBOL_TAG_INVERTED,
        0x259f,
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "    XXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX",
        0, 0, FALSE
    },
    /* Begin box drawing characters */
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2500,
        "        "
        "        "
        "        "
        "        "
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2501,
        "        "
        "        "
        "        "
        "XXXXXXXX"
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2502,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2503,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2504,
        "        "
        "        "
        "        "
        "        "
        "XX XX XX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2505,
        "        "
        "        "
        "        "
        "XX XX XX"
        "XX XX XX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2506,
        "    X   "
        "    X   "
        "        "
        "    X   "
        "    X   "
        "        "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2507,
        "   XX   "
        "   XX   "
        "        "
        "   XX   "
        "   XX   "
        "        "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2508,
        "        "
        "        "
        "        "
        "        "
        "X X X X "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2509,
        "        "
        "        "
        "        "
        "X X X X "
        "X X X X "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x250a,
        "    X   "
        "        "
        "    X   "
        "        "
        "    X   "
        "        "
        "    X   "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x250b,
        "   XX   "
        "        "
        "   XX   "
        "        "
        "   XX   "
        "        "
        "   XX   "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x250c,
        "        "
        "        "
        "        "
        "        "
        "    XXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x250d,
        "        "
        "        "
        "        "
        "    XXXX"
        "    XXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x250e,
        "        "
        "        "
        "        "
        "        "
        "   XXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x250f,
        "        "
        "        "
        "        "
        "   XXXXX"
        "   XXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2510,
        "        "
        "        "
        "        "
        "        "
        "XXXXX   "
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2511,
        "        "
        "        "
        "        "
        "XXXXX   "
        "XXXXX   "
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2512,
        "        "
        "        "
        "        "
        "        "
        "XXXXX   "
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2513,
        "        "
        "        "
        "        "
        "XXXXX   "
        "XXXXX   "
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2514,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "    XXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2515,
        "    X   "
        "    X   "
        "    X   "
        "    XXXX"
        "    XXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2516,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "   XXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2517,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XXXXX"
        "   XXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2518,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "XXXXX   "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2519,
        "    X   "
        "    X   "
        "    X   "
        "XXXXX   "
        "XXXXX   "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251a,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXX   "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251b,
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXX   "
        "XXXXX   "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251c,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "    XXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251d,
        "    X   "
        "    X   "
        "    X   "
        "    XXXX"
        "    XXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251e,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "   XXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x251f,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "   XXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2520,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "   XXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2521,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XXXXX"
        "   XXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2522,
        "    X   "
        "    X   "
        "    X   "
        "   XXXXX"
        "   XXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2523,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XXXXX"
        "   XXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2524,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "XXXXX   "
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2525,
        "    X   "
        "    X   "
        "    X   "
        "XXXXX   "
        "XXXXX   "
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2526,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXX   "
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2527,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "XXXXX   "
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2528,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXX   "
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2529,
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXX   "
        "XXXXX   "
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252a,
        "    X   "
        "    X   "
        "    X   "
        "XXXXX   "
        "XXXXX   "
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252b,
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXX   "
        "XXXXX   "
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252c,
        "        "
        "        "
        "        "
        "        "
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252d,
        "        "
        "        "
        "        "
        "XXXXX   "
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252e,
        "        "
        "        "
        "        "
        "    XXXX"
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x252f,
        "        "
        "        "
        "        "
        "XXXXXXXX"
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2530,
        "        "
        "        "
        "        "
        "        "
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2531,
        "        "
        "        "
        "        "
        "XXXXX   "
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2532,
        "        "
        "        "
        "        "
        "   XXXXX"
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2533,
        "        "
        "        "
        "        "
        "XXXXXXXX"
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2534,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2535,
        "    X   "
        "    X   "
        "    X   "
        "XXXXX   "
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2536,
        "    X   "
        "    X   "
        "    X   "
        "    XXXX"
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2537,
        "    X   "
        "    X   "
        "    X   "
        "XXXXXXXX"
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2538,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2539,
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXX   "
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253a,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XXXXX"
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253b,
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXXXXX"
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253c,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253d,
        "    X   "
        "    X   "
        "    X   "
        "XXXXX   "
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253e,
        "    X   "
        "    X   "
        "    X   "
        "    XXXX"
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x253f,
        "    X   "
        "    X   "
        "    X   "
        "XXXXXXXX"
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2540,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2541,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2542,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2543,
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXX   "
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2544,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XXXXX"
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2545,
        "    X   "
        "    X   "
        "    X   "
        "XXXXX   "
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2546,
        "    X   "
        "    X   "
        "    X   "
        "   XXXXX"
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2547,
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXXXXX"
        "XXXXXXXX"
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2548,
        "    X   "
        "    X   "
        "    X   "
        "XXXXXXXX"
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x2549,
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXX   "
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x254a,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XXXXX"
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x254b,
        "   XX   "
        "   XX   "
        "   XX   "
        "XXXXXXXX"
        "XXXXXXXX"
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x254c,
        "        "
        "        "
        "        "
        "        "
        "XXX  XXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x254d,
        "        "
        "        "
        "        "
        "XXX  XXX"
        "XXX  XXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x254e,
        "    X   "
        "    X   "
        "    X   "
        "        "
        "        "
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x254f,
        "   XX   "
        "   XX   "
        "   XX   "
        "        "
        "        "
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DIAGONAL,
        0x2571,
        "       X"
        "      X "
        "     X  "
        "    X   "
        "   X    "
        "  X     "
        " X      "
        "X       ",
        0, 0, FALSE
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DIAGONAL,
        0x2571,
        "      XX"
        "     XXX"
        "    XXX "
        "   XXX  "
        "  XXX   "
        " XXX    "
        "XXX     "
        "XX      ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DIAGONAL,
        0x2572,
        "X       "
        " X      "
        "  X     "
        "   X    "
        "    X   "
        "     X  "
        "      X "
        "       X",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DIAGONAL,
        0x2572,
        "XX      "
        "XXX     "
        " XXX    "
        "  XXX   "
        "   XXX  "
        "    XXX "
        "     XXX"
        "      XX",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DIAGONAL,
        0x2573,
        "X      X"
        " X    X "
        "  X  X  "
        "   XX   "
        "   XX   "
        "  X  X  "
        " X    X "
        "X      X",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2574,
        "        "
        "        "
        "        "
        "        "
        "XXXX    "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2575,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "        "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2576,
        "        "
        "        "
        "        "
        "        "
        "    XXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2577,
        "        "
        "        "
        "        "
        "        "
        "    X   "
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2578,
        "        "
        "        "
        "        "
        "XXXX    "
        "XXXX    "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x2579,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "        "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x257a,
        "        "
        "        "
        "        "
        "    XXXX"
        "    XXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER | CHAFA_SYMBOL_TAG_DOT,
        0x257b,
        "        "
        "        "
        "        "
        "        "
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x257c,
        "        "
        "        "
        "        "
        "    XXXX"
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x257d,
        "    X   "
        "    X   "
        "    X   "
        "    X   "
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x257e,
        "        "
        "        "
        "        "
        "XXXX    "
        "XXXXXXXX"
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_BORDER,
        0x257f,
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "    X   "
        "    X   "
        "    X   "
        "    X   ",
        0, 0, FALSE
    },
    /* Begin dot characters*/
    {
        CHAFA_SYMBOL_TAG_DOT,
        0x25ae, /* Black vertical rectangle */
        "        "
        " XXXXXX "
        " XXXXXX "
        " XXXXXX "
        " XXXXXX "
        " XXXXXX "
        " XXXXXX "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_DOT,
        0x25a0, /* Black square */
        "        "
        "        "
        " XXXXXX "
        " XXXXXX "
        " XXXXXX "
        " XXXXXX "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_DOT,
        0x25aa, /* Black small square */
        "        "
        "        "
        "  XXXX  "
        "  XXXX  "
        "  XXXX  "
        "  XXXX  "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x25aa, /* Black small square */
        "        "
        "        "
        " XXXX   "
        " XXXX   "
        " XXXX   "
        " XXXX   "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x25aa, /* Black small square */
        "        "
        "        "
        "   XXXX "
        "   XXXX "
        "   XXXX "
        "   XXXX "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x25aa, /* Black small square */
        "        "
        "  XXXX  "
        "  XXXX  "
        "  XXXX  "
        "  XXXX  "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x25aa, /* Black small square */
        "        "
        "        "
        "        "
        "  XXXX  "
        "  XXXX  "
        "  XXXX  "
        "  XXXX  "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_DOT,
        0x00b7, /* Middle dot */
        "        "
        "        "
        "        "
        "   XX   "
        "   XX   "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x00b7, /* Middle dot */
        "        "
        "        "
        "        "
        "  XX    "
        "  XX    "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x00b7, /* Middle dot */
        "        "
        "        "
        "        "
        "    XX   "
        "    XX  "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x00b7, /* Middle dot */
        "        "
        "        "
        "   XX   "
        "   XX   "
        "        "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x00b7, /* Middle dot */
        "        "
        "        "
        "        "
        "        "
        "   XX   "
        "   XX   "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        0, 0, "",
        0, 0, FALSE
    }
};

static const ChafaSymbol fill_symbol_defs [] =
{
    {
        CHAFA_SYMBOL_TAG_SPACE,
        0x20,
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
        "        ",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_STIPPLE,
        0x2591,
        "22222222"
        "22222222"
        "22222222"
        "22222222"
        "22222222"
        "22222222"
        "22222222"
        "22222222",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_STIPPLE,
        0x2592,
        "55555555"
        "55555555"
        "55555555"
        "55555555"
        "55555555"
        "55555555"
        "55555555"
        "55555555",
        0, 0, FALSE
    },
    {
        CHAFA_SYMBOL_TAG_STIPPLE,
        0x2593,
        "88888888"
        "88888888"
        "88888888"
        "88888888"
        "88888888"
        "88888888"
        "88888888"
        "88888888",
        0, 0, FALSE
    },
    {
        /* Full block */
        CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_SOLID,
        0x2588,
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX"
        "XXXXXXXX",
        0, 0, FALSE
    },
    {
      0, 0, "", 0, 0, FALSE
    }
};

static gboolean symbols_initialized;

static ChafaSymbol *
init_symbol_array (const ChafaSymbol *defs)
{
    gint i;
    gchar xlate [256];
    ChafaSymbol *syms;

    xlate [' '] = 0;
    xlate ['0'] = 0;
    xlate ['1'] = 1;
    xlate ['2'] = 2;
    xlate ['3'] = 3;
    xlate ['4'] = 4;
    xlate ['5'] = 5;
    xlate ['6'] = 6;
    xlate ['7'] = 7;
    xlate ['8'] = 8;
    xlate ['9'] = 9;
    xlate ['X'] = 10;

    syms = g_new0 (ChafaSymbol, 256);

    for (i = 0; defs [i].c; i++)
    {
        gint j;

        syms [i].sc = defs [i].sc;
        syms [i].c = defs [i].c;
        syms [i].coverage = g_malloc (CHAFA_SYMBOL_N_PIXELS);
        syms [i].fg_weight = 0;
        syms [i].bg_weight = 0;
        syms [i].have_mixed = FALSE;

        for (j = 0; j < CHAFA_SYMBOL_N_PIXELS; j++)
        {
            guchar p = defs [i].coverage [j];
            p = xlate [p];
            syms [i].coverage [j] = p;
            syms [i].fg_weight += p;
            syms [i].bg_weight += 10 - p;

            if (p != 0 && p != 10)
                syms [i].have_mixed = TRUE;
        }
    }

    return syms;
}

void
chafa_init_symbols (void)
{
    if (symbols_initialized)
        return;

    chafa_symbols = init_symbol_array (symbol_defs);
    chafa_fill_symbols = init_symbol_array (fill_symbol_defs);

    symbols_initialized = TRUE;
}
