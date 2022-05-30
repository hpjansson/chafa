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

/* This is meant to be #included in the symbol definition table of
 * chafa-symbols.c. It's kept in a separate file due to its size.
 *
 * The symbol bitmaps are derived from https://github.com/dhepper/font8x8 by
 * Daniel Hepper <daniel@hepper.net>. Excerpt from the accompanying README:
 *
 * 8x8 monochrome bitmap font for rendering
 * ========================================
 *
 * A collection of header files containing a 8x8 bitmap font.
 *
 * [...]
 * 
 * Author: Daniel Hepper <daniel@hepper.net>
 * License: Public Domain
 *
 * Credits
 * =======
 *
 * These header files are directly derived from an assembler file fetched from:
 * http://dimensionalrift.homelinux.net/combuster/mos3/?p=viewsource&file=/modules/gfx/font8_8.asm
 *
 * Original header:
 *
 * ; Summary: font8_8.asm
 * ; 8x8 monochrome bitmap fonts for rendering
 * ;
 * ; Author:
 * ;     Marcel Sondaar
 * ;     International Business Machines (public domain VGA fonts)
 * ;
 * ; License:
 * ;     Public Domain
 */

    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_SPACE,
        ' ',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '!',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  XXXX  "
            "  XXXX  "
            "   XX   "
            "   XX   "
            "        "
            "   XX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '"',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XX XX  "
            " XX XX  "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '#',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XX XX  "
            " XX XX  "
            "XXXXXXX "
            " XX XX  "
            "XXXXXXX "
            " XX XX  "
            " XX XX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '$',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX    "
            " XXXXX  "
            "XX      "
            " XXXX   "
            "    XX  "
            "XXXXX   "
            "  XX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '%',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "XX   XX "
            "XX  XX  "
            "   XX   "
            "  XX    "
            " XX  XX "
            "XX   XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '&',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            " XX XX  "
            "  XXX   "
            " XXX XX "
            "XX XXX  "
            "XX  XX  "
            " XXX XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        0x27,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XX     "
            " XX     "
            "XX      "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '(',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  XX    "
            " XX     "
            " XX     "
            " XX     "
            "  XX    "
            "   XX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        ')',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XX     "
            "  XX    "
            "   XX   "
            "   XX   "
            "   XX   "
            "  XX    "
            " XX     "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '*',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XX  XX "
            "  XXXX  "
            "XXXXXXXX"
            "  XXXX  "
            " XX  XX "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '+',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX    "
            "  XX    "
            "XXXXXX  "
            "  XX    "
            "  XX    "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        ',',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "  XX    "
            "  XX    "
            " XX     ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '-',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "XXXXXX  "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '.',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "  XX    "
            "  XX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '/',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "     XX "
            "    XX  "
            "   XX   "
            "  XX    "
            " XX     "
            "XX      "
            "X       "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '0',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXXX  "
            "XX   XX "
            "XX  XXX "
            "XX XXXX "
            "XXXX XX "
            "XXX  XX "
            " XXXXX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '1',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX    "
            " XXX    "
            "  XX    "
            "  XX    "
            "  XX    "
            "  XX    "
            "XXXXXX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '2',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXX   "
            "XX  XX  "
            "    XX  "
            "  XXX   "
            " XX     "
            "XX  XX  "
            "XXXXXX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '3',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXX   "
            "XX  XX  "
            "    XX  "
            "  XXX   "
            "    XX  "
            "XX  XX  "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '4',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            "  XXXX  "
            " XX XX  "
            "XX  XX  "
            "XXXXXXX "
            "    XX  "
            "   XXXX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '5',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXX  "
            "XX      "
            "XXXXX   "
            "    XX  "
            "    XX  "
            "XX  XX  "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '6',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            " XX     "
            "XX      "
            "XXXXX   "
            "XX  XX  "
            "XX  XX  "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '7',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXX  "
            "XX  XX  "
            "    XX  "
            "   XX   "
            "  XX    "
            "  XX    "
            "  XX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '8',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXX   "
            "XX  XX  "
            "XX  XX  "
            " XXXX   "
            "XX  XX  "
            "XX  XX  "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '9',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXX   "
            "XX  XX  "
            "XX  XX  "
            " XXXXX  "
            "    XX  "
            "   XX   "
            " XXX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        ':',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX    "
            "  XX    "
            "        "
            "        "
            "  XX    "
            "  XX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        ';',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX    "
            "  XX    "
            "        "
            "        "
            "  XX    "
            "  XX    "
            " XX     ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '<',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  XX    "
            " XX     "
            "XX      "
            " XX     "
            "  XX    "
            "   XX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '=',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XXXXXX  "
            "        "
            "        "
            "XXXXXX  "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '>',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XX     "
            "  XX    "
            "   XX   "
            "    XX  "
            "   XX   "
            "  XX    "
            " XX     "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '?',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXX   "
            "XX  XX  "
            "    XX  "
            "   XX   "
            "  XX    "
            "        "
            "  XX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '@',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXXX  "
            "XX   XX "
            "XX XXXX "
            "XX XXXX "
            "XX XXXX "
            "XX      "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'A',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX    "
            " XXXX   "
            "XX  XX  "
            "XX  XX  "
            "XXXXXX  "
            "XX  XX  "
            "XX  XX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'B',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXX  "
            " XX  XX "
            " XX  XX "
            " XXXXX  "
            " XX  XX "
            " XX  XX "
            "XXXXXX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'C',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " XX  XX "
            "XX      "
            "XX      "
            "XX      "
            " XX  XX "
            "  XXXX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'D',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXX   "
            " XX XX  "
            " XX  XX "
            " XX  XX "
            " XX  XX "
            " XX XX  "
            "XXXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'E',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXX "
            " XX   X "
            " XX X   "
            " XXXX   "
            " XX X   "
            " XX   X "
            "XXXXXXX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'F',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXX "
            " XX   X "
            " XX X   "
            " XXXX   "
            " XX X   "
            " XX     "
            "XXXX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'G',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " XX  XX "
            "XX      "
            "XX      "
            "XX  XXX "
            " XX  XX "
            "  XXXXX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'H',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "XXXXXX  "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'I',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXX   "
            "  XX    "
            "  XX    "
            "  XX    "
            "  XX    "
            "  XX    "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'J',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXXX "
            "    XX  "
            "    XX  "
            "    XX  "
            "XX  XX  "
            "XX  XX  "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'K',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXX  XX "
            " XX  XX "
            " XX XX  "
            " XXXX   "
            " XX XX  "
            " XX  XX "
            "XXX  XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'L',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXX    "
            " XX     "
            " XX     "
            " XX     "
            " XX   X "
            " XX  XX "
            "XXXXXXX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'M',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX   XX "
            "XXX XXX "
            "XXXXXXX "
            "XXXXXXX "
            "XX X XX "
            "XX   XX "
            "XX   XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'N',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX   XX "
            "XXX  XX "
            "XXXX XX "
            "XX XXXX "
            "XX  XXX "
            "XX   XX "
            "XX   XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'O',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            " XX XX  "
            "XX   XX "
            "XX   XX "
            "XX   XX "
            " XX XX  "
            "  XXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'P',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXX  "
            " XX  XX "
            " XX  XX "
            " XXXXX  "
            " XX     "
            " XX     "
            "XXXX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'Q',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXX   "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "XX XXX  "
            " XXXX   "
            "   XXX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'R',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXX  "
            " XX  XX "
            " XX  XX "
            " XXXXX  "
            " XX XX  "
            " XX  XX "
            "XXX  XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'S',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXX   "
            "XX  XX  "
            "XXX     "
            " XXX    "
            "   XXX  "
            "XX  XX  "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'T',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXX  "
            "X XX X  "
            "  XX    "
            "  XX    "
            "  XX    "
            "  XX    "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'U',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "XXXXXX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'V',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            " XXXX   "
            "  XX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'W',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX   XX "
            "XX   XX "
            "XX   XX "
            "XX X XX "
            "XXXXXXX "
            "XXX XXX "
            "XX   XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'X',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX   XX "
            "XX   XX "
            " XX XX  "
            "  XXX   "
            "  XXX   "
            " XX XX  "
            "XX   XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'Y',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            " XXXX   "
            "  XX    "
            "  XX    "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'Z',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXXXXX "
            "XX   XX "
            "X   XX  "
            "   XX   "
            "  XX  X "
            " XX  XX "
            "XXXXXXX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '[',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXX   "
            " XX     "
            " XX     "
            " XX     "
            " XX     "
            " XX     "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '\\',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX      "
            " XX     "
            "  XX    "
            "   XX   "
            "    XX  "
            "     XX "
            "      X "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        ']',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            "   XX   "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '^',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "  XXX   "
            " XX XX  "
            "XX   XX "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '_',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "XXXXXXX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '`',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX    "
            "  XX    "
            "   XX   "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'a',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " XXXX   "
            "    XX  "
            " XXXXX  "
            "XX  XX  "
            " XXX XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'b',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXX     "
            " XX     "
            " XX     "
            " XXXXX  "
            " XX  XX "
            " XX  XX "
            "XX XXX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'c',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " XXXX   "
            "XX  XX  "
            "XX      "
            "XX  XX  "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'd',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            "    XX  "
            "    XX  "
            " XXXXX  "
            "XX  XX  "
            "XX  XX  "
            " XXX XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'e',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " XXXX   "
            "XX  XX  "
            "XXXXXX  "
            "XX      "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'f',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            " XX XX  "
            " XX     "
            "XXXX    "
            " XX     "
            " XX     "
            "XXXX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'g',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " XXX XX "
            "XX  XX  "
            "XX  XX  "
            " XXXXX  "
            "    XX  "
            "XXXXX   ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'h',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXX     "
            " XX     "
            " XX XX  "
            " XXX XX "
            " XX  XX "
            " XX  XX "
            "XXX  XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'i',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX    "
            "        "
            " XXX    "
            "  XX    "
            "  XX    "
            "  XX    "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'j',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    XX  "
            "        "
            "    XX  "
            "    XX  "
            "    XX  "
            "XX  XX  "
            "XX  XX  "
            " XXXX   ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'k',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXX     "
            " XX     "
            " XX  XX "
            " XX XX  "
            " XXXX   "
            " XX XX  "
            "XXX  XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'l',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXX    "
            "  XX    "
            "  XX    "
            "  XX    "
            "  XX    "
            "  XX    "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'm',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XX  XX  "
            "XXXXXXX "
            "XXXXXXX "
            "XX X XX "
            "XX   XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'n',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XXXXX   "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'o',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " XXXX   "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            " XXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'p',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XX XXX  "
            " XX  XX "
            " XX  XX "
            " XXXXX  "
            " XX     "
            "XXXX    ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'q',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " XXX XX "
            "XX  XX  "
            "XX  XX  "
            " XXXXX  "
            "    XX  "
            "   XXXX ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'r',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XX XXX  "
            " XXX XX "
            " XX  XX "
            " XX     "
            "XXXX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        's',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " XXXXX  "
            "XX      "
            " XXXX   "
            "    XX  "
            "XXXXX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        't',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "  XX    "
            " XXXXX  "
            "  XX    "
            "  XX    "
            "  XX X  "
            "   XX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'u',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            " XXX XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'v',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            " XXXX   "
            "  XX    "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'w',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XX   XX "
            "XX X XX "
            "XXXXXXX "
            "XXXXXXX "
            " XX XX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'x',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XX   XX "
            " XX XX  "
            "  XXX   "
            " XX XX  "
            "XX   XX "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'y',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XX  XX  "
            "XX  XX  "
            "XX  XX  "
            " XXXXX  "
            "    XX  "
            "XXXXX   ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'z',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XXXXXX  "
            "X  XX   "
            "  XX    "
            " XX  X  "
            "XXXXXX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '{',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            "  XX    "
            "  XX    "
            "XXX     "
            "  XX    "
            "  XX    "
            "   XXX  "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '|',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "   XX   "
            "   XX   "
            "        "
            "   XX   "
            "   XX   "
            "   XX   "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '}',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXX     "
            "  XX    "
            "  XX    "
            "   XXX  "
            "  XX    "
            "  XX    "
            "XXX     "
            "        ")
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '~',
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXX XX "
            "XX XXX  "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
