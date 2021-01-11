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
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '!',
        "   XX   "
        "  XXXX  "
        "  XXXX  "
        "   XX   "
        "   XX   "
        "        "
        "   XX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '"',
        " XX XX  "
        " XX XX  "
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '#',
        " XX XX  "
        " XX XX  "
        "XXXXXXX "
        " XX XX  "
        "XXXXXXX "
        " XX XX  "
        " XX XX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '$',
        "  XX    "
        " XXXXX  "
        "XX      "
        " XXXX   "
        "    XX  "
        "XXXXX   "
        "  XX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '%',
        "        "
        "XX   XX "
        "XX  XX  "
        "   XX   "
        "  XX    "
        " XX  XX "
        "XX   XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '&',
        "  XXX   "
        " XX XX  "
        "  XXX   "
        " XXX XX "
        "XX XXX  "
        "XX  XX  "
        " XXX XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        0x27,
        " XX     "
        " XX     "
        "XX      "
        "        "
        "        "
        "        "
        "        "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '(',
        "   XX   "
        "  XX    "
        " XX     "
        " XX     "
        " XX     "
        "  XX    "
        "   XX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        ')',
        " XX     "
        "  XX    "
        "   XX   "
        "   XX   "
        "   XX   "
        "  XX    "
        " XX     "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '*',
        "        "
        " XX  XX "
        "  XXXX  "
        "XXXXXXXX"
        "  XXXX  "
        " XX  XX "
        "        "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '+',
        "        "
        "  XX    "
        "  XX    "
        "XXXXXX  "
        "  XX    "
        "  XX    "
        "        "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        ',',
        "        "
        "        "
        "        "
        "        "
        "        "
        "  XX    "
        "  XX    "
        " XX     "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '-',
        "        "
        "        "
        "        "
        "XXXXXX  "
        "        "
        "        "
        "        "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '.',
        "        "
        "        "
        "        "
        "        "
        "        "
        "  XX    "
        "  XX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '/',
        "     XX "
        "    XX  "
        "   XX   "
        "  XX    "
        " XX     "
        "XX      "
        "X       "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '0',
        " XXXXX  "
        "XX   XX "
        "XX  XXX "
        "XX XXXX "
        "XXXX XX "
        "XXX  XX "
        " XXXXX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '1',
        "  XX    "
        " XXX    "
        "  XX    "
        "  XX    "
        "  XX    "
        "  XX    "
        "XXXXXX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '2',
        " XXXX   "
        "XX  XX  "
        "    XX  "
        "  XXX   "
        " XX     "
        "XX  XX  "
        "XXXXXX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '3',
        " XXXX   "
        "XX  XX  "
        "    XX  "
        "  XXX   "
        "    XX  "
        "XX  XX  "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '4',
        "   XXX  "
        "  XXXX  "
        " XX XX  "
        "XX  XX  "
        "XXXXXXX "
        "    XX  "
        "   XXXX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '5',
        "XXXXXX  "
        "XX      "
        "XXXXX   "
        "    XX  "
        "    XX  "
        "XX  XX  "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '6',
        "  XXX   "
        " XX     "
        "XX      "
        "XXXXX   "
        "XX  XX  "
        "XX  XX  "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '7',
        "XXXXXX  "
        "XX  XX  "
        "    XX  "
        "   XX   "
        "  XX    "
        "  XX    "
        "  XX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '8',
        " XXXX   "
        "XX  XX  "
        "XX  XX  "
        " XXXX   "
        "XX  XX  "
        "XX  XX  "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_DIGIT,
        '9',
        " XXXX   "
        "XX  XX  "
        "XX  XX  "
        " XXXXX  "
        "    XX  "
        "   XX   "
        " XXX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        ':',
        "        "
        "  XX    "
        "  XX    "
        "        "
        "        "
        "  XX    "
        "  XX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        ';',
        "        "
        "  XX    "
        "  XX    "
        "        "
        "        "
        "  XX    "
        "  XX    "
        " XX     "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '<',
        "   XX   "
        "  XX    "
        " XX     "
        "XX      "
        " XX     "
        "  XX    "
        "   XX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '=',
        "        "
        "        "
        "XXXXXX  "
        "        "
        "        "
        "XXXXXX  "
        "        "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '>',
        " XX     "
        "  XX    "
        "   XX   "
        "    XX  "
        "   XX   "
        "  XX    "
        " XX     "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '?',
        " XXXX   "
        "XX  XX  "
        "    XX  "
        "   XX   "
        "  XX    "
        "        "
        "  XX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '@',
        " XXXXX  "
        "XX   XX "
        "XX XXXX "
        "XX XXXX "
        "XX XXXX "
        "XX      "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'A',
        "  XX    "
        " XXXX   "
        "XX  XX  "
        "XX  XX  "
        "XXXXXX  "
        "XX  XX  "
        "XX  XX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'B',
        "XXXXXX  "
        " XX  XX "
        " XX  XX "
        " XXXXX  "
        " XX  XX "
        " XX  XX "
        "XXXXXX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'C',
        "  XXXX  "
        " XX  XX "
        "XX      "
        "XX      "
        "XX      "
        " XX  XX "
        "  XXXX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'D',
        "XXXXX   "
        " XX XX  "
        " XX  XX "
        " XX  XX "
        " XX  XX "
        " XX XX  "
        "XXXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'E',
        "XXXXXXX "
        " XX   X "
        " XX X   "
        " XXXX   "
        " XX X   "
        " XX   X "
        "XXXXXXX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'F',
        "XXXXXXX "
        " XX   X "
        " XX X   "
        " XXXX   "
        " XX X   "
        " XX     "
        "XXXX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'G',
        "  XXXX  "
        " XX  XX "
        "XX      "
        "XX      "
        "XX  XXX "
        " XX  XX "
        "  XXXXX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'H',
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "XXXXXX  "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'I',
        " XXXX   "
        "  XX    "
        "  XX    "
        "  XX    "
        "  XX    "
        "  XX    "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'J',
        "   XXXX "
        "    XX  "
        "    XX  "
        "    XX  "
        "XX  XX  "
        "XX  XX  "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'K',
        "XXX  XX "
        " XX  XX "
        " XX XX  "
        " XXXX   "
        " XX XX  "
        " XX  XX "
        "XXX  XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'L',
        "XXXX    "
        " XX     "
        " XX     "
        " XX     "
        " XX   X "
        " XX  XX "
        "XXXXXXX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'M',
        "XX   XX "
        "XXX XXX "
        "XXXXXXX "
        "XXXXXXX "
        "XX X XX "
        "XX   XX "
        "XX   XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'N',
        "XX   XX "
        "XXX  XX "
        "XXXX XX "
        "XX XXXX "
        "XX  XXX "
        "XX   XX "
        "XX   XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'O',
        "  XXX   "
        " XX XX  "
        "XX   XX "
        "XX   XX "
        "XX   XX "
        " XX XX  "
        "  XXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'P',
        "XXXXXX  "
        " XX  XX "
        " XX  XX "
        " XXXXX  "
        " XX     "
        " XX     "
        "XXXX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'Q',
        " XXXX   "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "XX XXX  "
        " XXXX   "
        "   XXX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'R',
        "XXXXXX  "
        " XX  XX "
        " XX  XX "
        " XXXXX  "
        " XX XX  "
        " XX  XX "
        "XXX  XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'S',
        " XXXX   "
        "XX  XX  "
        "XXX     "
        " XXX    "
        "   XXX  "
        "XX  XX  "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'T',
        "XXXXXX  "
        "X XX X  "
        "  XX    "
        "  XX    "
        "  XX    "
        "  XX    "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'U',
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "XXXXXX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'V',
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        " XXXX   "
        "  XX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'W',
        "XX   XX "
        "XX   XX "
        "XX   XX "
        "XX X XX "
        "XXXXXXX "
        "XXX XXX "
        "XX   XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'X',
        "XX   XX "
        "XX   XX "
        " XX XX  "
        "  XXX   "
        "  XXX   "
        " XX XX  "
        "XX   XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'Y',
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        " XXXX   "
        "  XX    "
        "  XX    "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'Z',
        "XXXXXXX "
        "XX   XX "
        "X   XX  "
        "   XX   "
        "  XX  X "
        " XX  XX "
        "XXXXXXX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '[',
        " XXXX   "
        " XX     "
        " XX     "
        " XX     "
        " XX     "
        " XX     "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '\\',
        "XX      "
        " XX     "
        "  XX    "
        "   XX   "
        "    XX  "
        "     XX "
        "      X "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        ']',
        " XXXX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        "   XX   "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '^',
        "   X    "
        "  XXX   "
        " XX XX  "
        "XX   XX "
        "        "
        "        "
        "        "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '_',
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
        "XXXXXXX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '`',
        "  XX    "
        "  XX    "
        "   XX   "
        "        "
        "        "
        "        "
        "        "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'a',
        "        "
        "        "
        " XXXX   "
        "    XX  "
        " XXXXX  "
        "XX  XX  "
        " XXX XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'b',
        "XXX     "
        " XX     "
        " XX     "
        " XXXXX  "
        " XX  XX "
        " XX  XX "
        "XX XXX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'c',
        "        "
        "        "
        " XXXX   "
        "XX  XX  "
        "XX      "
        "XX  XX  "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'd',
        "   XXX  "
        "    XX  "
        "    XX  "
        " XXXXX  "
        "XX  XX  "
        "XX  XX  "
        " XXX XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'e',
        "        "
        "        "
        " XXXX   "
        "XX  XX  "
        "XXXXXX  "
        "XX      "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'f',
        "  XXX   "
        " XX XX  "
        " XX     "
        "XXXX    "
        " XX     "
        " XX     "
        "XXXX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'g',
        "        "
        "        "
        " XXX XX "
        "XX  XX  "
        "XX  XX  "
        " XXXXX  "
        "    XX  "
        "XXXXX   "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'h',
        "XXX     "
        " XX     "
        " XX XX  "
        " XXX XX "
        " XX  XX "
        " XX  XX "
        "XXX  XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'i',
        "  XX    "
        "        "
        " XXX    "
        "  XX    "
        "  XX    "
        "  XX    "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'j',
        "    XX  "
        "        "
        "    XX  "
        "    XX  "
        "    XX  "
        "XX  XX  "
        "XX  XX  "
        " XXXX   "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'k',
        "XXX     "
        " XX     "
        " XX  XX "
        " XX XX  "
        " XXXX   "
        " XX XX  "
        "XXX  XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'l',
        " XXX    "
        "  XX    "
        "  XX    "
        "  XX    "
        "  XX    "
        "  XX    "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'm',
        "        "
        "        "
        "XX  XX  "
        "XXXXXXX "
        "XXXXXXX "
        "XX X XX "
        "XX   XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'n',
        "        "
        "        "
        "XXXXX   "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'o',
        "        "
        "        "
        " XXXX   "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        " XXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'p',
        "        "
        "        "
        "XX XXX  "
        " XX  XX "
        " XX  XX "
        " XXXXX  "
        " XX     "
        "XXXX    "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'q',
        "        "
        "        "
        " XXX XX "
        "XX  XX  "
        "XX  XX  "
        " XXXXX  "
        "    XX  "
        "   XXXX "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'r',
        "        "
        "        "
        "XX XXX  "
        " XXX XX "
        " XX  XX "
        " XX     "
        "XXXX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        's',
        "        "
        "        "
        " XXXXX  "
        "XX      "
        " XXXX   "
        "    XX  "
        "XXXXX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        't',
        "   X    "
        "  XX    "
        " XXXXX  "
        "  XX    "
        "  XX    "
        "  XX X  "
        "   XX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'u',
        "        "
        "        "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        " XXX XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'v',
        "        "
        "        "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        " XXXX   "
        "  XX    "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'w',
        "        "
        "        "
        "XX   XX "
        "XX X XX "
        "XXXXXXX "
        "XXXXXXX "
        " XX XX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'x',
        "        "
        "        "
        "XX   XX "
        " XX XX  "
        "  XXX   "
        " XX XX  "
        "XX   XX "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'y',
        "        "
        "        "
        "XX  XX  "
        "XX  XX  "
        "XX  XX  "
        " XXXXX  "
        "    XX  "
        "XXXXX   "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_ALPHA,
        'z',
        "        "
        "        "
        "XXXXXX  "
        "X  XX   "
        "  XX    "
        " XX  X  "
        "XXXXXX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '{',
        "   XXX  "
        "  XX    "
        "  XX    "
        "XXX     "
        "  XX    "
        "  XX    "
        "   XXX  "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '|',
        "   XX   "
        "   XX   "
        "   XX   "
        "        "
        "   XX   "
        "   XX   "
        "   XX   "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '}',
        "XXX     "
        "  XX    "
        "  XX    "
        "   XXX  "
        "  XX    "
        "  XX    "
        "XXX     "
        "        "
    },
    {
        CHAFA_SYMBOL_TAG_ASCII,
        '~',
        " XXX XX "
        "XX XXX  "
        "        "
        "        "
        "        "
        "        "
        "        "
        "        "
    },
