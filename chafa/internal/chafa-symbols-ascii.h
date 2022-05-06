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
 * These are 7-bit ASCII symbols. The bitmaps are a close match to the
 * Terminus font (specifically ter-x14n.pcf).
 *
 * ASCII symbols are also "Latin" symbols, loosely corresponding to the
 * symbols you'd find in the European ISO/IEC 8859 charsets.
 */

    {
        /* [ ] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN | CHAFA_SYMBOL_TAG_SPACE,
        0x20,
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
        /* [!] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x21,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "        "
            "   X    "
            "        ")
    },
    {
        /* ["] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x22,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "  X  X  "
            "  X  X  "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [#] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x23,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  X  X  "
            "  X  X  "
            " XXX XX "
            " XX XXX "
            "  X  X  "
            "  X  X  "
            "        ")
    },
    {
        /* [$] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x24,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "   X    "
            " XXXXX  "
            "X  X    "
            " XXXXXX "
            "   X  X "
            " XXXXX  "
            "   X    ")
    },
    {
        /* [%] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x25,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XX  X  "
            " XX X   "
            "   XX   "
            "  XX    "
            "  X XX  "
            " X  XX  "
            "        ")
    },
    {
        /* [&] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x26,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "  X  X  "
            "  XXX   "
            " X  X X "
            " X   X  "
            "  XXX X "
            "        ")
    },
    {
        /* ['] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x27,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "   X    "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [(] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x28,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "  X     "
            "  X     "
            "  X     "
            "  X     "
            "   XX   "
            "        ")
    },
    {
        /* [)] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x29,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX    "
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "  XX    "
            "        ")
    },
    {
        /* [*] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x2a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "  XXXX  "
            " XXXXXX "
            "  X  X  "
            "        "
            "        ")
    },
    {
        /* [+] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x2b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "   X    "
            " XXXXX  "
            "   X    "
            "        "
            "        ")
    },
    {
        /* [,] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x2c,
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
        /* [-] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x2d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            " XXXXXX "
            "        "
            "        "
            "        ")
    },
    {
        /* [.] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x2e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "   X    "
            "        ")
    },
    {
        /* [/] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x2f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "     X  "
            "    X   "
            "   XX   "
            "  XX    "
            "  X     "
            " X      "
            "        ")
    },
    {
        /* [0] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x30,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X  XXX "
            " XXX  X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [1] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x31,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   XX   "
            "  X X   "
            "    X   "
            "    X   "
            "    X   "
            "  XXXXX "
            "        ")
    },
    {
        /* [2] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x32,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            "     XX "
            "   XX   "
            "  X     "
            " XXXXXX "
            "        ")
    },
    {
        /* [3] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x33,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            "   XXX  "
            "      X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [4] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x34,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "     XX "
            "   XX X "
            "  X   X "
            " XXXXXX "
            "      X "
            "      X "
            "        ")
    },
    {
        /* [5] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x35,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            " X      "
            " XXXXX  "
            "      X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [6] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x36,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X      "
            " XXXXX  "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [7] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x37,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            "      X "
            "     X  "
            "    X   "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [8] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x38,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            "  XXXX  "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [9] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x39,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X    X "
            "  XXXXX "
            "      X "
            "  XXXX  "
            "        ")
    },
    {
        /* [:] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x3a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "   X    "
            "        "
            "        "
            "   X    "
            "        ")
    },
    {
        /* [;] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x3b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "   X    "
            "        "
            "        "
            "   X    "
            "  X     ")
    },
    {
        /* [<] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x3c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "     X  "
            "    X   "
            "  XX    "
            " XX     "
            "   XX   "
            "     X  "
            "        ")
    },
    {
        /* [=] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x3d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " XXXXXX "
            "        "
            "        "
            "        ")
    },
    {
        /* [>] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x3e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            "  X     "
            "   XX   "
            "    XX  "
            "  XX    "
            " X      "
            "        ")
    },
    {
        /* [?] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x3f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X   XX "
            "    X   "
            "        "
            "    X   "
            "        ")
    },
    {
        /* [@] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x40,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXX  "
            "X     X "
            "X XXX X "
            "X X  XX "
            "X  XX   "
            " XXXXXX "
            "        ")
    },
    {
        /* [A] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x41,
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
        /* [B] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x42,
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
        /* [C] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x43,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X      "
            " X      "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [D] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x44,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            " XXXXX  "
            "        ")
    },
    {
        /* [E] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x45,
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
        /* [F] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x46,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXXX "
            " X      "
            " XXXX   "
            " X      "
            " X      "
            " X      "
            "        ")
    },
    {
        /* [G] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x47,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X      "
            " X  XXX "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [H] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x48,
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
        /* [I] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x49,
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
        /* [J] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x4a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    XXX "
            "     X  "
            "     X  "
            "     X  "
            " X   X  "
            "  XXX   "
            "        ")
    },
    {
        /* [K] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x4b,
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
        /* [L] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x4c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            " X      "
            " X      "
            " X      "
            " X      "
            " XXXXXX "
            "        ")
    },
    {
        /* [M] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x4d,
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
        /* [N] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x4e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            " XX   X "
            " X X  X "
            " X  X X "
            " X   XX "
            " X    X "
            "        ")
    },
    {
        /* [O] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x4f,
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
        /* [P] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x50,
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
        /* [Q] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x51,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "      X ")
    },
    {
        /* [R] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x52,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " XXXXX  "
            " X    X "
            " X    X "
            " XXXXX  "
            " X  XX  "
            " X    X "
            "        ")
    },
    {
        /* [S] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x53,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXXX  "
            " X    X "
            " XXXXX  "
            "      X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [T] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x54,
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
        /* [U] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x55,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            " X    X "
            "  XXXX  "
            "        ")
    },
    {
        /* [V] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x56,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X    X "
            " X    X "
            " X    X "
            "  X  X  "
            "  X  X  "
            "   XX   "
            "        ")
    },
    {
        /* [W] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x57,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "X     X "
            "X     X "
            "X     X "
            "X  X  X "
            "X X X X "
            "XX   XX "
            "        ")
    },
    {
        /* [X] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x58,
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
        /* [Y] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x59,
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
        /* [Z] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x5a,
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
        /* [[] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x5b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXX   "
            "  X     "
            "  X     "
            "  X     "
            "  X     "
            "  XXX   "
            "        ")
    },
    {
        /* [\] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x5c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            "  X     "
            "  XX    "
            "   XX   "
            "    X   "
            "     X  "
            "        ")
    },
    {
        /* []] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x5d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XXX   "
            "    X   "
            "    X   "
            "    X   "
            "    X   "
            "  XXX   "
            "        ")
    },
    {
        /* [^] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x5e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "   X    "
            "  X X   "
            " X   X  "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
    {
        /* [_] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x5f,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        "
            " XXXXXX ")
    },
    {
        /* [`] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x60,
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
        /* [a] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x61,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "  XXXXX "
            " XXXXXX "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [b] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x62,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            " X      "
            " XXXXXX "
            " X    X "
            " X    X "
            " XXXXX  "
            "        ")
    },
    {
        /* [c] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x63,
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
        /* [d] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x64,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "      X "
            "      X "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [e] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x65,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X XX X "
            " X      "
            "  XXXX  "
            "        ")
    },
    {
        /* [f] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x66,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    XXX "
            "   X    "
            " XXXXX  "
            "   X    "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [g] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x67,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [h] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x68,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            " X      "
            " XXXXXX "
            " X    X "
            " X    X "
            " X    X "
            "        ")
    },
    {
        /* [i] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x69,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "        "
            "  XX    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [j] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x6a,
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
        /* [k] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x6b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            " X      "
            " X      "
            " X   XX "
            " XXXX   "
            " X  X   "
            " X   XX "
            "        ")
    },
    {
        /* [l] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x6c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "  XXX   "
            "        ")
    },
    {
        /* [m] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x6d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "XXXXXXX "
            "X  X  X "
            "X  X  X "
            "X  X  X "
            "        ")
    },
    {
        /* [n] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x6e,
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
        /* [o] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x6f,
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
        /* [p] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x70,
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
        /* [q] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x71,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X    X "
            " X    X "
            "  XXXXX "
            "      X ")
    },
    {
        /* [r] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x72,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " X      "
            " X      "
            " X      "
            "        ")
    },
    {
        /* [s] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x73,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            " XXXXX  "
            "      X "
            " XXXXX  "
            "        ")
    },
    {
        /* [t] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x74,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "   X    "
            " XXXXX  "
            "   X    "
            "   X    "
            "    XXX "
            "        ")
    },
    {
        /* [u] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x75,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "        ")
    },
    {
        /* [v] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x76,
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
        /* [w] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x77,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            "X     X "
            "X  X  X "
            "X  X  X "
            " XXXXX  "
            "        ")
    },
    {
        /* [x] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x78,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " X    X "
            "  XXXX  "
            "  X  X  "
            " X    X "
            "        ")
    },
    {
        /* [y] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x79,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " X    X "
            " X    X "
            " X    X "
            "  XXXXX "
            "  XXXX  ")
    },
    {
        /* [z] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x7a,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "        "
            "        "
            " XXXXXX "
            "    XX  "
            "  XX    "
            " XXXXXX "
            "        ")
    },
    {
        /* [{] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x7b,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "    XX  "
            "   X    "
            "  X     "
            "   X    "
            "   X    "
            "    XX  "
            "        ")
    },
    {
        /* [|] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x7c,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "   X    "
            "        ")
    },
    {
        /* [}] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x7d,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            "        "
            "  XX    "
            "    X   "
            "     X  "
            "    X   "
            "    X   "
            "  XX    "
            "        ")
    },
    {
        /* [~] */
        CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_LATIN,
        0x7e,
        CHAFA_SYMBOL_OUTLINE_8X8 (
            " XX   X "
            "X  XXX  "
            "        "
            "        "
            "        "
            "        "
            "        "
            "        ")
    },
