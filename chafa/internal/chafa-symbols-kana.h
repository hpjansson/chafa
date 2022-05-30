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

/* Japanese hiragana and katakana
 * ------------------------------
 *
 * This is meant to be #included in the symbol definition table of
 * chafa-symbols.c. It's kept in a separate file due to its size. */

/* Hiragana */

    {
        /* [ぁ] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3041,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "   XXXXXXXXX    "
            "      X   X     "
            "     XXXXX XX   "
            "   XX X X   XX  "
            "   X  XX    X   "
            "    XX   XXX    ")
    },
    {
        /* [あ] freq=374 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3042,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "      XX        "
            "  XXXXXXXXXXX   "
            "      X   XX    "
            "     XXXXXXXX   "
            "   XX X  X   X  "
            "  X   X X    X  "
            "  X   XX     X  "
            "   XXX   XXXX   ")
    },
    {
        /* [ぃ] freq=73 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3043,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "   X            "
            "   X       X    "
            "   X        X   "
            "   X        X   "
            "    X  X    X   "
            "     XX         ")
    },
    {
        /* [い] freq=12 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3044,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XX            "
            "  X        XX   "
            "  X         XX  "
            "  XX         X  "
            "   X   X     XX "
            "   XX  X        "
            "    XXX         ")
    },
    {
        /* [ぅ] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3045,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "      XXXX      "
            "                "
            "      XXXX      "
            "    XX    XX    "
            "           X    "
            "          XX    "
            "      XXXX      ")
    },
    {
        /* [う] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3046,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "     XXXXXX     "
            "       XXX      "
            "   XXXX   XXX   "
            "            X   "
            "           X    "
            "         XX     "
            "     XXXX       ")
    },
    {
        /* [ぇ] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3047,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "      XXXXX     "
            "                "
            "    XXXXXXX     "
            "       XX       "
            "     XX XX      "
            "   XX    XXXX   ")
    },
    {
        /* [え] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3048,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "     XXXXXX     "
            "                "
            "   XXXXXXXX     "
            "       XX       "
            "     XXXXX      "
            "   XX    X      "
            "  X       XXXX  ")
    },
    {
        /* [ぉ] freq=11 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3049,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "      X         "
            "   XXXXXX  XXX  "
            "      X         "
            "    XXXXXXXXX   "
            "   X  X     X   "
            "   XXXX XXXX    ")
    },
    {
        /* [お] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x304a,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "     XX         "
            "     X     X    "
            "  XXXXXXX   XXX "
            "     X   XX     "
            "    XXXXX  XXX  "
            "  XX X       X  "
            "  X  X      X   "
            "   XXX  XXXX    ")
    },
    {
        /* [か] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x304b,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "      X         "
            "     XX     X   "
            " XXXXXXXXX  XX  "
            "     X    X  XX "
            "    X     X   X "
            "   XX    XX     "
            "  XX     X      "
            "  X   XXX       ")
    },
    {
        /* [が] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x304c,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "     XX     X X "
            "     X       X  "
            " XXXXXXXXX  X   "
            "    XX   XX  X  "
            "    X    XX   X "
            "   X     X      "
            "  XX     X      "
            "  X  XXXX       ")
    },
    {
        /* [き] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x304d,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "        X       "
            "   XX   XXXXX   "
            "     XXX X      "
            "   XXXXXXXXXX   "
            "          XX    "
            "   X     XXX    "
            "   X            "
            "    XXXXXXXX    ")
    },
    {
        /* [ぎ] freq=48 */
        CHAFA_SYMBOL_TAG_NONE,
        0x304e,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       X   X XX "
            "  XX   XXXXXX   "
            "    XXX X       "
            "  XXX XXXXXXX   "
            "     X    X     "
            "   X    XXXX    "
            "  XX            "
            "   XXXXXXXX     ")
    },
    {
        /* [く] freq=20 */
        CHAFA_SYMBOL_TAG_NONE,
        0x304f,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "         XX     "
            "       XX       "
            "     XX         "
            "   XX           "
            "     XX         "
            "       XXX      "
            "          X     ")
    },
    {
        /* [ぐ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3050,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "         XX     "
            "       XX       "
            "    XXX   X X   "
            "   XX           "
            "     XX         "
            "       XX       "
            "         XX     ")
    },
    {
        /* [け] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3051,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           X    "
            "  XX       X    "
            "  X   XXXXXXXXX "
            "  X        X    "
            "  X        X    "
            "  XX       X    "
            "  XX      X     "
            "        XX      ")
    },
    {
        /* [げ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3052,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            X X "
            "  XX       X X  "
            "  X   XXXXXXXXX "
            "  X        X    "
            "  X        X    "
            "  XX      XX    "
            "  XX      X     "
            "   X   XXX      ")
    },
    {
        /* [こ] freq=6 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3053,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    XXXXXXXX    "
            "                "
            "                "
            "                "
            "   X            "
            "   XX           "
            "     XXXXXXXX   ")
    },
    {
        /* [ご] freq=7 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3054,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            X X "
            "   XXXXXXXXX X  "
            "                "
            "                "
            "                "
            "   X            "
            "   X            "
            "    XXXXXXXXX   ")
    },
    {
        /* [さ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3055,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "        X       "
            "         X   X  "
            "   XXXXXXXXXX   "
            "          X     "
            "           XX   "
            "   X     XX     "
            "   XX           "
            "     XXXXXXX    ")
    },
    {
        /* [ざ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3056,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "        X   X X "
            "        XX      "
            "  XXXXXXXXXXX   "
            "          X     "
            "        X  X    "
            "   X     XX     "
            "   X            "
            "    XXXXXXXX    ")
    },
    {
        /* [し] freq=21 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3057,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "    X           "
            "    X           "
            "    X           "
            "    X           "
            "    X           "
            "    X        X  "
            "    X       XX  "
            "     XXXXXXX    ")
    },
    {
        /* [じ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3058,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "    X           "
            "    X    X XX   "
            "    X     X     "
            "    X           "
            "    X           "
            "    X           "
            "    X       XX  "
            "    XXXXXXXX    ")
    },
    {
        /* [す] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3059,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "         X      "
            "    XXXXXXXXXXX "
            "  XX     X      "
            "      XXXX      "
            "     X   X      "
            "     XXXXX      "
            "        XX      "
            "     XXX        ")
    },
    {
        /* [ず] freq=10 */
        CHAFA_SYMBOL_TAG_NONE,
        0x305a,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "        XX X X  "
            "    XXXXXXXXXX  "
            "  XX    X       "
            "     XXXX       "
            "    XX   X      "
            "     XXXXX      "
            "       XX       "
            "    XXX         ")
    },
    {
        /* [せ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x305b,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    X      X    "
            "    X     XXXXX "
            " XXXXXXXXX X    "
            "    X     XX    "
            "    X    XX     "
            "    XX          "
            "      XXXXXXX   ")
    },
    {
        /* [ぜ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x305c,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "              X "
            "    X     X  X  "
            "    X     XXXX  "
            " XXXXXXXXXX     "
            "    X     X     "
            "    X   XXX     "
            "    X           "
            "     XXXXXXXX   ")
    },
    {
        /* [そ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x305d,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "        XXXX    "
            "     XXX XX     "
            "       XX       "
            "     XX    XXX  "
            "  XXX  XXXX     "
            "      X         "
            "      X         "
            "       XXXXX    ")
    },
    {
        /* [ぞ] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x305e,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "        XXX     "
            "    XXXXXX   X  "
            "       X   XX   "
            "     XX   XXXX  "
            " XXXX XXXX      "
            "     X          "
            "     XX         "
            "       XXXX     ")
    },
    {
        /* [た] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x305f,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "      X         "
            "  XXXXXXXX      "
            "     X          "
            "     X   XXXXX  "
            "    X           "
            "   XX   X       "
            "   X    X       "
            "  XX     XXXXX  ")
    },
    {
        /* [だ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3060,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "     XX      X  "
            "     X      X X "
            " XXXXXXXX       "
            "    XX  XXXXXX  "
            "    X           "
            "   X   X        "
            "  XX   X        "
            " XX     XXXXXX  ")
    },
    {
        /* [ち] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3061,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "     XX         "
            "  XXXXXXXXXXX   "
            "     X          "
            "    X    X      "
            "    XXXX  XXX   "
            "   X        XX  "
            "           XX   "
            "     XXXXXX     ")
    },
    {
        /* [ぢ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3062,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           XX X "
            "     X      X   "
            " XXXXXXXXXX     "
            "    X           "
            "   XXXXXXXXX    "
            "   X        X   "
            "           XX   "
            "     XXXXXX     ")
    },
    {
        /* [っ] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3063,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "                "
            "     XXXXXXX    "
            "   XX       XX  "
            "            X   "
            "          XX    "
            "      XXXX      ")
    },
    {
        /* [つ] freq=5 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3064,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "        XXX     "
            " XXXXXXX   XXX  "
            "             XX "
            "             X  "
            "           XX   "
            "     XXXXXX     "
            "                ")
    },
    {
        /* [づ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3065,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "             X  "
            "            X X "
            "     XXXXXXXX   "
            " XXXX        X  "
            "             X  "
            "            XX  "
            "          XX    "
            "     XXXXX      ")
    },
    {
        /* [て] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3066,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XXXXXXXXXXXX  "
            "        X       "
            "      XX        "
            "      X         "
            "      XX        "
            "       XX       "
            "         XXX    ")
    },
    {
        /* [で] freq=5 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3067,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XXXXXXXXXXXX  "
            "       XX    X  "
            "      XX   XX X "
            "      X         "
            "      XX        "
            "       XX       "
            "         XXX    ")
    },
    {
        /* [と] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3068,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    XX          "
            "     X   XXXX   "
            "      XXX       "
            "    XX          "
            "   XX           "
            "   XX           "
            "     XXXXXXXX   ")
    },
    {
        /* [ど] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3069,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "              X "
            "    X       XX  "
            "     X   XXX    "
            "     XXXX       "
            "    X           "
            "   X            "
            "   XX           "
            "     XXXXXXXX   ")
    },
    {
        /* [な] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x306a,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "      X         "
            "     XX         "
            "  XXXX XX  XXX  "
            "    XX    X     "
            "   XX     X     "
            "  XX      X     "
            "     XXXXXXXXX  "
            "      XXXXX     ")
    },
    {
        /* [に] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x306b,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   X   XXXXXXX  "
            "  XX            "
            "  X             "
            "  X             "
            "  X X  X        "
            "  XX   X        "
            "  XX    XXXXXX  ")
    },
    {
        /* [ぬ] freq=26 */
        CHAFA_SYMBOL_TAG_NONE,
        0x306c,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "        X       "
            "  X     XX      "
            "   XXXXXX XXX   "
            "  XXX  X     X  "
            "  X XX X     X  "
            " X   XX XXXXXX  "
            "  XXX   X   XXX "
            "         XXX    ")
    },
    {
        /* [ね] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x306d,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "     X          "
            "     X    X     "
            " XXXXXXXXX XX   "
            "    XX       X  "
            "   XX        X  "
            "  X X   XXXXXX  "
            " X  X   X   X X "
            "    XX   XXX    ")
    },
    {
        /* [の] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x306e,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "     XXXXXXX    "
            "   XX  XX   XX  "
            "  X    X     X  "
            " XX   XX     XX "
            "  X   X      X  "
            "  XXXX     XX   "
            "        XXX     ")
    },
    {
        /* [は] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x306f,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   X       X    "
            "  X   XXXXXXXX  "
            "  X        X    "
            "  X        X    "
            "  XX   XXXXXX   "
            "  XX  X    X XX "
            "  XX   XXXX     ")
    },
    {
        /* [ば] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3070,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            X X "
            "  X       X  X  "
            "  X   XXXXXXXX  "
            "  X       X     "
            " XX       XX    "
            " XXX  XXXXXXX   "
            "  X   X   XX XX "
            "  X    XXX      ")
    },
    {
        /* [ぱ] freq=51 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3071,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            XXXX"
            "  X       X  XX "
            "  X   XXXXXXXX  "
            "  X       X     "
            " XX       XX    "
            " XXX  XXXXXXX   "
            "  X   X   XX XX "
            "  X    XXX      ")
    },
    {
        /* [ひ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3072,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XXXXXX   X    "
            "    XX     XX   "
            "   XX      XX   "
            "   X       X XX "
            "  XX       X    "
            "   X      X     "
            "    XXXXXX      ")
    },
    {
        /* [び] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3073,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            X X "
            " XXXXXXX  XX X  "
            "    X      X    "
            "   X       XX   "
            "  XX       X X  "
            "  X       XX    "
            "  XX      X     "
            "    XXXXXX      ")
    },
    {
        /* [ぴ] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3074,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            XXX "
            " XXXXXXX  XXX X "
            "    X      X X  "
            "   X       XX   "
            "  XX       X X  "
            "  X        X    "
            "  XX      X     "
            "    XXXXXX      ")
    },
    {
        /* [ふ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3075,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "      X         "
            "       XXX      "
            "        XX      "
            "      X         "
            "       XX   X   "
            "    X    X   X  "
            " XXX     XX   X "
            "     XXXX       ")
    },
    {
        /* [ぶ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3076,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "      XXXX   XX "
            "        XX  X X "
            "      X         "
            "       XX  XX   "
            "   XX   XX   X  "
            " XX      X   XX "
            "     XXXX       ")
    },
    {
        /* [ぷ] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3077,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "      XXXX  XXX "
            "        XX  X X "
            "      X      X  "
            "       XX  XX   "
            "   XX   XX   X  "
            " XX      X   XX "
            "     XXXX       ")
    },
    {
        /* [へ] freq=25 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3078,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "     XXX        "
            "    X   XX      "
            "  XX      XX    "
            " X         XX   "
            "             XX "
            "                ")
    },
    {
        /* [べ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3079,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "             X  "
            "     XXX   X    "
            "   XX   XX      "
            "  XX      X     "
            " X         XX   "
            "             XX "
            "                ")
    },
    {
        /* [ぺ] freq=6 */
        CHAFA_SYMBOL_TAG_NONE,
        0x307a,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "           XXX  "
            "     XXX  X  X  "
            "   XX   XX XX   "
            "  XX      X     "
            " X         XX   "
            "             XX "
            "                ")
    },
    {
        /* [ほ] freq=9 */
        CHAFA_SYMBOL_TAG_NONE,
        0x307b,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XX   XXXXXXX  "
            "  X       XX    "
            "  X    XXXXXXX  "
            "  X        X    "
            "  XX   XXXXXX   "
            "  XX  X    X XX "
            "  XX   XXXX     ")
    },
    {
        /* [ぼ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x307c,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            X X "
            "  X   XXXXXX X  "
            "  X       X     "
            "  X   XXXXXXXX  "
            " XX       X     "
            " XXX      X     "
            "  X   XXXXXXXX  "
            "  X    XXX      ")
    },
    {
        /* [ぽ] freq=181 */
        CHAFA_SYMBOL_TAG_NONE,
        0x307d,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            XXXX"
            "  X   XXXXXX XX "
            "  X       X     "
            "  X   XXXXXXXX  "
            " XX       X     "
            " XXX      X     "
            "  X   XXXXXXXX  "
            "  X    XXX      ")
    },
    {
        /* [ま] freq=7 */
        CHAFA_SYMBOL_TAG_NONE,
        0x307e,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "        X       "
            "   XXXXXXXXXX   "
            "        X       "
            "   XXXXXXXXXXX  "
            "        X       "
            "    XXXXXXX     "
            "   X    X  XX   "
            "   XXXXXX       ")
    },
    {
        /* [み] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x307f,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    XXXXX       "
            "       XX       "
            "      XX    X   "
            "  XXXXX XXXXX   "
            " X   X     XXXX "
            " XXXX     XX    "
            "        XX      ")
    },
    {
        /* [む] freq=8 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3080,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "     X          "
            " XXXXXXXX  XX   "
            "     X       X  "
            "  XXXX          "
            "  X  X          "
            "  XXXX      X   "
            "    X       X   "
            "     XXXXXXX    ")
    },
    {
        /* [め] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3081,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   X     X      "
            "    XXXXXXXX    "
            "   XX   X   XX  "
            "  X  X XX    X  "
            " XX   XX     X  "
            "  XXXX     XX   "
            "         XX     ")
    },
    {
        /* [も] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3082,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "      X         "
            "      X         "
            "  XXXXXXXX      "
            "     X          "
            "  XXXXXXXX  XX  "
            "     X       X  "
            "     X      X   "
            "      XXXXXX    ")
    },
    {
        /* [ゃ] freq=6 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3083,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "       XX       "
            "    X  XXXXXX   "
            "   XXXX     X   "
            "  X   X XXXXX   "
            "      XX        "
            "       X        ")
    },
    {
        /* [や] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3084,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       X        "
            "   X    X XX    "
            "    XXXXXX  XX  "
            " XXX X       XX "
            "      X  XXXXX  "
            "      XX        "
            "       X        "
            "       XX       ")
    },
    {
        /* [ゅ] freq=5 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3085,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "    X   XX      "
            "   X  XX XXXX   "
            "   X X   X  XX  "
            "   XX X  X  XX  "
            "   XX  XXXXX    "
            "       XX       ")
    },
    {
        /* [ゆ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3086,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "        XX      "
            "  XX     X      "
            "  X  XXXXX XXX  "
            "  X X    X   X  "
            "  XX     X   X  "
            "  XX  X XX  XX  "
            "       XXXXX    "
            "      XX        ")
    },
    {
        /* [ょ] freq=14 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3087,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "       X        "
            "       XXXXX    "
            "       XX       "
            "       XX       "
            "   XXXXXXXXX    "
            "   XXXXX        ")
    },
    {
        /* [よ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3088,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       XX       "
            "       XX       "
            "       XXXXXX   "
            "       XX       "
            "       XX       "
            "   XXXXXXX      "
            "  X    XX XXXX  "
            "   XXXX         ")
    },
    {
        /* [ら] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3089,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "     X          "
            "      XXXXX     "
            "    X           "
            "   XX   XXX     "
            "   XXXXX   XX   "
            "   X        XX  "
            "           XX   "
            "     XXXXXX     ")
    },
    {
        /* [り] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x308a,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "    X           "
            "    X  XXXX     "
            "    XXX    X    "
            "   XX      XX   "
            "   XX      XX   "
            "           X    "
            "         XX     "
            "     XXXX       ")
    },
    {
        /* [る] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x308b,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    XXXXXXXX    "
            "        XX      "
            "      XX        "
            "   XXX XXXXXX   "
            "  X         XX  "
            "     XXXXX  X   "
            "      XXXXXX    ")
    },
    {
        /* [れ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x308c,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "     X          "
            "     X    X     "
            " XXXXXXXXX XX   "
            "    XX     XX   "
            "   XX      X    "
            "  X X      X    "
            " X  X      X    "
            "    X      XXXX ")
    },
    {
        /* [ろ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x308d,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    XXXXXXX     "
            "        XX      "
            "      XX X      "
            "   XXX XX XXX   "
            "  X         XX  "
            "           XX   "
            "     XXXXXX     ")
    },
    {
        /* [ゎ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x308e,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "     XX         "
            "   XXX XXXXXX   "
            "     XX      X  "
            "   XXX       X  "
            "  X  X     XX   "
            "     X   XX     ")
    },
    {
        /* [わ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x308f,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "     X          "
            "     X          "
            " XXXXX XXXXXX   "
            "    XXX      XX "
            "   XX         X "
            "  X X        X  "
            " X  X     XXX   "
            "    XX   X      ")
    },
    {
        /* [ゐ] freq=14 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3090,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       XX       "
            "    XXX X       "
            "       XXX      "
            "    XXXX  XXX   "
            "  XX  X      X  "
            "  X  XX      X  "
            " XX XX XXXXXXX  "
            "  XXX   XXX     ")
    },
    {
        /* [ゑ] freq=17 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3091,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "    XXXXXXX     "
            "       XX       "
            "    XXX  XXX    "
            "   X X XX   X   "
            "     XXXXXXX    "
            "    XX          "
            "   XX X  XXXX   "
            " XX    XX    XX ")
    },
    {
        /* [を] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3092,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "      X         "
            "  XXXXXXXXXX    "
            "     X          "
            "   XXXXXX XXXX  "
            "  X    XXX      "
            "     XX X       "
            "     X          "
            "      XXXXXXX   ")
    },
    {
        /* [ん] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3093,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       X        "
            "      XX        "
            "     XX         "
            "    XX X        "
            "    X X X       "
            "   X    XX    X "
            "  X     XX   X  "
            "  X      XXXX   ")
    },
    {
        /* [ゔ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3094,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "     XXXXXX  XX "
            "      XXXX  X   "
            "  XXXX    XX    "
            "           X    "
            "          XX    "
            "        XXX     "
            "     XXX        ")
    },
    {
        /* [ゕ] freq=5 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3095,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "      X         "
            "   X XXXX  XX   "
            "    XXX  XX  X  "
            "     X   XX  X  "
            "    X    X      "
            "   XX XXXX      ")
    },
    {
        /* [ゖ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x3096,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "    X     XX    "
            "   X   XXXXXXX  "
            "   X      XX    "
            "   X X    XX    "
            "   XX     X     "
            "    X   XX      ")
    },

/* Katakana */

    {
        /* [゠] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30a0,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "                "
            "      XXXX      "
            "      XXXX      "
            "                "
            "                "
            "                ")
    },
    {
        /* [ァ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30a1,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "   XXXXXXXXXX   "
            "           XX   "
            "       X  X     "
            "       X        "
            "      XX        "
            "    XX          ")
    },
    {
        /* [ア] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30a2,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XXXXXXXXXXXXX "
            "       X    X   "
            "       X  XX    "
            "       X        "
            "      XX        "
            "     XX         "
            "   XX           ")
    },
    {
        /* [ィ] freq=7 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30a3,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "           X    "
            "         XX     "
            "      XXX       "
            "  XXXX  X       "
            "        X       "
            "        X       ")
    },
    {
        /* [イ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30a4,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "            XX  "
            "         XXX    "
            "      XXX       "
            "  XXXX  X       "
            "        X       "
            "        X       "
            "        XX      ")
    },
    {
        /* [ゥ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30a5,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "       XX       "
            "   XXXXXXXXXX   "
            "   X       XX   "
            "           X    "
            "         XX     "
            "     XXXX       ")
    },
    {
        /* [ウ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30a6,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       XX       "
            "       XX       "
            "  XXXXXXXXXXXX  "
            "  XX        XX  "
            "  XX        X   "
            "          XX    "
            "        XXX     "
            "      XX        ")
    },
    {
        /* [ェ] freq=20 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30a7,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "                "
            "   XXXXXXXXXX   "
            "       XX       "
            "       XX       "
            "       XX       "
            "  XXXXXXXXXXXX  ")
    },
    {
        /* [エ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30a8,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XXXXXXXXXXXX  "
            "       XX       "
            "       XX       "
            "       XX       "
            "       XX       "
            " XXXXXXXXXXXXXX "
            "                ")
    },
    {
        /* [ォ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30a9,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "         X      "
            "   XXXXXXXXXXX  "
            "        XX      "
            "      XX X      "
            "    XX   X      "
            "   X   XXX      ")
    },
    {
        /* [オ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30aa,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "         XX     "
            "         X      "
            "  XXXXXXXXXXXX  "
            "        XXX     "
            "      XX XX     "
            "   XXX   XX     "
            "  X      XX     "
            "       XXX      ")
    },
    {
        /* [カ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ab,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       X        "
            "       X        "
            "  XXXXXXXXXXXX  "
            "      XX    XX  "
            "      X     X   "
            "     X      X   "
            "    X       X   "
            "  XX    XXXX    ")
    },
    {
        /* [ガ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ac,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "      XX   X X  "
            "      XX    X X "
            "  XXXXXXXXXXX   "
            "      X     X   "
            "     XX     X   "
            "     X      X   "
            "   XX      XX   "
            "  X     XXXX    ")
    },
    {
        /* [キ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ad,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "      XX        "
            "       XXXXXX   "
            "  XXXXXX        "
            "       XX XXXX  "
            "  XXXXX XX      "
            "        XX      "
            "         X      ")
    },
    {
        /* [ギ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ae,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           X X  "
            "      X     X X "
            " XXXXXXXXXXX    "
            "       X        "
            "     XXXXXXXXX  "
            "  XXX   X       "
            "        X       "
            "                ")
    },
    {
        /* [ク] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30af,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       X        "
            "      XXXXXXX   "
            "    XX      X   "
            "  XX       XX   "
            "          XX    "
            "         X      "
            "      XXX       "
            "    XX          ")
    },
    {
        /* [グ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30b0,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "      X       X "
            "      X     X X "
            "     XXXXXXXX   "
            "   XX      X    "
            " XX       XX    "
            "         X      "
            "      XXX       "
            "   XXX          ")
    },
    {
        /* [ケ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30b1,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "     XX         "
            "     X          "
            "    XXXXXXXXXXX "
            "  XX     X      "
            "        XX      "
            "        X       "
            "      XX        "
            "     X          ")
    },
    {
        /* [ゲ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30b2,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "             X  "
            "    XX      X X "
            "    XXXXXXXXXX  "
            "   X     X      "
            " XX     XX      "
            "        X       "
            "      XX        "
            "    XXX         ")
    },
    {
        /* [コ] freq=6 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30b3,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   XXXXXXXXXXX  "
            "            XX  "
            "            XX  "
            "            XX  "
            "            XX  "
            "   XXXXXXXXXXX  "
            "            XX  ")
    },
    {
        /* [ゴ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30b4,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           X XX "
            "  XXXXXXXXXXX   "
            "            X   "
            "            X   "
            "            X   "
            "            X   "
            "  XXXXXXXXXXX   "
            "            X   ")
    },
    {
        /* [サ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30b5,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "          XX    "
            "    XX    XX    "
            " XXXXXXXXXXXXXX "
            "    XX    XX    "
            "    XX    X     "
            "          X     "
            "        XX      "
            "      XX        ")
    },
    {
        /* [ザ] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30b6,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "              X "
            "    X     X XXX "
            " XXXXXXXXXXXXX  "
            "    X     X     "
            "    X     X     "
            "         XX     "
            "        XX      "
            "      XX        ")
    },
    {
        /* [シ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30b7,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    XX          "
            "      XX        "
            "  XXX        XX "
            "           XX   "
            "         XX     "
            "   XXXXXX       "
            "                ")
    },
    {
        /* [ジ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30b8,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    X      X X  "
            "     XXX    X X "
            "  XX         X  "
            "    X       X   "
            "         XXX    "
            "      XXX       "
            "   XXX          ")
    },
    {
        /* [ス] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30b9,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   XXXXXXXXXX   "
            "          XX    "
            "         XX     "
            "        XX      "
            "      XX  XX    "
            "   XXX      X   "
            "  X          X  ")
    },
    {
        /* [ズ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ba,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           XX X "
            "   XXXXXXXXXX   "
            "          XX    "
            "         XX     "
            "        XX      "
            "      XX XX     "
            "    XX     XX   "
            "  XX        XX  ")
    },
    {
        /* [セ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30bb,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "     X          "
            "     X          "
            "     X  XXXXXX  "
            " XXXXXXX   XX   "
            "     X    XX    "
            "     X          "
            "     X          "
            "      XXXXXXX   ")
    },
    {
        /* [ゼ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30bc,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           X  X "
            "     X      X   "
            "     X   XXXXX  "
            " XXXXXXXX  XX   "
            "     X    XX    "
            "     X          "
            "     X          "
            "      XXXXXXX   ")
    },
    {
        /* [ソ] freq=5 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30bd,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XX        X   "
            "   XX      XX   "
            "    X      X    "
            "          X     "
            "        XX      "
            "     XXX        "
            "                ")
    },
    {
        /* [ゾ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30be,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           X X  "
            "  X         X X "
            "   X       XX   "
            "    XX     X    "
            "          X     "
            "        XX      "
            "     XXX        "
            "                ")
    },
    {
        /* [タ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30bf,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       X        "
            "      XXXXXXX   "
            "     X      X   "
            "   XX X    XX   "
            "  X    XXXXX    "
            "        XXXX    "
            "      XX        "
            "   XXX          ")
    },
    {
        /* [ダ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30c0,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "      XX    X X "
            "     XXXXXXXX   "
            "    XX     XX   "
            "  XX  X    X    "
            "       XX X     "
            "        XXXX    "
            "      XX        "
            "   XXX          ")
    },
    {
        /* [チ] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30c1,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   XXXXXXXXX    "
            "        X       "
            " XXXXXXXXXXXXXX "
            "       XX       "
            "       X        "
            "      X         "
            "    XX          ")
    },
    {
        /* [ヂ] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30c2,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XXXXXXXXXX X  "
            "       X    X X "
            " XXXXXXXXXXXXX  "
            "       X        "
            "       X        "
            "     XX         "
            "   XX           ")
    },
    {
        /* [ッ] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30c3,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "                "
            "   X   X    X   "
            "    X   X  XX   "
            "          XX    "
            "        XX      "
            "     XXX        ")
    },
    {
        /* [ツ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30c4,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "      XX     X  "
            "  XX   X    XX  "
            "   X        X   "
            "           X    "
            "         XX     "
            "     XXXX       "
            "                ")
    },
    {
        /* [ヅ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30c5,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           X  X "
            "      X     X   "
            "  X    X    XX  "
            "   X       XX   "
            "          XX    "
            "        XX      "
            "     XXX        "
            "                ")
    },
    {
        /* [テ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30c6,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   XXXXXXXXXX   "
            "                "
            "  XXXXXXXXXXXXX "
            "        X       "
            "       XX       "
            "      XX        "
            "    XX          ")
    },
    {
        /* [デ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30c7,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            X X "
            "   XXXXXXXXX    "
            "                "
            " XXXXXXXXXXXXX  "
            "       XX       "
            "       X        "
            "      XX        "
            "    XX          ")
    },
    {
        /* [ト] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30c8,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "     XX         "
            "     XX         "
            "     XXXX       "
            "     XX  XXXXX  "
            "     XX         "
            "     XX         "
            "                ")
    },
    {
        /* [ド] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30c9,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "     X    X X   "
            "     X     X    "
            "     XXX        "
            "     X  XXXXX   "
            "     X          "
            "     X          "
            "     X          ")
    },
    {
        /* [ナ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ca,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "        X       "
            "        X       "
            "  XXXXXXXXXXXXX "
            "        X       "
            "       XX       "
            "       X        "
            "     XX         "
            "    X           ")
    },
    {
        /* [ニ] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30cb,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   XXXXXXXXXX   "
            "                "
            "                "
            "                "
            "                "
            " XXXXXXXXXXXXX  "
            "                ")
    },
    {
        /* [ヌ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30cc,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   XXXXXXXXXX   "
            "           X    "
            "    XXX   X     "
            "       XXX      "
            "       XXXX     "
            "     XX    XX   "
            "   XX           ")
    },
    {
        /* [ネ] freq=5 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30cd,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       XX       "
            "  XXXXXXXXXXX   "
            "          XX    "
            "        XX      "
            "     XXXX XX    "
            " XXXX  XX   XXX "
            "       XX       "
            "       XX       ")
    },
    {
        /* [ノ] freq=6 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ce,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "           XX   "
            "          XX    "
            "         XX     "
            "        XX      "
            "      XX        "
            "  XXXX          "
            "                ")
    },
    {
        /* [ハ] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30cf,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "          X     "
            "     X    XX    "
            "    XX     X    "
            "    X       X   "
            "   X        XX  "
            "  X          XX "
            "                ")
    },
    {
        /* [バ] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30d0,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "            X X "
            "     X    X     "
            "    XX     X    "
            "    X       X   "
            "   X        XX  "
            " XX          X  "
            "                ")
    },
    {
        /* [パ] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30d1,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            XXX "
            "     X   XX X X "
            "     X    XX X  "
            "    XX     X    "
            "    X       X   "
            "   X        XX  "
            "  X          X  "
            "                ")
    },
    {
        /* [ヒ] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30d2,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    X           "
            "    X     XXX   "
            "    XXXXXX      "
            "    X           "
            "    X           "
            "    X           "
            "     XXXXXXXX   ")
    },
    {
        /* [ビ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30d3,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           X X  "
            "   X        X   "
            "   X      XX    "
            "   XXXXXXX      "
            "   X            "
            "   X            "
            "   X            "
            "    XXXXXXXXX   ")
    },
    {
        /* [ピ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30d4,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            XXX "
            "   X        X X "
            "   X     XXX X  "
            "   XXXXXX       "
            "   X            "
            "   X            "
            "   X            "
            "    XXXXXXXXX   ")
    },
    {
        /* [フ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30d5,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XXXXXXXXXXXX  "
            "            X   "
            "           XX   "
            "          XX    "
            "         XX     "
            "      XXX       "
            "    XX          ")
    },
    {
        /* [ブ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30d6,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            X X "
            "  XXXXXXXXXXX   "
            "            X   "
            "           XX   "
            "          XX    "
            "         XX     "
            "      XXX       "
            "    XX          ")
    },
    {
        /* [プ] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30d7,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            XXXX"
            "  XXXXXXXXXXX  X"
            "            XXX "
            "           XX   "
            "          XX    "
            "         XX     "
            "      XXX       "
            "    XX          ")
    },
    {
        /* [ヘ] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30d8,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "     XXX        "
            "    X   XX      "
            "  XX      XX    "
            " X          XX  "
            "             XX "
            "                ")
    },
    {
        /* [ベ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30d9,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "             X  "
            "     XXX   X    "
            "   XX   XX      "
            "  XX      XX    "
            " X         XX   "
            "             XX "
            "                ")
    },
    {
        /* [ペ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30da,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "           XXX  "
            "     XXX   X X  "
            "   XX   XX  X   "
            "  XX      XX    "
            " X         XX   "
            "             XX "
            "                ")
    },
    {
        /* [ホ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30db,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "        X       "
            "        X       "
            "  XXXXXXXXXXXX  "
            "        X       "
            "    X   X   X   "
            "  XX    X    X  "
            "        X       "
            "     XXX        ")
    },
    {
        /* [ボ] freq=14 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30dc,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       XX  X X  "
            "       XX   X X "
            " XXXXXXXXXXXXX  "
            "       XX       "
            "   XX  XX  XX   "
            "  X    XX    X  "
            " X     XX       "
            "     XXX        ")
    },
    {
        /* [ポ] freq=32 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30dd,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           XXXX "
            "       XX   XX  "
            " XXXXXXXXXXXXX  "
            "       XX       "
            "   XX  XX  XX   "
            "  XX   XX   XX  "
            "       XX       "
            "     XXX        ")
    },
    {
        /* [マ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30de,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XXXXXXXXXXXX  "
            "            XX  "
            "           XX   "
            "    XX   XX     "
            "      XXX       "
            "        XX      "
            "          X     ")
    },
    {
        /* [ミ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30df,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    XXXXXX      "
            "          XXX   "
            "    XXXX        "
            "        XXXX    "
            "   X            "
            "    XXXXXXXX    "
            "                ")
    },
    {
        /* [ム] freq=8 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30e0,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "      XX        "
            "      X         "
            "     XX         "
            "     X     X    "
            "    X       X   "
            " XXXXXXXXXXX X  "
            "                ")
    },
    {
        /* [メ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30e1,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "           XX   "
            "    X     XX    "
            "     XXX XX     "
            "        XXX     "
            "      XX   XX   "
            "   XXX          "
            "                ")
    },
    {
        /* [モ] freq=9 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30e2,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   XXXXXXXXXX   "
            "      XX        "
            "  XXXXXXXXXXXXX "
            "      XX        "
            "      XX        "
            "      XX        "
            "        XXXXXX  ")
    },
    {
        /* [ャ] freq=10 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30e3,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "     X          "
            "     XX    XXX  "
            "   XXXXXXXXXX   "
            "       X XX     "
            "       X        "
            "        X       ")
    },
    {
        /* [ヤ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30e4,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    XX          "
            "     X   XXXXX  "
            " XXXXXXXX   X   "
            "      X   XX    "
            "      XX        "
            "       X        "
            "       XX       ")
    },
    {
        /* [ュ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30e5,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "                "
            "    XXXXXXXX    "
            "          X     "
            "          X     "
            "  XXXXXXXXXXXX  "
            "                ")
    },
    {
        /* [ユ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30e6,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   XXXXXXXXX    "
            "           X    "
            "           X    "
            "          XX    "
            "          XX    "
            " XXXXXXXXXXXXXX "
            "                ")
    },
    {
        /* [ョ] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30e7,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "    XXXXXXXX    "
            "           X    "
            "    XXXXXXXX    "
            "           X    "
            "           X    "
            "   XXXXXXXXX    ")
    },
    {
        /* [ヨ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30e8,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   XXXXXXXXXX   "
            "            X   "
            "            X   "
            "   XXXXXXXXXX   "
            "            X   "
            "  XXXXXXXXXXX   "
            "            X   ")
    },
    {
        /* [ラ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30e9,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    XXXXXXXXX   "
            "                "
            "  XXXXXXXXXXXX  "
            "            X   "
            "           X    "
            "        XXX     "
            "     XXX        ")
    },
    {
        /* [リ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ea,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    X      X    "
            "    X      X    "
            "    X      X    "
            "    X      X    "
            "          XX    "
            "       XXX      "
            "                ")
    },
    {
        /* [ル] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30eb,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    XX  XX      "
            "    XX  XX      "
            "    X   XX      "
            "    X   XX    X "
            "   XX   XX  XX  "
            "  XX    XXXX    "
            "                ")
    },
    {
        /* [レ] freq=6 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ec,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "    X           "
            "    X           "
            "    X           "
            "    X       XX  "
            "    X    XXX    "
            "    XXXXX       "
            "                ")
    },
    {
        /* [ロ] freq=653 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ed,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XXXXXXXXXXXX  "
            "  XX        XX  "
            "  XX        XX  "
            "  XX        XX  "
            "  XX        XX  "
            "  XXXXXXXXXXXX  "
            "  XX        XX  ")
    },
    {
        /* [ヮ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ee,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "   XXXXXXXXXX   "
            "   X        X   "
            "   X       X    "
            "          XX    "
            "         XX     "
            "     XXXX       ")
    },
    {
        /* [ワ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30ef,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XXXXXXXXXXXX  "
            "  XX        XX  "
            "  XX        X   "
            "           XX   "
            "          XX    "
            "       XXX      "
            "      X         ")
    },
    {
        /* [ヰ] freq=2 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30f0,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "          X     "
            "  XXXXXXXXXXXX  "
            "     X    X     "
            "     X    X     "
            " XXXXXXXXXXXXXX "
            "          X     "
            "          X     ")
    },
    {
        /* [ヱ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30f1,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "  XXXXXXXXXXXX  "
            "            X   "
            "       X  XX    "
            "       X        "
            "       X        "
            " XXXXXXXXXXXXXX "
            "                ")
    },
    {
        /* [ヲ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30f2,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   XXXXXXXXXXX  "
            "            XX  "
            "   XXXXXXXXXX   "
            "           X    "
            "          X     "
            "       XXX      "
            "     XX         ")
    },
    {
        /* [ン] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30f3,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "   XX           "
            "     XX         "
            "            XX  "
            "           X    "
            "        XXX     "
            "  XXXXXX        "
            "                ")
    },
    {
        /* [ヴ] freq=0 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30f4,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "       X   X XX "
            "       X    X   "
            "  XXXXXXXXXXXX  "
            "  X         X   "
            "  X        XX   "
            "          XX    "
            "        XX      "
            "     XXX        ")
    },
    {
        /* [ヵ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30f5,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "       X        "
            "   XXXXXXXXX    "
            "      XX   XX   "
            "      X    XX   "
            "     XX    X    "
            "   XX   XXXX    ")
    },
    {
        /* [ヶ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30f6,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "     XX         "
            "     XXXXXXXXX  "
            "   XX    X      "
            "        XX      "
            "       XX       "
            "     XXX        ")
    },
    {
        /* [ヷ] freq=3 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30f7,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           X XX "
            "  XXXXXXXXXXX   "
            "  X         X   "
            "  X         X   "
            "           X    "
            "          XX    "
            "        XX      "
            "     XXX        ")
    },
    {
        /* [ヸ] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30f8,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "            X X "
            "          X  X  "
            "  XXXXXXXXXXXX  "
            "     X    X     "
            "     X    X     "
            " XXXXXXXXXXXXXX "
            "          X     "
            "          X     ")
    },
    {
        /* [ヹ] freq=1 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30f9,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           X XX "
            "  XXXXXXXXXXXX  "
            "            X   "
            "       X  XX    "
            "       X        "
            "       X        "
            " XXXXXXXXXXXXXX "
            "                ")
    },
    {
        /* [ヺ] freq=4 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30fa,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "           X XX "
            "  XXXXXXXXXXX   "
            "            X   "
            "  XXXXXXXXXXX   "
            "          XX    "
            "         XX     "
            "       XX       "
            "    XXX         ")
    },
    {
        /* [・] freq=99 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30fb,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "                "
            "      XXXX      "
            "       XX       "
            "                "
            "                "
            "                ")
    },
    {
        /* [ー] freq=5 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30fc,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "                "
            "  XXXXXXXXXXXX  "
            "                "
            "                "
            "                "
            "                ")
    },
    {
        /* [ヽ] freq=19 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30fd,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "                "
            "     X          "
            "      XX        "
            "        XX      "
            "          XX    "
            "                "
            "                ")
    },
    {
        /* [ヾ] freq=5 */
        CHAFA_SYMBOL_TAG_NONE,
        0x30fe,
        CHAFA_SYMBOL_OUTLINE_16X8 (
            "                "
            "         X X    "
            "     X    X     "
            "      XX        "
            "        XX      "
            "          X     "
            "                "
            "                ")
    },
