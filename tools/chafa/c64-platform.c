/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2018-2023 Hans Petter Jansson
 *
 * This file is part of Chafa, a program that shows pictures on text terminals.
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

#define UNICODE_UNDEF 0

typedef struct
{
    gint c;
    gunichar u;
}
CodeMap;

/* It's better to keep a single entry per line, since it leaves room for annotation.
 * Also, maintaining tabular layouts is a pita even when we don't have out-there
 * Unicode characters messing up the formatting (which, in this case, we do). */
static const CodeMap c64_unshifted_pseudo_codes [] =
{
    /* 0-127 */
    { 0, '@' },
    { 1, 'A' },
    { 2, 'B' },
    { 3, 'C' },
    { 4, 'D' },
    { 5, 'E' },
    { 6, 'F' },
    { 7, 'G' },
    { 8, 'H' },
    { 9, 'I' },
    { 10, 'J' },
    { 11, 'K' },
    { 12, 'L' },
    { 13, 'M' },
    { 14, 'N' },
    { 15, 'O' },
    { 16, 'P' },
    { 17, 'Q' },
    { 18, 'R' },
    { 19, 'S' },
    { 20, 'T' },
    { 21, 'U' },
    { 22, 'V' },
    { 23, 'W' },
    { 24, 'X' },
    { 25, 'Y' },
    { 26, 'Z' },
    { 27, '[' },
    { 28, 0xa3 }, /* £ pound symbol*/
    { 29, ']' },
    { 30, 0x2191 }, /* ↑ up arrow */
    { 31, 0x2190 }, /* ← left arrow */
    { 32, ' ' },
    { 33, '!' },
    { 34, '"' },
    { 35, '#' },
    { 36, '$' },
    { 37, '%' },
    { 38, '&' },
    { 39, '\'' },
    { 40, '(' },
    { 41, ')' },
    { 42, '*' },
    { 43, '+' },
    { 44, ',' },
    { 45, '-' },
    { 46, '.' },
    { 47, '/' },
    { 48, '0' },
    { 49, '1' },
    { 50, '2' },
    { 51, '3' },
    { 52, '4' },
    { 53, '5' },
    { 54, '6' },
    { 55, '7' },
    { 56, '8' },
    { 57, '9' },
    { 58, ':' },
    { 59, ';' },
    { 60, '<' },
    { 61, '=' },
    { 62, '>' },
    { 63, '?' },
    { 64, 0x2500 }, /* ─ horizontal box drawing line */
    { 65, 0x2660 }, /* ♠ black spade suit */
    { 66, 0x2502 }, /* │ vertical 1/8 block 4 */
    { 67, 0x1fb79 }, /* 🭹 horizontal 1/8 block 5 */
    { 68, 0x1fb78 }, /* 🭸 horizontal 1/8 block 4 */
    { 69, 0x1fb77 }, /* 🭷 horizontal 1/8 block 3 */
    { 70, 0x1fb7a }, /* 🭶 horizontal 1/8 block 6 */
    { 71, 0x1fb71 }, /* 🭱 vertical 1/8 block 3 */
    { 72, 0x1fb73 }, /* 🭳 vertical 1/8 block 5 */
    { 73, 0x256e }, /* ╮ left-bottom arc connector */
    { 74, 0x2570 }, /* ╰ right-top arc connector */
    { 75, 0x256f }, /* ╯ left-top arc connector */
    { 76, 0x1fb7c }, /* 🭼 left and lower one-eight corner */
    { 77, 0x2572 }, /* ╲ box drawing light diagonal UL to LR */
    { 78, 0x2571 }, /* ╱ box drawing light diagonal UR to LL */
    { 79, 0x1fb7d }, /* 🭽 left and upper one-eight corner */
    { 80, 0x1fb7e }, /* 🭾 right and upper one-eight corner */
    { 81, 0x25cf }, /* ● black circle */
    { 82, 0x1fb7b }, /* 🭻 horizontal 1/8 block 7 */
    { 83, 0x2665 }, /* ♥ black heart suit */
    { 84, 0x1fb70 }, /* 🭰 vertical 1/8 block 2 */
    { 85, 0x256d }, /* ╭ right-bottom arc connector */
    { 86, 0x2573 }, /* ╳ box drawing light cross */
    { 87, 0x25cb }, /* ○ white circle */
    { 88, 0x2663 }, /* ♣ black club suit */
    { 89, 0x1fb75 }, /* 🭵 vertical 1/8 block 7 */
    { 90, 0x2666 }, /* ♦ black diamond suit */
    { 91, 0x253c }, /* ┼ box drawing light vertical and horizontal */
    { 92, 0x1fb8c }, /* 🮌 left half medium shade (kinda) */
    { 93, 0x2502 }, /* │ vertical box drawing line */
    { 94, 0x03c0 }, /* π greek small letter Pi */
    { 95, 0x25e5 }, /* ◥ black upper right triangle */
    { 96, ' ' }, /* looks like a space (identical to #32) */
    { 97, 0x258c }, /* ▌ left half block */
    { 98, 0x2584 }, /* ▄ lower half block */
    { 99, 0x23ba }, /* ⎺ horizontal scan line 1 (upper) */
    { 100, 0x23bd }, /* ⎽ horizontal scan line 9 (lower) */
    { 101, 0x258f }, /* ▏ left 1/8 block */
    { 102, 0x1fb95 }, /* 🮕 checker board fill */
    { 103, 0x2595 }, /* ▕ right 1/8 block */
    { 104, 0x1fb8f }, /* 🮏 lower half medium shade (kinda) */
    { 105, 0x25e4 }, /* ◤ black upper left triangle */
    { 106, 0x2595 }, /* ▕ right 1/8 block (identical to #103) */
    { 107, 0x251c }, /* ├ box drawing light vertical and right */
    { 108, 0x2597 }, /* ▗ quadrant lower right */
    { 109, 0x2514 }, /* └ box drawing light up and right */
    { 110, 0x2510 }, /* ┐ box drawing light down and left */
    { 111, 0x2581 }, /* ▁ lower 1/8 block */
    { 112, 0x250c }, /* ┌ box drawing light down and right */
    { 113, 0x2534 }, /* ┴ box drawing light up and horizontal */
    { 114, 0x252c }, /* ┬ box drawing light down and horizontal */
    { 115, 0x2524 }, /* ┤ box drawing light vertical and left */
    { 116, 0x258f }, /* ▏ left 1/8 block (identical to #101) */
    { 117, 0x258e }, /* ▎ left 1/4 block */
    { 118, 0x1fb87 }, /* 🮇 right 1/4 block */
    { 119, 0x2594 }, /* ▔ upper 1/8 block */
    { 120, 0x1fb82 }, /* 🮂 upper 1/4 block */
    { 121, 0x2582 }, /* ▂ lower 1/4 block */
    { 122, 0x1fb7f }, /* 🭿 right and lower 1/8 corner */
    { 123, 0x2596 }, /* ▖ quadrant lower left */
    { 124, 0x259d }, /* ▝ quadrant upper right */
    { 125, 0x2518 }, /* ┘ box drawing light up and left */
    { 126, 0x2598 }, /* ▘ quadrant upper left */
    { 127, 0x259a }, /* ▚ quadrant upper left and lower right */

    /* Chars 128-255 are inverted versions of 0-127. Some of them make sense
     * as plain characters too. We list those here. */

    { 160, 0x2588 }, /* █ full block */
    { 214, 0x1fbbd }, /* 🮽 negative diagonal cross */
    { 220, 0x1fb94 }, /* 🮔 left medium half inverse medium shade and right half block (kinda) */
    { 223, 0x25e3 }, /* ◣ black lower left triangle */
    { 225, 0x2590 }, /* ▐ right half block */
    { 226, 0x2580 }, /* ▀ upper half block */
    { 229, 0x1fb8b }, /* 🮋 right 7/8 block */
    { 230, 0x1fb96 }, /* 🮖 inverse checker board fill */
    { 231, 0x2589 }, /* ▉ left 7/8 block */
    { 232, 0x1fb91 }, /* 🮑 upper half block and lower half inverse medium shade (kinda) */
    { 233, 0x25e2 }, /* ◢ black lower right triangle */
    { 234, 0x2589 }, /* ▉ left 7/8 block, (identical to #231) */
    { 236, 0x259b }, /* ▛ quadrant UL and UR and LL */
    { 239, 0x1fb86 }, /* 🮆 upper 7/8 block */
    { 244, 0x1fb8b }, /* 🮋 right 7/8 block (identical to #229) */
    { 245, 0x1fb8a }, /* 🮊 right 3/4 block */
    { 246, 0x258a }, /* ▊ left 3/4 block */
    { 247, 0x2587 }, /* ▇ lower 7/8 block */
    { 248, 0x2586 }, /* ▆ lower 3/4 block */
    { 249, 0x1fb85 }, /* 🮅 upper 3/4 block */
    { 251, 0x259c }, /* ▜ quadrant UL and UR and LR */
    { 252, 0x2599 }, /* ▙ quadrant UL and LL and LR */
    { 254, 0x259f }, /* ▟ quadrant UR and LL and LR */
    { 255, 0x259e }, /* ▞ quadrant upper right and lower left */
    { -1, 0 }
};
