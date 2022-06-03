/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2022 Hans Petter Jansson
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
 * These are Latin symbols not in the ASCII 7-bit range. The bitmaps are a
 * close match to the Terminus font (specifically ter-x14n.pcf).
 */

    {
        /* [¡] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xa1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "        "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [¢] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xa2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "   X    "
            "XXXXXXX "
            "X  X    "
            "X  X  X "
            " XXXXX  "
            "   X    ")
    },
    {
        /* [£] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xa3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "  X  X  "
            " XXXX   "
            "  X     "
            "  X     "
            " XXXXXX "
            "        ")
    },
    {
        /* [¤] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xa4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            " X   X  "
            " XXXXX  "
            " X   X  "
            "  XXX   "
            " X   X  "
            "        ")
    },
    {
        /* [¥] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xa5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "X     X "
            " X   X  "
            "  XXX   "
            " XXXXX  "
            " XXXXX  "
            "   X    "
            "        ")
    },
    {
        /* [¦] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xa6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "   X    "
            "   X    "
            "        "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [§] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xa7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            " X   X  "
            "  XX    "
            " X  XX  "
            " XX  X  "
            "   XX   "
            " X   X  "
            "  XXX   ")
    },
    {
        /* [¨] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xa8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [©] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xa9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XXXXXXXX"
            "X XXXX X"
            "X X    X"
            "X  XXX X"
            " XXXXXX "
            "        ")
    },
    {
        /* [ª] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xaa,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            "     X  "
            " XX  X  "
            "  XXX   "
            " XXXXX  "
            "        "
            "        "
            "        ")
    },
    {
        /* [«] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xab,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "  XX XX "
            "XX XX   "
            " XX XX  "
            "   X  X "
            "        ")
    },
    {
        /* [¬] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xac,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            "      X "
            "        "
            "        "
            "        ")
    },
    {
        /* [®] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xae,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "XXXXXXXX"
            "X X  X X"
            "X XXX  X"
            "X X  X X"
            " XXXXXX "
            "        ")
    },
    {
        /* [¯] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xaf,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [°] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xb0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  X  X  "
            "   XX   "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [±] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xb1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "   X    "
            " XXXXX  "
            "   X    "
            " XXXXX  "
            "        ")
    },
    {
        /* [²] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xb2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  X  X  "
            "    X   "
            "  XXXX  "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [³] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xb3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            "    XX  "
            "     X  "
            "  XXXX  "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [´] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xb4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [µ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xb5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " X    X "
            " X    X "
            " X   XX "
            " XXXX X "
            " X      ")
    },
    {
        /* [¶] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xb6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "X  X  X "
            "X  X  X "
            " XXX  X "
            "   X  X "
            "   X  X "
            "        ")
    },
    {
        /* [·] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xb7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "   X    "
            "   X    "
            "        "
            "        "
            "        ")
    },
    {
        /* [¸] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xb8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "   X    "
            "  X     ")
    },
    {
        /* [¹] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xb9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "  XX    "
            "   X    "
            "  XXX   "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [º] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xba,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            " X   X  "
            " X   X  "
            "  XXX   "
            " XXXXX  "
            "        "
            "        "
            "        ")
    },
    {
        /* [»] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xbb,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "XX XX   "
            "  XX XX "
            " XX XX  "
            "X  X    "
            "        ")
    },
    {
        /* [¼] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xbc,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XX     "
            "  X   X "
            "  X  X  "
            "   XX   "
            " XX  XX "
            "X   X X "
            "    XXX "
            "      X ")
    },
    {
        /* [½] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xbd,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XX     "
            "  X   X "
            "  X  X  "
            "   XX   "
            " XX XX  "
            "X  X  X "
            "    X   "
            "   XXXX ")
    },
    {
        /* [¾] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xbe,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XXXX    "
            " XX   X "
            "XXX  X  "
            "   XX   "
            " XX  XX "
            "X   X X "
            "    XXX "
            "      X ")
    },
    {
        /* [¿] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xbf,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "        "
            "   X    "
            " XX   X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [À] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xc0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Á] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xc1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Â] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xc2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Ã] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xc3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXXXX "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Ä] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xc4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Å] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xc5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Æ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xc6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "X  X    "
            "XXXXXX  "
            "X  X    "
            "X  X    "
            "X  XXXX "
            "        ")
    },
    {
        /* [Ç] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xc7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X      "
            " X      "
            " X    X "
            "  XXXX  "
            "  X     ")
    },
    {
        /* [È] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xc8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            " XXXXXX "
            " X      "
            " X      "
            " XXXX   "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [É] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xc9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            " XXXXXX "
            " X      "
            " X      "
            " XXXX   "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [Ê] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xca,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " XXXXXX "
            " X      "
            " X      "
            " XXXX   "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [Ë] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xcb,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            " XXXXXX "
            " X      "
            " X      "
            " XXXX   "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [Ì] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xcc,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX    "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Í] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xcd,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Î] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xce,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ï] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xcf,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " X   X  "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ð] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xd0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXX  "
            " X    X "
            "XXXX  X "
            " X    X "
            " X    X "
            " XXXXX  "
            "        ")
    },
    {
        /* [Ñ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xd1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXXXX "
            " X    X "
            " X    X "
            " XXX  X "
            " X  XXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Ò] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xd2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ó] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xd3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ô] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xd4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Õ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xd5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXXXX "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ö] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xd6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [×] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xd7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XX  XX "
            "   XX   "
            "  X  X  "
            " X    X "
            "        ")
    },
    {
        /* [Ø] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xd8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    XX"
            " X  XXX "
            " XXX  X "
            "XX    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ù] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xd9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXX   "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ú] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xda,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Û] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xdb,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ü] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xdc,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ý] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xdd,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            "X     X "
            " X   X  "
            " XX XX  "
            "   X    "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [Þ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xde,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            " XXXXX  "
            " X    X "
            " X    X "
            " XXXXX  "
            " X      "
            "        ")
    },
    {
        /* [ß] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xdf,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXX   "
            " X   X  "
            " XXXXX  "
            " X    X "
            " XX   X "
            " X XXX  "
            "        ")
    },
    {
        /* [à] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xe0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "    X   "
            "  XXXXX "
            " XXXXXX "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [á] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xe1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            "  XXXXX "
            " XXXXXX "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [â] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xe2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "        "
            "  XXXXX "
            " XXXXXX "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [ã] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xe3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX  X "
            " X  XX  "
            "  XXXXX "
            " XXXXXX "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [ä] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xe4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "        "
            "  XXXXX "
            " XXXXXX "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [å] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xe5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "   XX   "
            "  XXXXX "
            " XXXXXX "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [æ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xe6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " XXX  X "
            "X  XXXX "
            " XX XX  "
            "        ")
    },
    {
        /* [ç] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xe7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X      "
            " X    X "
            "  XXXX  "
            "  X     ")
    },
    {
        /* [è] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xe8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "    X   "
            " XXXXXX "
            " X XX X "
            " X      "
            "  XXXX  "
            "        ")
    },
    {
        /* [é] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xe9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            " XXXXXX "
            " X XX X "
            " X      "
            "  XXXX  "
            "        ")
    },
    {
        /* [ê] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xea,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "        "
            " XXXXXX "
            " X XX X "
            " X      "
            "  XXXX  "
            "        ")
    },
    {
        /* [ë] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xeb,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "        "
            " XXXXXX "
            " X XX X "
            " X      "
            "  XXXX  "
            "        ")
    },
    {
        /* [ì] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xec,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X     "
            "   X    "
            "  XX    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [í] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xed,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            "  XX    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [î] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xee,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX    "
            " X  X   "
            "  XX    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [ï] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xef,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X  X   "
            "        "
            "  XX    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [ð] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xf0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X X   "
            "  X XX  "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ñ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xf1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX  X "
            " X  XX  "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [ò] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xf2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "    X   "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ó] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xf3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ô] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xf4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [õ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xf5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX  X "
            " X  XX  "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ö] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xf6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [÷] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xf7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "   X    "
            "   X    "
            " XXXXX  "
            "   X    "
            "        "
            "        ")
    },
    {
        /* [ø] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xf8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "      X "
            " XXXXXX "
            " X XX X "
            " XX   X "
            "X XXXX  "
            "        ")
    },
    {
        /* [ù] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xf9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "    X   "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [ú] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xfa,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [û] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xfb,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "  X  X  "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [ü] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xfc,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "        "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [ý] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xfd,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [þ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xfe,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            " X      "
            " XXXXXX "
            " X    X "
            " X    X "
            " XXXXX  "
            " X      ")
    },
    {
        /* [ÿ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0xff,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "        "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [Ā] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x100,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [ā] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x101,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "   XX   "
            "  XXXXX "
            " XXXXXX "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [Ă] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x102,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X XX  "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [ă] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x103,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "        "
            "  XXXXX "
            " XXXXXX "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [Ą] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x104,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "     XXX")
    },
    {
        /* [ą] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x105,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "  XXXXX "
            " XXXXXX "
            " X    X "
            "  XXXXX "
            "      XX")
    },
    {
        /* [Ć] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x106,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            "  XXXX  "
            " X    X "
            " X      "
            " X      "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ć] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x107,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "        "
            " XXXXXX "
            " X      "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ĉ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x108,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X      "
            " X      "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ĉ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x109,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "        "
            " XXXXXX "
            " X      "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ċ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x10a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "  XXXX  "
            " X    X "
            " X      "
            " X      "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ċ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x10b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "        "
            " XXXXXX "
            " X      "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Č] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x10c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X      "
            " X      "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [č] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x10d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " XXXXXX "
            " X      "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ď] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x10e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            " XXXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            " XXXXX  "
            "        ")
    },
    {
        /* [ď] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x10f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            "   XX X "
            "      X "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [Đ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x110,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXX  "
            " X    X "
            "XXXX  X "
            " X    X "
            " X    X "
            " XXXXX  "
            "        ")
    },
    {
        /* [đ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x111,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "      X "
            "    XXX "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [Ē] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x112,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " XXXXXX "
            " X      "
            " XXXX   "
            " X      "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ē] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x113,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "  XXX   "
            " XXXXXX "
            " X XX X "
            " X      "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ĕ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x114,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " XXXXXX "
            " X      "
            " XXXX   "
            " X      "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ĕ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x115,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " XXXXXX "
            " X XX X "
            " X      "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ė] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x116,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            " XXXXXX "
            " X      "
            " XXXX   "
            " X      "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ė] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x117,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "        "
            " XXXXXX "
            " X XX X "
            " X      "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ę] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x118,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            " X      "
            " XXXX   "
            " X      "
            " X      "
            " XXXXXX "
            "      XX")
    },
    {
        /* [ę] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x119,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X XX X "
            " X      "
            "  XXXX  "
            "     XX ")
    },
    {
        /* [Ě] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x11a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " XXXXXX "
            " X      "
            " XXXX   "
            " X      "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ě] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x11b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " XXXXXX "
            " X XX X "
            " X      "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ĝ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x11c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X      "
            " X  XXX "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ĝ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x11d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "  X  X  "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [Ğ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x11e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X      "
            " X  XXX "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ğ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x11f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [Ġ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x120,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "  XXXX  "
            " X    X "
            " X      "
            " X  XXX "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ġ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x121,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [Ģ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x122,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X      "
            " X  XXX "
            " X    X "
            "  XXXX  "
            "  X     ")
    },
    {
        /* [ģ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x123,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "     X  "
            "    X   "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [Ĥ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x124,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "        "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [ĥ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x125,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXXX "
            " X      "
            " X      "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Ħ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x126,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            "XXX  XXX"
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [ħ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x127,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            " XXX    "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Ĩ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x128,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXXXX "
            "        "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [ĩ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x129,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX XX "
            " X  X   "
            "  XX    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ī] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x12a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXXX  "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [ī] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x12b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "  XXX   "
            "  XX    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ĭ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x12c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [ĭ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x12d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X  X   "
            "        "
            "  XX    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Į] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x12e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "    XX  ")
    },
    {
        /* [į] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x12f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "        "
            "  XX    "
            "   X    "
            "   X    "
            "  XXX   "
            "    XX  ")
    },
    {
        /* [İ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x130,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [ı] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x131,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "  XX    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ĳ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x132,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "XXX  XXX"
            " X    X "
            " X    X "
            " X    X "
            " X X  X "
            "XXX XX  "
            "        ")
    },
    {
        /* [ĳ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x133,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            "        "
            "XX   XX "
            " X    X "
            " X    X "
            "XXXX  X "
            "    XX  ")
    },
    {
        /* [Ĵ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x134,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    XX  "
            "   X  X "
            "     X  "
            "     X  "
            "     X  "
            " X   X  "
            "  XXX   "
            "        ")
    },
    {
        /* [ĵ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x135,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    XX  "
            "   X  X "
            "    XX  "
            "     X  "
            "     X  "
            " X   X  "
            "  XXX   ")
    },
    {
        /* [Ķ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x136,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            " X  XX  "
            " XXX    "
            " XXX    "
            " X  XX  "
            " X X  X "
            "  X     ")
    },
    {
        /* [ķ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x137,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            " X      "
            " X   XX "
            " XXXX   "
            " X  XX  "
            " X X  X "
            "  X     ")
    },
    {
        /* [ĸ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x138,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " X   XX "
            " XXXX   "
            " X  XX  "
            " X    X "
            "        ")
    },
    {
        /* [Ĺ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x139,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XX     "
            "        "
            " X      "
            " X      "
            " X      "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ĺ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x13a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  XX    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ļ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x13b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            " X      "
            " X      "
            " X      "
            " X      "
            " XXXXXX "
            "  X     ")
    },
    {
        /* [ļ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x13c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "  X     ")
    },
    {
        /* [Ľ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x13d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            " X XX   "
            " X      "
            " X      "
            " X      "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ľ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x13e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXX   "
            "  XX    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ŀ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x13f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            " X      "
            " X   X  "
            " X   X  "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ŀ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x140,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX    "
            "   X    "
            "   X   X"
            "   X   X"
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ł] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x141,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            " X      "
            " XX     "
            "XX      "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ł] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x142,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX    "
            "   X    "
            "   XX   "
            "  XX    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ń] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x143,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            " X    X "
            " X    X "
            " XXX  X "
            " X  XXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [ń] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x144,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Ņ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x145,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            " X    X "
            " XXX  X "
            " X  XXX "
            " X    X "
            " X X  X "
            "  X     ")
    },
    {
        /* [ņ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x146,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            " X X  X "
            "  X     ")
    },
    {
        /* [Ň] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x147,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            " X XX X "
            " X    X "
            " XXX  X "
            " X  XXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [ň] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x148,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [ŉ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x149,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " X      "
            " X      "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Ŋ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x14a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            " X    X "
            " XXX  X "
            " X  XXX "
            " X    X "
            " X    X "
            "    XX  ")
    },
    {
        /* [ŋ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x14b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "    XX  ")
    },
    {
        /* [Ō] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x14c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ō] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x14d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "   XX   "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ŏ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x14e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            "   XX   "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ŏ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x14f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ő] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x150,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX XX "
            "        "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ő] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x151,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X  X "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Œ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x152,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "X  X    "
            "X  XXX  "
            "X  X    "
            "X  X    "
            " XXXXXX "
            "        ")
    },
    {
        /* [œ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x153,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "XXXXXXX "
            "X  XXXX "
            "X  X    "
            " XXXXXX "
            "        ")
    },
    {
        /* [Ŕ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x154,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            " XXXXX  "
            " X    X "
            " X    X "
            " XXXXX  "
            " X  X   "
            " X   XX "
            "        ")
    },
    {
        /* [ŕ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x155,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "        "
            " XXXXXX "
            " X      "
            " X      "
            " X      "
            "        ")
    },
    {
        /* [Ŗ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x156,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXX  "
            " X    X "
            " X    X "
            " XXXXX  "
            " X  X   "
            " X X XX "
            "  X     ")
    },
    {
        /* [ŗ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x157,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X      "
            " X      "
            " XX     "
            " X      ")
    },
    {
        /* [Ř] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x158,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " XXXXX  "
            " X    X "
            " X    X "
            " XXXXX  "
            " X  X   "
            " X   XX "
            "        ")
    },
    {
        /* [ř] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x159,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " XXXXXX "
            " X      "
            " X      "
            " X      "
            "        ")
    },
    {
        /* [Ś] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x15a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  XXXX  "
            " X    X "
            " X      "
            "  XXXXX "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ś] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x15b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "     X  "
            "    X   "
            " XXXXXX "
            " XXXXX  "
            "      X "
            " XXXXX  "
            "        ")
    },
    {
        /* [Ŝ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x15c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X      "
            "  XXXXX "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ŝ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x15d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "  X  X  "
            " XXXXXX "
            " XXXXX  "
            "      X "
            " XXXXX  "
            "        ")
    },
    {
        /* [Ş] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x15e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " XXXXX  "
            "      X "
            " X    X "
            "  XXXX  "
            "  X     ")
    },
    {
        /* [ş] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x15f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " XXXXX  "
            "      X "
            " XXXXX  "
            "  X     ")
    },
    {
        /* [Š] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x160,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X      "
            "  XXXXX "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [š] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x161,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " XXXXXX "
            " XXXXX  "
            "      X "
            " XXXXX  "
            "        ")
    },
    {
        /* [Ţ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x162,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "XXXXXXX "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "   XX   "
            "   X    ")
    },
    {
        /* [ţ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x163,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "   X    "
            " XXXXX  "
            "   X    "
            "   X    "
            "    XXX "
            "    X   ")
    },
    {
        /* [Ť] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x164,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "        "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [ť] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x165,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "        "
            "   X    "
            " XXXXX  "
            "   X    "
            "   X    "
            "    XXX "
            "        ")
    },
    {
        /* [Ŧ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x166,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "XXXXXXX "
            "   X    "
            " XXXXX  "
            "   X    "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [ŧ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x167,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "   X    "
            " XXXXX  "
            "  XXX   "
            "   X    "
            "    XXX "
            "        ")
    },
    {
        /* [Ũ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x168,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX  X "
            " X  XX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ũ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x169,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX  X "
            "    XX  "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [Ū] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x16a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ū] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x16b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [Ŭ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x16c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            " X XX X "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ŭ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x16d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [Ů] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x16e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "   XX   "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ů] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x16f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "   XX   "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [Ű] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x170,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX XX "
            "        "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ű] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x171,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X  X "
            "  X  X  "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [Ų] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x172,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "     XX ")
    },
    {
        /* [ų] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x173,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "      XX")
    },
    {
        /* [Ŵ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x174,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "        "
            "X     X "
            "X     X "
            "X  X  X "
            "X X X X "
            "XX   XX "
            "        ")
    },
    {
        /* [ŵ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x175,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "  X  X  "
            "X     X "
            "X  X  X "
            "X  X  X "
            " XXXXX  "
            "        ")
    },
    {
        /* [Ŷ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x176,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "        "
            "X     X "
            " X   X  "
            "  X X   "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [ŷ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x177,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "  X  X  "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [Ÿ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x178,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " X   X  "
            "        "
            "X     X "
            " X   X  "
            "  X X   "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [Ź] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x179,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            " XXXXXX "
            "      X "
            "    XX  "
            "  XX    "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ź] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x17a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            " XXXXXX "
            "   XX   "
            "  X     "
            " XXXXXX "
            "        ")
    },
    {
        /* [Ż] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x17b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            " XXXXXX "
            "      X "
            "    XX  "
            "  XX    "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ż] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x17c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "        "
            " XXXXXX "
            "   XX   "
            "  X     "
            " XXXXXX "
            "        ")
    },
    {
        /* [Ž] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x17d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " XXXXXX "
            "      X "
            "    XX  "
            "  XX    "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ž] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x17e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " XXXXXX "
            "   XX   "
            "  X     "
            " XXXXXX "
            "        ")
    },
    {
        /* [ſ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x17f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    XXX "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [Ɔ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x186,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            "      X "
            "      X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ǝ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x18e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "      X "
            "   XXXX "
            "      X "
            "      X "
            " XXXXXX "
            "        ")
    },
    {
        /* [Ə] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x18f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            "      X "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ɛ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x190,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            "  XXX   "
            " X      "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ƒ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x192,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    XX  "
            "   X  X "
            " XXXXX  "
            "   X    "
            "   X    "
            "   X    "
            "XXX     ")
    },
    {
        /* [Ɲ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x19d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            " X    X "
            " XXX  X "
            " X  XXX "
            " X    X "
            " X    X "
            "X       ")
    },
    {
        /* [ƞ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x19e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "      X ")
    },
    {
        /* [Ƶ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1b5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "     X  "
            " XXXXXX "
            "  XX    "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [ƶ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1b6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            "  XXXX  "
            "   X    "
            " XXXXXX "
            "        ")
    },
    {
        /* [Ʒ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1b7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "     X  "
            "   XXX  "
            "      X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ǎ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1cd,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [ǎ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1ce,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            "  XXXXX "
            " XXXXXX "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [Ǐ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1cf,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [ǐ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1d0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X  X   "
            "  XX    "
            "  XX    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ǒ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1d1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ǒ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1d2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ǔ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1d3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ǔ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1d4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [Ǣ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1e2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXXX  "
            " XXXXXX "
            "X  X    "
            "XXXXXX  "
            "X  X    "
            "X  X    "
            "X  XXXX "
            "        ")
    },
    {
        /* [ǣ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1e3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "   XX   "
            " XXXXXX "
            " XXXXXX "
            "X  X    "
            " XX XX  "
            "        ")
    },
    {
        /* [Ǥ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1e4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X      "
            " X  XXX "
            " X  XXXX"
            "  XXXX  "
            "        ")
    },
    {
        /* [ǥ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1e5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X  XXXX"
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [Ǧ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1e6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X      "
            " X  XXX "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ǧ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1e7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [Ǩ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1e8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " X    X "
            " X  XX  "
            " XXX    "
            " XXX    "
            " X  XX  "
            " X    X "
            "        ")
    },
    {
        /* [ǩ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1e9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            " X      "
            " X      "
            " X   XX "
            " XXXX   "
            " X  XX  "
            " X    X "
            "        ")
    },
    {
        /* [Ǫ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1ea,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "     XX ")
    },
    {
        /* [ǫ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1eb,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "     XX ")
    },
    {
        /* [Ǭ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1ec,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XXXX  "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "     XX ")
    },
    {
        /* [ǭ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1ed,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "  XXX   "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "     XX ")
    },
    {
        /* [Ǯ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1ee,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XXX  "
            "  XXXXX "
            "     X  "
            "   XXX  "
            "      X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ǯ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1ef,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "   XX   "
            " XXXXXX "
            "     X  "
            "   XXX  "
            "      X "
            " XXXXX  ")
    },
    {
        /* [ǰ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1f0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X  X "
            "        "
            "    XX  "
            "     X  "
            "     X  "
            " X   X  "
            "  XXX   ")
    },
    {
        /* [Ǵ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1f4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    XX  "
            "  XXXX  "
            " X    X "
            " X      "
            " X  XXX "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ǵ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1f5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "     X  "
            "    X   "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [Ǽ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1fc,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    XX  "
            " XXXXXX "
            "X  X    "
            "X  X    "
            "XXXXXX  "
            "X  X    "
            "X  XXXX "
            "        ")
    },
    {
        /* [ǽ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1fd,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            " XXXXXX "
            " XXXXXX "
            "X  X    "
            " XX XX  "
            "        ")
    },
    {
        /* [Ǿ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1fe,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    XX  "
            "  XXXX  "
            " X   XXX"
            " X  X X "
            " XXX  X "
            "XX    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ǿ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x1ff,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X  X "
            " XXXXXX "
            " X XX X "
            " XX   X "
            "X XXXX  "
            "        ")
    },
    {
        /* [Ș] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x218,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " XXXXX  "
            "      X "
            " X    X "
            "  XXXX  "
            "  X     ")
    },
    {
        /* [ș] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x219,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " XXXXX  "
            "      X "
            " XXXXX  "
            "  X     ")
    },
    {
        /* [Ț] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x21a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "XXXXXXX "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "   XX   "
            "   X    ")
    },
    {
        /* [ț] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x21b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "   X    "
            " XXXXX  "
            "   X    "
            "   X    "
            "    XXX "
            "     X  ")
    },
    {
        /* [Ȳ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x232,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XXXXX  "
            "X     X "
            " X   X  "
            " XX XX  "
            "   X    "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [ȳ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x233,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [ȷ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x237,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "    XX  "
            "     X  "
            "     X  "
            " X   X  "
            "  XXX   ")
    },
    {
        /* [ɔ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x254,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            "      X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ɘ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x258,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X XX X "
            "      X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ə] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x259,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "  XXXXX "
            " XXXX X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ɛ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x25b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " XXXX   "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ɲ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x272,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "X       ")
    },
    {
        /* [ʒ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x292,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            "     X  "
            "   XXX  "
            "      X "
            " XXXXX  ")
    },
    {
        /* [ʻ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x2bb,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    X   "
            "   X    "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [ʼ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x2bc,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    X   "
            "   X    "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [ʽ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x2bd,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "    X   "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [ˆ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x2c6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   XX   "
            "  X  X  "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [ˇ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x2c7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            "   XX   "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [˘] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x2d8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            "   XX   "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [˙] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x2d9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [˛] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x2db,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "     X  "
            "    XXX ")
    },
    {
        /* [˜] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x2dc,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX  X "
            " X  XX  "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [˝] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x2dd,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  XX XX "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [΄] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x384,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX      "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [΅] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x385,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    X   "
            "  XX X  "
            "  X  X  "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [Ά] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x386,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX      "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [·] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x387,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "   X    "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [Έ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x388,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX      "
            " XXXXXX "
            " X      "
            " XXXX   "
            " X      "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [Ή] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x389,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX      "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Ί] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x38a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX      "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ό] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x38c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX      "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ύ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x38e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX      "
            "        "
            "X     X "
            " X   X  "
            "  X X   "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [Ώ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x38f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "XX      "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            "  X  X  "
            " XX  XX "
            "        ")
    },
    {
        /* [ΐ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x390,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            " XX X   "
            "        "
            "  XX    "
            "   X    "
            "   X    "
            "    XX  "
            "        ")
    },
    {
        /* [Α] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x391,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Β] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x392,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXX  "
            " X    X "
            " XXXXX  "
            " X    X "
            " X    X "
            " XXXXX  "
            "        ")
    },
    {
        /* [Γ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x393,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            " X      "
            " X      "
            " X      "
            " X      "
            " X      "
            "        ")
    },
    {
        /* [Δ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x394,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "  X X   "
            " XX XX  "
            " X   X  "
            "X     X "
            "XXXXXXX "
            "        ")
    },
    {
        /* [Ε] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x395,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            " X      "
            " XXXX   "
            " X      "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [Ζ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x396,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "      X "
            "    XX  "
            "  XX    "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [Η] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x397,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Θ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x398,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X XX X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Ι] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x399,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Κ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x39a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            " X  XX  "
            " XXX    "
            " XXX    "
            " X  XX  "
            " X    X "
            "        ")
    },
    {
        /* [Λ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x39b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "  X X   "
            " XX XX  "
            " X   X  "
            "X     X "
            "X     X "
            "        ")
    },
    {
        /* [Μ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x39c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "XX   XX "
            "X X X X "
            "X  X  X "
            "X     X "
            "X     X "
            "X     X "
            "        ")
    },
    {
        /* [Ν] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x39d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            " X    X "
            " XXX  X "
            " X  XXX "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Ξ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x39e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "        "
            "  XXXX  "
            "        "
            "        "
            " XXXXXX "
            "        ")
    },
    {
        /* [Ο] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x39f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [Π] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3a0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [Ρ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3a1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXX  "
            " X    X "
            " X    X "
            " XXXXX  "
            " X      "
            " X      "
            "        ")
    },
    {
        /* [Σ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3a3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "  X     "
            "   XX   "
            "   XX   "
            "  X     "
            " XXXXXX "
            "        ")
    },
    {
        /* [Τ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3a4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "XXXXXXX "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [Υ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3a5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "X     X "
            " X   X  "
            " XX XX  "
            "   X    "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [Φ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3a6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXX  "
            "X  X  X "
            "X  X  X "
            "X  X  X "
            "X  X  X "
            " XXXXX  "
            "        ")
    },
    {
        /* [Χ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3a7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            "  X  X  "
            "   XX   "
            "   XX   "
            "  X  X  "
            " X    X "
            "        ")
    },
    {
        /* [Ψ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3a8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "X  X  X "
            "X  X  X "
            "X  X  X "
            "X  X  X "
            " XXXXX  "
            "   X    "
            "        ")
    },
    {
        /* [Ω] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3a9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            "  X  X  "
            " XX  XX "
            "        ")
    },
    {
        /* [Ϊ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3aa,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " X   X  "
            "  XXX   "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [Ϋ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3ab,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " X   X  "
            "        "
            "X     X "
            " X   X  "
            "  X X   "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [ά] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3ac,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            " XXXXXX "
            " X   X  "
            " X   X  "
            "  XXX X "
            "        ")
    },
    {
        /* [έ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3ad,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            " XXXXXX "
            " XXXX   "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ή] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3ae,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "      X ")
    },
    {
        /* [ί] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3af,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            "  XX    "
            "   X    "
            "   X    "
            "    XX  "
            "        ")
    },
    {
        /* [ΰ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3b0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "    X   "
            "  XX X  "
            "  X  X  "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [α] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3b1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X   X  "
            " X   X  "
            "  XXX X "
            "        ")
    },
    {
        /* [β] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3b2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXX   "
            " X   X  "
            " XXXXX  "
            " X    X "
            " X    X "
            " XXXXX  "
            " X      ")
    },
    {
        /* [γ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3b3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "X     X "
            " X   X  "
            "  X X   "
            "   X    "
            "   X    ")
    },
    {
        /* [δ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3b4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXXX "
            "    X   "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ε] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3b5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " XXXX   "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ζ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3b6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "    X   "
            "  XX    "
            " X      "
            " X      "
            "  XXXX  "
            "     XX ")
    },
    {
        /* [η] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3b7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "      X ")
    },
    {
        /* [θ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3b8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXX   "
            " X   X  "
            " XXXXX  "
            " X   X  "
            " X   X  "
            "  XXX   "
            "        ")
    },
    {
        /* [ι] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3b9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "  XX    "
            "   X    "
            "   X    "
            "    XX  "
            "        ")
    },
    {
        /* [κ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3ba,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " X   XX "
            " XXXX   "
            " X  X   "
            " X    X "
            "        ")
    },
    {
        /* [λ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3bb,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X     "
            "   X    "
            "  X X   "
            " XX XX  "
            " X   X  "
            "X     X "
            "        ")
    },
    {
        /* [μ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3bc,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " X    X "
            " X    X "
            " X   XX "
            " XXXX X "
            " X      ")
    },
    {
        /* [ν] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3bd,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " X    X "
            " XX  XX "
            "  X  X  "
            "   XX   "
            "        ")
    },
    {
        /* [ξ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3be,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXXX "
            " X      "
            "  XXXX  "
            " X      "
            " X      "
            "  XXXX  "
            "     XX ")
    },
    {
        /* [ο] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3bf,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [π] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3c0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [ρ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3c1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            " XXXXX  "
            " X      ")
    },
    {
        /* [ς] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3c2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X      "
            " X      "
            "  XXXX  "
            "     XX ")
    },
    {
        /* [σ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3c3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X   X  "
            " X   X  "
            "  XXX   "
            "        ")
    },
    {
        /* [τ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3c4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "XXXXXXX "
            "   X    "
            "   X    "
            "    XX  "
            "        ")
    },
    {
        /* [υ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3c5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [φ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3c6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "XX XXXX "
            "X  X  X "
            "X  X  X "
            " XXXXX  "
            "   X    ")
    },
    {
        /* [χ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3c7,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " X    X "
            "  X  X  "
            "   XX   "
            "  X  X  "
            " X    X ")
    },
    {
        /* [ψ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3c8,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "X  X  X "
            "X  X  X "
            "X  X  X "
            " XXXXX  "
            "   X    ")
    },
    {
        /* [ω] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3c9,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "XX   XX "
            "X  X  X "
            "X  X  X "
            " XX XX  "
            "        ")
    },
    {
        /* [ϊ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3ca,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X  X   "
            "        "
            "  XX    "
            "   X    "
            "   X    "
            "    XX  "
            "        ")
    },
    {
        /* [ϋ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3cb,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "        "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ό] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3cc,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ύ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3cd,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ώ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3ce,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    X   "
            "   X    "
            "XX   XX "
            "X  X  X "
            "X  X  X "
            " XX XX  "
            "        ")
    },
    {
        /* [ϑ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3d1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXX   "
            " X   X  "
            "  XXXXX "
            "X    X  "
            " X   X  "
            "  XXX   "
            "        ")
    },
    {
        /* [ϕ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3d5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "   X    "
            "XXXXXXX "
            "X  X  X "
            "X  X  X "
            " XXXXX  "
            "   X    ")
    },
    {
        /* [ϰ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3f0,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "XXX  XX "
            "   XX   "
            "  XX    "
            "X    XX "
            "        ")
    },
    {
        /* [ϱ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3f1,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            " XXXXX  "
            "  XXXX  ")
    },
    {
        /* [ϲ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3f2,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X      "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ϳ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3f3,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "     X  "
            "        "
            "    XX  "
            "     X  "
            "     X  "
            " X   X  "
            "  XXX   ")
    },
    {
        /* [ϴ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3f4,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [ϵ] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3f5,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "  XXXXX "
            " XXXXX  "
            " X      "
            "  XXXXX "
            "        ")
    },
    {
        /* [϶] */
        CHAFA_SYMBOL_TAG_LATIN,
        0x3f6,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXX  "
            "  XXXXX "
            "      X "
            " XXXXX  "
            "        ")
    },
