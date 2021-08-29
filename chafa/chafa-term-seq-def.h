/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright (C) 2020-2021 Hans Petter Jansson
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

/* Terminal sequence definitions
 * -----------------------------
 *
 * This file is #included in various contexts with CHAFA_TERM_SEQ_DEF()
 * expanding to different things. It allows us to keep all the terminal
 * sequence metadata in one place.
 *
 * We process this file with 'cpp -CC' to let the docstrings through to
 * gtk-doc.
 *
 * The generator macro is invoked with the following arguments:
 *
 * CHAFA_TERM_SEQ_DEF (name, NAME, n_args, args_proc, args_type, ...)
 *
 * Sequences are grouped by the library version they became available in,
 * with CHAFA_TERM_SEQ_AVAILABILITY expanding to the appropriate version
 * macro in each case.
 *
 * The actual sequence strings are not defined here; they belong to the
 * individual terminal model definitions.
 *
 * References
 * ----------
 *
 * VT220 sequences: https://vt100.net/docs/vt220-rm/chapter4.html
 * Sixels: https://vt100.net/docs/vt3xx-gp/chapter14.html
 */

/* __VA_OPT__ from C++2a would be nice, but it's too recent to rely on in
 * public headers just yet. So we have this exciting trick instead. */
#define CHAFA_TERM_SEQ_ARGS ,

/* --- Available in 1.6+ --- */

#define CHAFA_TERM_SEQ_AVAILABILITY CHAFA_AVAILABLE_IN_1_6

/**
 * chafa_term_info_emit_reset_terminal_soft:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_RESET_TERMINAL_SOFT.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(reset_terminal_soft, RESET_TERMINAL_SOFT, 0, none, void)

/**
 * chafa_term_info_emit_reset_terminal_hard:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_RESET_TERMINAL_HARD.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(reset_terminal_hard, RESET_TERMINAL_HARD, 0, none, void)

/**
 * chafa_term_info_emit_reset_attributes:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_RESET_ATTRIBUTES.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(reset_attributes, RESET_ATTRIBUTES, 0, none, void)

/**
 * chafa_term_info_emit_clear:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CLEAR.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(clear, CLEAR, 0, none, void)

/**
 * chafa_term_info_emit_invert_colors:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_INVERT_COLORS.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(invert_colors, INVERT_COLORS, 0, none, void)

/* Cursor movement. Cursor stops at margins. */

/**
 * chafa_term_info_emit_cursor_to_top_left:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_TO_TOP_LEFT.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_to_top_left, CURSOR_TO_TOP_LEFT, 0, none, void)

/**
 * chafa_term_info_emit_cursor_to_bottom_left:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_TO_BOTTOM_LEFT.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_to_bottom_left, CURSOR_TO_BOTTOM_LEFT, 0, none, void)

/**
 * chafa_term_info_emit_cursor_to_pos:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @x: Offset from left edge of display, zero-indexed
 * @y: Offset from top edge of display, zero-indexed
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_TO_POS.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_to_pos, CURSOR_TO_POS, 2, pos, guint, CHAFA_TERM_SEQ_ARGS guint x, guint y)

/**
 * chafa_term_info_emit_cursor_up_1:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_UP_1.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_up_1, CURSOR_UP_1, 0, none, void)

/**
 * chafa_term_info_emit_cursor_up:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @n: Distance to move the cursor
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_UP.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_up, CURSOR_UP, 1, none, guint, CHAFA_TERM_SEQ_ARGS guint n)

/**
 * chafa_term_info_emit_cursor_down_1:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_DOWN_1.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_down_1, CURSOR_DOWN_1, 0, none, void)

/**
 * chafa_term_info_emit_cursor_down:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @n: Distance to move the cursor
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_DOWN.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_down, CURSOR_DOWN, 1, none, guint, CHAFA_TERM_SEQ_ARGS guint n)

/**
 * chafa_term_info_emit_cursor_left_1:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_LEFT_1.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_left_1, CURSOR_LEFT_1, 0, none, void)

/**
 * chafa_term_info_emit_cursor_left:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @n: Distance to move the cursor
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_LEFT.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_left, CURSOR_LEFT, 1, none, guint, CHAFA_TERM_SEQ_ARGS guint n)

/**
 * chafa_term_info_emit_cursor_right_1:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_RIGHT_1.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_right_1, CURSOR_RIGHT_1, 0, none, void)

/**
 * chafa_term_info_emit_cursor_right:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @n: Distance to move the cursor
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_RIGHT.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_right, CURSOR_RIGHT, 1, none, guint, CHAFA_TERM_SEQ_ARGS guint n)

/* Cursor movement. Cursor crossing margin causes scrolling region to
 * scroll. */

/**
 * chafa_term_info_emit_cursor_up_scroll:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_UP_SCROLL.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_up_scroll, CURSOR_UP_SCROLL, 0, none, void)

/**
 * chafa_term_info_emit_cursor_down_scroll:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_CURSOR_DOWN_SCROLL.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(cursor_down_scroll, CURSOR_DOWN_SCROLL, 0, none, void)

/* Cells will shift on insert. Cells shifted off the edge will be lost. */

/**
 * chafa_term_info_emit_insert_cells:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @n: Number of cells to insert
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_INSERT_CELLS.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(insert_cells, INSERT_CELLS, 1, none, guint, CHAFA_TERM_SEQ_ARGS guint n)

/**
 * chafa_term_info_emit_delete_cells:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @n: Number of cells to delete
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_DELETE_CELLS.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(delete_cells, DELETE_CELLS, 1, none, guint, CHAFA_TERM_SEQ_ARGS guint n)

/* Cursor must be inside scrolling region. Rows are shifted inside the
 * scrolling region. Rows shifted off the edge will be lost. The cursor
 * position is reset to the first column. */

/**
 * chafa_term_info_emit_insert_rows:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @n: Number of rows to insert
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_INSERT_ROWS.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(insert_rows, INSERT_ROWS, 1, none, guint, CHAFA_TERM_SEQ_ARGS guint n)

/**
 * chafa_term_info_emit_delete_rows:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @n: Number of rows to delete
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_DELETE_ROWS.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(delete_rows, DELETE_ROWS, 1, none, guint, CHAFA_TERM_SEQ_ARGS guint n)

/* Defines the scrolling region. */

/**
 * chafa_term_info_emit_set_scrolling_rows:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @top: First row in scrolling area, zero-indexed
 * @bottom: Last row in scrolling area, zero-indexed
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_SET_SCROLLING_ROWS.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(set_scrolling_rows, SET_SCROLLING_ROWS, 2, pos, guint, CHAFA_TERM_SEQ_ARGS guint top, guint bottom)

/* Indicates whether characters printed in the middle of a row should
 * cause subsequent cells to shift forwards. Cells shifted off the edge
 * will be lost. If disabled, cells at the cursor position will be
 * overwritten instead. */

/**
 * chafa_term_info_emit_enable_insert:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_ENABLE_INSERT.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(enable_insert, ENABLE_INSERT, 0, none, void)

/**
 * chafa_term_info_emit_disable_insert:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_DISABLE_INSERT.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(disable_insert, DISABLE_INSERT, 0, none, void)

/**
 * chafa_term_info_emit_enable_cursor:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_ENABLE_CURSOR.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(enable_cursor, ENABLE_CURSOR, 0, none, void)

/**
 * chafa_term_info_emit_disable_cursor:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_DISABLE_CURSOR.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(disable_cursor, DISABLE_CURSOR, 0, none, void)

/**
 * chafa_term_info_emit_enable_echo:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_ENABLE_ECHO.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(enable_echo, ENABLE_ECHO, 0, none, void)

/**
 * chafa_term_info_emit_disable_echo:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_DISABLE_ECHO.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(disable_echo, DISABLE_ECHO, 0, none, void)

/* When printing a character in the last column, indicates whether the
 * cursor should move to the next row and potentially cause scrolling. If
 * disabled, the cursor may still move to the first column. */

/**
 * chafa_term_info_emit_enable_wrap:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_ENABLE_WRAP.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(enable_wrap, ENABLE_WRAP, 0, none, void)

/**
 * chafa_term_info_emit_disable_wrap:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_DISABLE_WRAP.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(disable_wrap, DISABLE_WRAP, 0, none, void)

/**
 * chafa_term_info_emit_set_color_fg_direct:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @r: Red component, 0-255
 * @g: Green component, 0-255
 * @b: Blue component, 0-255
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(set_color_fg_direct, SET_COLOR_FG_DIRECT, 3, none, guint8, CHAFA_TERM_SEQ_ARGS guint8 r, guint8 g, guint8 b)

/**
 * chafa_term_info_emit_set_color_bg_direct:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @r: Red component, 0-255
 * @g: Green component, 0-255
 * @b: Blue component, 0-255
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_SET_COLOR_BG_DIRECT.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(set_color_bg_direct, SET_COLOR_BG_DIRECT, 3, none, guint8, CHAFA_TERM_SEQ_ARGS guint8 r, guint8 g, guint8 b)

/**
 * chafa_term_info_emit_set_color_fgbg_direct:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @fg_r: Foreground red component, 0-255
 * @fg_g: Foreground green component, 0-255
 * @fg_b: Foreground blue component, 0-255
 * @bg_r: Background red component, 0-255
 * @bg_g: Background green component, 0-255
 * @bg_b: Background blue component, 0-255
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(set_color_fgbg_direct, SET_COLOR_FGBG_DIRECT, 6, none, guint8, CHAFA_TERM_SEQ_ARGS guint8 fg_r, guint8 fg_g, guint8 fg_b, guint8 bg_r, guint8 bg_g, guint8 bg_b)

/**
 * chafa_term_info_emit_set_color_fg_256:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @pen: Pen number, 0-255
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_SET_COLOR_FG_256.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(set_color_fg_256, SET_COLOR_FG_256, 1, none, guint8, CHAFA_TERM_SEQ_ARGS guint8 pen)

/**
 * chafa_term_info_emit_set_color_bg_256:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @pen: Pen number, 0-255
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_SET_COLOR_BG_256.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(set_color_bg_256, SET_COLOR_BG_256, 1, none, guint8, CHAFA_TERM_SEQ_ARGS guint8 pen)

/**
 * chafa_term_info_emit_set_color_fgbg_256:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @fg_pen: Foreground pen number, 0-255
 * @bg_pen: Background pen number, 0-255
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_SET_COLOR_FGBG_256.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(set_color_fgbg_256, SET_COLOR_FGBG_256, 2, none, guint8, CHAFA_TERM_SEQ_ARGS guint8 fg_pen, guint8 bg_pen)

/**
 * chafa_term_info_emit_set_color_fg_16:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @pen: Pen number, 0-15
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_SET_COLOR_FG_16.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(set_color_fg_16, SET_COLOR_FG_16, 1, aix16fg, guint8, CHAFA_TERM_SEQ_ARGS guint8 pen)

/**
 * chafa_term_info_emit_set_color_bg_16:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @pen: Pen number, 0-15
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_SET_COLOR_BG_16.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(set_color_bg_16, SET_COLOR_BG_16, 1, aix16bg, guint8, CHAFA_TERM_SEQ_ARGS guint8 pen)

/**
 * chafa_term_info_emit_set_color_fgbg_16:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @fg_pen: Foreground pen number, 0-15
 * @bg_pen: Background pen number, 0-15
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_SET_COLOR_FGBG_16.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(set_color_fgbg_16, SET_COLOR_FGBG_16, 2, aix16fgbg, guint8, CHAFA_TERM_SEQ_ARGS guint8 fg_pen, guint8 bg_pen)

/**
 * chafa_term_info_emit_begin_sixels:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @p1: Pixel aspect selector
 * @p2: Background color selector
 * @p3: Horizontal grid selector
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_BEGIN_SIXELS.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * All three parameters (@p1, @p2 and @p3) can normally be set to 0.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(begin_sixels, BEGIN_SIXELS, 3, none, guint, CHAFA_TERM_SEQ_ARGS guint p1, guint p2, guint p3)

/**
 * chafa_term_info_emit_end_sixels:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_END_SIXELS.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(end_sixels, END_SIXELS, 0, none, void)

/**
 * chafa_term_info_emit_repeat_char:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @n: Number of repetitions
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_REPEAT_CHAR.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.6
 **/
CHAFA_TERM_SEQ_DEF(repeat_char, REPEAT_CHAR, 1, none, guint, CHAFA_TERM_SEQ_ARGS guint n)

/* --- Available in 1.8+ --- */

#undef CHAFA_TERM_SEQ_AVAILABILITY
#define CHAFA_TERM_SEQ_AVAILABILITY CHAFA_AVAILABLE_IN_1_8

/**
 * chafa_term_info_emit_begin_kitty_immediate_image_v1:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @bpp: Bits per pixel
 * @width_pixels: Image width in pixels
 * @height_pixels: Image height in pixels
 * @width_cells: Target width in cells
 * @height_cells: Target height in cells
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_BEGIN_KITTY_IMAGE_IMMEDITATE.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * @bpp must be set to either 24 for RGB data, 32 for RGBA, or 100 to embed a
 * PNG file.
 *
 * This sequence must be followed by zero or more paired sequences of
 * type #CHAFA_TERM_SEQ_BEGIN_KITTY_IMAGE_CHUNK and #CHAFA_TERM_SEQ_END_KITTY_IMAGE_CHUNK
 * with base-64 encoded image data between them.
 *
 * When the image data has been transferred, #CHAFA_TERM_SEQ_END_KITTY_IMAGE must
 * be emitted.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.8
 **/
CHAFA_TERM_SEQ_DEF(begin_kitty_immediate_image_v1, BEGIN_KITTY_IMMEDIATE_IMAGE_V1, 5, none, guint, CHAFA_TERM_SEQ_ARGS guint bpp, guint width_pixels, guint height_pixels, guint width_cells, guint height_cells)

/**
 * chafa_term_info_emit_end_kitty_image:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_END_KITTY_IMAGE.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.8
 **/
CHAFA_TERM_SEQ_DEF(end_kitty_image, END_KITTY_IMAGE, 0, none, void)

/**
 * chafa_term_info_emit_begin_kitty_image_chunk:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_BEGIN_KITTY_IMAGE_CHUNK.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.8
 **/
CHAFA_TERM_SEQ_DEF(begin_kitty_image_chunk, BEGIN_KITTY_IMAGE_CHUNK, 0, none, void)

/**
 * chafa_term_info_emit_end_kitty_image_chunk:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_END_KITTY_IMAGE_CHUNK.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.8
 **/
CHAFA_TERM_SEQ_DEF(end_kitty_image_chunk, END_KITTY_IMAGE_CHUNK, 0, none, void)

/**
 * chafa_term_info_emit_begin_iterm2_image:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 * @width: Image width in character cells
 * @height: Image height in character cells
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_BEGIN_ITERM2_IMAGE_IMMEDITATE.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * This sequence must be followed by base64-encoded image file data. The image
 * can be any format supported by MacOS, e.g. PNG, JPEG, TIFF, GIF. When the
 * image data has been transferred, #CHAFA_TERM_SEQ_END_ITERM2_IMAGE must be
 * emitted.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.8
 **/
CHAFA_TERM_SEQ_DEF(begin_iterm2_image, BEGIN_ITERM2_IMAGE, 2, none, guint, CHAFA_TERM_SEQ_ARGS guint width, guint height)

/**
 * chafa_term_info_emit_end_iterm2_image:
 * @term_info: A #ChafaTermInfo
 * @dest: String destination
 *
 * Prints the control sequence for #CHAFA_TERM_SEQ_END_ITERM2_IMAGE.
 *
 * @dest must have enough space to hold
 * #CHAFA_TERM_SEQ_LENGTH_MAX bytes, even if the emitted sequence is
 * shorter. The output will not be zero-terminated.
 *
 * Returns: Pointer to first byte after emitted string
 *
 * Since: 1.8
 **/
CHAFA_TERM_SEQ_DEF(end_iterm2_image, END_ITERM2_IMAGE, 0, none, void)

#undef CHAFA_TERM_SEQ_AVAILABILITY

#undef CHAFA_TERM_SEQ_ARGS
