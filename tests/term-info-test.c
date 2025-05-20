#include "config.h"

#include <chafa.h>
#include <stdio.h>
#include <string.h>

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

    chafa_term_info_unref (ti);
}

static void
parsing_test (void)
{
    ChafaTermInfo *ti;
    gchar *dyn;
    gint dyn_len;
    gchar *p;
    guint args [CHAFA_TERM_SEQ_ARGS_MAX];
    gint n_args;
    ChafaParseResult result;
    gboolean r;
    guint i;

    ti = chafa_term_info_new ();

    /* Define and emit */

    r = chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG, "def-fg-%1-%2-%3,", NULL);
    g_assert (r == TRUE);
    dyn = chafa_term_info_emit_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG,
                                    0xffff, 0x0000, 0x1234, -1);

    /* Parse success */

    p = dyn;
    dyn_len = strlen (dyn);
    result = chafa_term_info_parse_seq_varargs (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG, &p, &dyn_len, args, &n_args);
    g_assert (result == CHAFA_PARSE_SUCCESS);
    g_assert (n_args == 3);
    g_assert (args [0] == 0xffff);
    g_assert (args [1] == 0x0000);
    g_assert (args [2] == 0x1234);

    /* Not enough data */

    for (i = 0; i < strlen (dyn); i++)
    {
        p = dyn;
        dyn_len = i;
        result = chafa_term_info_parse_seq_varargs (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG, &p, &dyn_len, args, &n_args);
        g_assert (result == CHAFA_PARSE_AGAIN);
    }

    /* Parse failure */

    p = dyn + 1;
    dyn_len = strlen (dyn) - 1;
    result = chafa_term_info_parse_seq_varargs (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG, &p, &dyn_len, args, &n_args);
    g_assert (result == CHAFA_PARSE_FAILURE);

    g_free (dyn);
    chafa_term_info_unref (ti);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
parsing_legacy_test (void)
{
    ChafaTermInfo *ti;
    gchar *dyn;
    gint dyn_len;
    gchar *p;
    guint args [CHAFA_TERM_SEQ_ARGS_MAX];
    ChafaParseResult result;
    gboolean r;
    guint i;

    ti = chafa_term_info_new ();

    /* Define and emit */

    r = chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG, "def-fg-%1-%2-%3,", NULL);
    g_assert (r == TRUE);
    dyn = chafa_term_info_emit_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG,
                                    0xffff, 0x0000, 0x1234, -1);

    /* Parse success */

    p = dyn;
    dyn_len = strlen (dyn);
    result = chafa_term_info_parse_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG, &p, &dyn_len, args);
    g_assert (result == CHAFA_PARSE_SUCCESS);
    g_assert (args [0] == 0xffff);
    g_assert (args [1] == 0x0000);
    g_assert (args [2] == 0x1234);

    /* Not enough data */

    for (i = 0; i < strlen (dyn); i++)
    {
        p = dyn;
        dyn_len = i;
        result = chafa_term_info_parse_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG, &p, &dyn_len, args);
        g_assert (result == CHAFA_PARSE_AGAIN);
    }

    /* Parse failure */

    p = dyn + 1;
    dyn_len = strlen (dyn) - 1;
    result = chafa_term_info_parse_seq (ti, CHAFA_TERM_SEQ_SET_DEFAULT_FG, &p, &dyn_len, args);
    g_assert (result == CHAFA_PARSE_FAILURE);

    g_free (dyn);
    chafa_term_info_unref (ti);
}
G_GNUC_END_IGNORE_DEPRECATIONS

static void
parsing_varargs_test (void)
{
    ChafaTermInfo *ti;
    gchar *dyn;
    gint dyn_len;
    gchar *p;
    guint args [CHAFA_TERM_SEQ_ARGS_MAX];
    gint n_args;
    ChafaParseResult result;
    gboolean r;
    guint i;

    ti = chafa_term_info_new ();

    /* Define and emit */

    r = chafa_term_info_set_seq (ti, CHAFA_TERM_SEQ_PRIMARY_DEVICE_ATTRIBUTES, "attr-%v,", NULL);
    g_assert (r == TRUE);
    dyn = chafa_term_info_emit_seq (ti, CHAFA_TERM_SEQ_PRIMARY_DEVICE_ATTRIBUTES,
                                    0xff, 0x0000, 0x1234, -1);
    g_assert (strcmp (dyn, "attr-255;0;4660,") == 0);

    /* Parse success */

    p = dyn;
    dyn_len = strlen (dyn);
    result = chafa_term_info_parse_seq_varargs (ti, CHAFA_TERM_SEQ_PRIMARY_DEVICE_ATTRIBUTES, &p, &dyn_len, args, &n_args);
    g_assert (result == CHAFA_PARSE_SUCCESS);
    g_assert (n_args == 3);
    g_assert (args [0] == 0xff);
    g_assert (args [1] == 0x0000);
    g_assert (args [2] == 0x1234);

    /* Not enough data */

    for (i = 0; i < strlen (dyn); i++)
    {
        p = dyn;
        dyn_len = i;
        result = chafa_term_info_parse_seq_varargs (ti, CHAFA_TERM_SEQ_PRIMARY_DEVICE_ATTRIBUTES, &p, &dyn_len, args, &n_args);
        g_assert (result == CHAFA_PARSE_AGAIN);
    }

    /* Parse failure */

    p = dyn + 1;
    dyn_len = strlen (dyn) - 1;
    result = chafa_term_info_parse_seq_varargs (ti, CHAFA_TERM_SEQ_PRIMARY_DEVICE_ATTRIBUTES, &p, &dyn_len, args, &n_args);
    g_assert (result == CHAFA_PARSE_FAILURE);

    g_free (dyn);
    chafa_term_info_unref (ti);
}

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/term-info/formatting", formatting_test);
    g_test_add_func ("/term-info/dynamic", dynamic_test);
    g_test_add_func ("/term-info/parsing", parsing_test);
    g_test_add_func ("/term-info/parsing-legacy", parsing_legacy_test);
    g_test_add_func ("/term-info/parsing-varargs", parsing_varargs_test);

    return g_test_run ();
}
