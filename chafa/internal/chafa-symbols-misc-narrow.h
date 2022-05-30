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

/* Miscellaneous single-cell symbols
 * ---------------------------------
 *
 * This is meant to be #included in the symbol definition table of
 * chafa-symbols.c. It's kept in a separate file due to its size. */

    {
        /* Horizontal Scan Line 1 */
        CHAFA_SYMBOL_TAG_TECHNICAL,
        0x23ba,
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
        /* Horizontal Scan Line 3 */
        CHAFA_SYMBOL_TAG_TECHNICAL,
        0x23bb,
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
        /* Horizontal Scan Line 7 */
        CHAFA_SYMBOL_TAG_TECHNICAL,
        0x23bc,
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
        /* Horizontal Scan Line 9 */
        CHAFA_SYMBOL_TAG_TECHNICAL,
        0x23bd,
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
    /* Begin dot characters */
    {
        CHAFA_SYMBOL_TAG_DOT,
        0x25ae, /* Black vertical rectangle */
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            " XXXXXX "
            " XXXXXX "
            " XXXXXX "
            " XXXXXX "
            " XXXXXX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_DOT,
        0x25a0, /* Black square */
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " XXXXXX "
            " XXXXXX "
            " XXXXXX "
            " XXXXXX "
            "        "
            "        ")
    },
    {
        /* Has an emoji variant that may show up unbidden */
        CHAFA_SYMBOL_TAG_DOT | CHAFA_SYMBOL_TAG_AMBIGUOUS,
        0x25aa, /* Black small square */
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "  XXXX  "
            "  XXXX  "
            "  XXXX  "
            "  XXXX  "
            "        "
            "        ")
    },
    {
        /* Black up-pointing triangle */
        CHAFA_SYMBOL_TAG_GEOMETRIC,
        0x25b2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "  XXXX  "
            " XXXXXX "
            " XXXXXX "
            "XXXXXXXX"
            "        "
            "        ")
    },
    {
        /* Black right-pointing triangle */
        /* Has an emoji variant that may show up unbidden */
        CHAFA_SYMBOL_TAG_GEOMETRIC | CHAFA_SYMBOL_TAG_AMBIGUOUS,
        0x25b6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " X      "
            " XXX    "
            " XXXX   "
            " XXXXXX "
            " XXXX   "
            " XXX    "
            " X      "
            "        ")
    },
    {
        /* Black down-pointing triangle */
        CHAFA_SYMBOL_TAG_GEOMETRIC,
        0x25bc,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "XXXXXXXX"
            " XXXXXX "
            " XXXXXX "
            "  XXXX  "
            "   XX   "
            "        "
            "        ")
    },
    {
        /* Black left-pointing triangle */
        /* Has an emoji variant that may show up unbidden */
        CHAFA_SYMBOL_TAG_GEOMETRIC | CHAFA_SYMBOL_TAG_AMBIGUOUS,
        0x25c0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "      X "
            "    XXX "
            "   XXXX "
            " XXXXXX "
            "   XXXX "
            "    XXX "
            "      X "
            "        ")
    },
    {
        /* Black diamond */
        /* Depending on font, may exceed cell boundaries on VTE */
        CHAFA_SYMBOL_TAG_GEOMETRIC | CHAFA_SYMBOL_TAG_AMBIGUOUS,
        0x25c6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "  XXXX  "
            " XXXXXX "
            "  XXXX  "
            "   XX   "
            "        "
            "        ")
    },
    {
        /* Black Circle */
        CHAFA_SYMBOL_TAG_GEOMETRIC,
        0x25cf,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " XXXXXX "
            " XXXXXX "
            " XXXXXX "
            "  XXXX  "
            "        "
            "        ")
    },
    {
        /* Black Lower Right Triangle */
        CHAFA_SYMBOL_TAG_GEOMETRIC | CHAFA_SYMBOL_TAG_AMBIGUOUS,
        0x25e2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "     XX "
            "    XXX "
            "   XXXX "
            " XXXXXX "
            "        "
            "        ")
    },
    {
        /* Black Lower Left Triangle */
        CHAFA_SYMBOL_TAG_GEOMETRIC | CHAFA_SYMBOL_TAG_AMBIGUOUS,
        0x25e3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " XX     "
            " XXX    "
            " XXXX   "
            " XXXXXX "
            "        "
            "        ")
    },
    {
        /* Black Upper Left Triangle */
        CHAFA_SYMBOL_TAG_GEOMETRIC | CHAFA_SYMBOL_TAG_AMBIGUOUS,
        0x25e4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " XXXXXX "
            " XXXX   "
            " XXX    "
            " XX     "
            "        "
            "        ")
    },
    {
        /* Black Upper Right Triangle */
        CHAFA_SYMBOL_TAG_GEOMETRIC | CHAFA_SYMBOL_TAG_AMBIGUOUS,
        0x25e5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " XXXXXX "
            "   XXXX "
            "    XXX "
            "     XX "
            "        "
            "        ")
    },
    {
        /* Black Medium Square */
        /* Has en emoji variant that may show up unbidden. See:
         * https://github.com/hpjansson/chafa/issues/52 */
        CHAFA_SYMBOL_TAG_GEOMETRIC | CHAFA_SYMBOL_TAG_AMBIGUOUS,
        0x25fc,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "  XXXX  "
            "  XXXX  "
            "  XXXX  "
            "  XXXX  "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_DOT,
        0x00b7, /* Middle dot */
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "   XX   "
            "   XX   "
            "        "
            "        "
            "        ")
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x00b7, /* Middle dot */
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "  XX    "
            "  XX    "
            "        "
            "        "
            "        ")
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x00b7, /* Middle dot */
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "    XX  "
            "    XX  "
            "        "
            "        "
            "        ")
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x00b7, /* Middle dot */
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "   XX   "
            "   XX   "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* Variant */
        CHAFA_SYMBOL_TAG_DOT,
        0x00b7, /* Middle dot */
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "   XX   "
            "   XX   "
            "        "
            "        ")
    },
    {
        /* Greek Capital Letter Xi */
        CHAFA_SYMBOL_TAG_EXTRA,
        0x039e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "        "
            "  XXXX  "
            "        "
            " XXXXXX "
            "        "
            "        ")
    },
