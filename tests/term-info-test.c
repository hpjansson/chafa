#include "config.h"

#include <chafa.h>
#include <stdio.h>

static void
formatting_test (void)
{
    ChafaTermInfo *ti;
    gchar buf [CHAFA_TERM_SEQ_LENGTH_MAX * 14 + 1];
    gchar *out = buf;

    ti = chafa_term_info_new ();

    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_RESET_TERMINAL_SOFT, "soft-reset", NULL);
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_CURSOR_UP, "cursor-up-%1", NULL);
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_CURSOR_TO_POS, "%1-cursor-to-pos-%2", NULL);
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT, "%1%2-fg-direct-%3", NULL);
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_COLOR_BG_DIRECT, "%1-bg-direct%2%3-", NULL);
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT, "%1%2-fgbg-%3,%4%5-%6", NULL);
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_COLOR_FG_16, "aix%1,", NULL);
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_COLOR_BG_16, "aix%1,", NULL);
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_COLOR_FGBG_16, "aix-%1-%2,", NULL);
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG, "def-fg-%1-%2-%3,", NULL);
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_BG, "def-bg-%1-%2-%3,", NULL);

    out = chafa_term_info_emit_reset_terminal_soft (ti, out);
    out = chafa_term_info_emit_cursor_up (ti, out, 9876);
    out = chafa_term_info_emit_cursor_to_pos (ti, out, 1234, 0);
    out = chafa_term_info_emit_set_color_fg_direct (ti, out, 41, 0, 244);
    out = chafa_term_info_emit_set_color_bg_direct (ti, out, 0, 100, 99);
    out = chafa_term_info_emit_set_color_fgbg_direct (ti, out, 1, 199, 99, 0, 0, 9);
    out = chafa_term_info_emit_set_color_fg_16 (ti, out, 0);
    out = chafa_term_info_emit_set_color_fg_16 (ti, out, 8);
    out = chafa_term_info_emit_set_color_bg_16 (ti, out, 0);
    out = chafa_term_info_emit_set_color_bg_16 (ti, out, 8);
    out = chafa_term_info_emit_set_color_fgbg_16 (ti, out, 0, 0);
    out = chafa_term_info_emit_set_color_fgbg_16 (ti, out, 8, 8);
    out = chafa_term_info_emit_set_default_fg (ti, out, 0xffff, 0x0000, 0x1234);
    out = chafa_term_info_emit_set_default_bg (ti, out, 0x1234, 0xffff, 0x0000);

    *out = '\0';

    chafa_term_info_unref (ti);

    g_assert_cmpstr (buf, ==, 
                     "soft-reset"
                     "cursor-up-9876"
                     "1235-cursor-to-pos-1"
                     "410-fg-direct-244"
                     "0-bg-direct10099-"
                     "1199-fgbg-99,00-9"
                     "aix30,"
                     "aix90,"
                     "aix40,"
                     "aix100,"
                     "aix-30-40,"
                     "aix-90-100,"
                     "def-fg-ffff-0000-1234,"
                     "def-bg-1234-ffff-0000,");
}

static void
dynamic_test (void)
{
    ChafaTermInfo *ti;
    gchar buf [CHAFA_TERM_SEQ_LENGTH_MAX + 1];
    gchar *out = buf;
    gchar *dyn;

    ti = chafa_term_info_new ();

    /* No args */
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_RESET_TERMINAL_SOFT, "reset-soft", NULL);
    out = chafa_term_info_emit_reset_terminal_soft (ti, buf);
    *out = '\0';
    dyn = chafa_term_info_emit_seq (ti, CHAFA_TERM_SEQ_RESET_TERMINAL_SOFT, -1);
    g_assert_cmpstr (buf, ==, dyn);
    g_free (dyn);

    /* 8-bit args */
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT, "%1%2-fg-direct-%3", NULL);
    out = chafa_term_info_emit_set_color_fg_direct (ti, buf, 0xff, 0x00, 0x12);
    *out = '\0';
    dyn = chafa_term_info_emit_seq (ti, CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT,
                                    0xff, 0x00, 0x12, -1);
    g_assert_cmpstr (buf, ==, dyn);
    g_free (dyn);

    /* uint args */
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_CURSOR_TO_POS, "%1-cursor-to-pos-%2", NULL);
    out = chafa_term_info_emit_cursor_to_pos (ti, buf, 100000, 200000);
    *out = '\0';
    dyn = chafa_term_info_emit_seq (ti, CHAFA_TERM_SEQ_CURSOR_TO_POS,
                                    100000, 200000, -1);
    g_assert_cmpstr (buf, ==, dyn);
    g_free (dyn);

    /* 16-bit hex args */
    chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG, "def-fg-%1-%2-%3,", NULL);

    out = chafa_term_info_emit_set_default_fg (ti, buf, 0xffff, 0x0000, 0x1234);
    *out = '\0';
    dyn = chafa_term_info_emit_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG,
                                    0xffff, 0x0000, 0x1234, -1);
    g_assert_cmpstr (buf, ==, dyn);
    g_free (dyn);

    /* Arg out of range */
    dyn = chafa_term_info_emit_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG,
                                    0xffff, 0x10000, 0x1234, 0, -1);
    g_assert (dyn == NULL);

    /* Too many args */
    dyn = chafa_term_info_emit_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG,
                                    0xffff, 0x0000, 0x1234, 0, -1);
    g_assert (dyn == NULL);

    /* Too few args */
    dyn = chafa_term_info_emit_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG,
                                    0xffff, 0x0000, -1);
    g_assert (dyn == NULL);

    /* Too few (zero) args */
    dyn = chafa_term_info_emit_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG, -1);
    g_assert (dyn == NULL);
}

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/term-info/formatting", formatting_test);
    g_test_add_func ("/term-info/dynamic", dynamic_test);

    return g_test_run ();
}
