/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020 Hans Petter Jansson
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
#include "internal/chafa-string-util.h"

/* Generate a const table of the ASCII decimal numbers 0..255, avoiding leading
 * zeroes. Each entry is exactly 4 bytes. The strings are not zero-terminated;
 * instead their lengths are stored in the 4th byte, potentially leaving a gap
 * between the string and the length.
 *
 * This allows us to fetch a string using fixed-length memcpy() followed by
 * incrementing the target pointer. We copy all four bytes (32 bits) in the
 * hope that the compiler will generate register-wide loads and stores where
 * alignment is not an issue.
 *
 * The idea is to speed up printing for decimal numbers in this range (common
 * with palette indexes and color channels) at the cost of exactly 1kiB in the
 * executable.
 *
 * We require C99 due to __VA_ARGS__ and designated init but use no extensions. */

#define GEN_1(len, ...) { __VA_ARGS__, [3] = len }

#define GEN_10(len, ...) \
    GEN_1 (len, __VA_ARGS__, '0'), GEN_1 (len, __VA_ARGS__, '1'), \
    GEN_1 (len, __VA_ARGS__, '2'), GEN_1 (len, __VA_ARGS__, '3'), \
    GEN_1 (len, __VA_ARGS__, '4'), GEN_1 (len, __VA_ARGS__, '5'), \
    GEN_1 (len, __VA_ARGS__, '6'), GEN_1 (len, __VA_ARGS__, '7'), \
    GEN_1 (len, __VA_ARGS__, '8'), GEN_1 (len, __VA_ARGS__, '9')

#define GEN_100(len, ...) \
    GEN_10 (len, __VA_ARGS__, '0'), GEN_10 (len, __VA_ARGS__, '1'), \
    GEN_10 (len, __VA_ARGS__, '2'), GEN_10 (len, __VA_ARGS__, '3'), \
    GEN_10 (len, __VA_ARGS__, '4'), GEN_10 (len, __VA_ARGS__, '5'), \
    GEN_10 (len, __VA_ARGS__, '6'), GEN_10 (len, __VA_ARGS__, '7'), \
    GEN_10 (len, __VA_ARGS__, '8'), GEN_10 (len, __VA_ARGS__, '9')

const char chafa_ascii_dec_u8 [256] [4] =
{
    /* 0-9 */
    GEN_1 (1, '0'), GEN_1 (1, '1'), GEN_1 (1, '2'), GEN_1 (1, '3'),
    GEN_1 (1, '4'), GEN_1 (1, '5'), GEN_1 (1, '6'), GEN_1 (1, '7'),
    GEN_1 (1, '8'), GEN_1 (1, '9'),

    /* 10-99 */
    GEN_10 (2, '1'), GEN_10 (2, '2'), GEN_10 (2, '3'), GEN_10 (2, '4'),
    GEN_10 (2, '5'), GEN_10 (2, '6'), GEN_10 (2, '7'), GEN_10 (2, '8'),
    GEN_10 (2, '9'),

    /* 100-199 */
    GEN_100 (3, '1'),

    /* 200-249 */
    GEN_10 (3, '2', '0'), GEN_10 (3, '2', '1'), GEN_10 (3, '2', '2'),
    GEN_10 (3, '2', '3'), GEN_10 (3, '2', '4'),

    /* 250-255 */
    GEN_1 (3, '2', '5', '0'), GEN_1 (3, '2', '5', '1'), GEN_1 (3, '2', '5', '2'),
    GEN_1 (3, '2', '5', '3'), GEN_1 (3, '2', '5', '4'), GEN_1 (3, '2', '5', '5')
};
