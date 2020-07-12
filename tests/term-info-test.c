#include <chafa.h>
#include <stdio.h>

static void
formatting_test (void)
{
    ChafaTermInfo *ti;
    gchar buf [CHAFA_TERM_SEQ_LENGTH_MAX * 12 + 1];
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

    *out = '\0';

    chafa_term_info_unref (ti);

    g_assert_cmpstr (buf, ==, 
                     "soft-reset"
                     "cursor-up-9876"
                     "1234-cursor-to-pos-0"
                     "410-fg-direct-244"
                     "0-bg-direct10099-"
                     "1199-fgbg-99,00-9"
                     "aix30,"
                     "aix90,"
                     "aix40,"
                     "aix100,"
                     "aix-30-40,"
                     "aix-90-100,");
}

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/term-info/formatting", formatting_test);

    return g_test_run ();
}
