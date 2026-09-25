#include "config.h"

#include <chafa.h>
#include "internal/chafa-byte-fifo.h"
#include <stdio.h>

static void
byte_fifo_test (void)
{
    ChafaByteFifo *byte_fifo;
    gchar buf [32768];
    gint result;

    memset (buf, 'x', 32768);

    byte_fifo = chafa_byte_fifo_new ();

    chafa_byte_fifo_push (byte_fifo, "abc", 3);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == 0);

    chafa_byte_fifo_drop (byte_fifo, 3);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == -1);

    chafa_byte_fifo_push (byte_fifo, "ababababcababab", 15);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == 6);

    chafa_byte_fifo_pop (byte_fifo, NULL, 1);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == 5);

    chafa_byte_fifo_push (byte_fifo, buf, 30000);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == 5);

    chafa_byte_fifo_drop (byte_fifo, 10);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == -1);

    chafa_byte_fifo_push (byte_fifo, "abc", 3);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == 30004);

    chafa_byte_fifo_drop (byte_fifo, 100000);
    result = chafa_byte_fifo_search (byte_fifo, "abc", 3, NULL);
    g_assert (result == -1);

    chafa_byte_fifo_push (byte_fifo, buf, 16380);
    chafa_byte_fifo_push (byte_fifo, "abracadabra", 11);
    result = chafa_byte_fifo_search (byte_fifo, "abracadabra", 11, NULL);
    g_assert (result == 16380);

    chafa_byte_fifo_drop (byte_fifo, 100000);
    chafa_byte_fifo_push (byte_fifo, buf, 16380);
    chafa_byte_fifo_push (byte_fifo, "abracadfrumpy", 13);
    result = chafa_byte_fifo_search (byte_fifo, "abracadabra", 11, NULL);
    g_assert (result == -1);

    chafa_byte_fifo_push (byte_fifo, "abracadabra", 11);
    result = chafa_byte_fifo_search (byte_fifo, "abracadabra", 11, NULL);
    g_assert (result == 16393);

    chafa_byte_fifo_unref (byte_fifo);
}

/* On a miss, the restart position must advance to the first byte that
 * could still begin a match once more data arrives. */
static void
byte_fifo_restart_pos_test (void)
{
    ChafaByteFifo *byte_fifo;
    gint64 pos;
    gint result;

    byte_fifo = chafa_byte_fifo_new ();

    /* Nothing in the buffer can start a match */

    pos = 0;
    chafa_byte_fifo_push (byte_fifo, "abc", 3);
    result = chafa_byte_fifo_search (byte_fifo, "\n", 1, &pos);
    g_assert_cmpint (result, ==, -1);
    g_assert_cmpint (pos, ==, 3);

    /* Match after subsequent push */

    chafa_byte_fifo_push (byte_fifo, "d\n", 2);
    result = chafa_byte_fifo_search (byte_fifo, "\n", 1, &pos);
    g_assert_cmpint (result, ==, 4);
    g_assert_cmpint (pos, ==, 4);

    /* Consume through the separator; position clamped to new tail */

    chafa_byte_fifo_drop (byte_fifo, 5);
    result = chafa_byte_fifo_search (byte_fifo, "\n", 1, &pos);
    g_assert_cmpint (result, ==, -1);
    g_assert_cmpint (pos, ==, 5);

    /* Multibyte separator; retry partial match near tail */

    chafa_byte_fifo_push (byte_fifo, "ab-", 3);
    result = chafa_byte_fifo_search (byte_fifo, "--", 2, &pos);
    g_assert_cmpint (result, ==, -1);
    g_assert_cmpint (pos, ==, 7);

    chafa_byte_fifo_push (byte_fifo, "-", 1);
    result = chafa_byte_fifo_search (byte_fifo, "--", 2, &pos);
    g_assert_cmpint (result, ==, 2);
    g_assert_cmpint (pos, ==, 7);

    /* Separator longer than the buffer */

    chafa_byte_fifo_drop (byte_fifo, 100);
    chafa_byte_fifo_push (byte_fifo, "ab", 2);
    result = chafa_byte_fifo_search (byte_fifo, "abcd", 4, &pos);
    g_assert_cmpint (result, ==, -1);
    g_assert_cmpint (pos, ==, 9);

    chafa_byte_fifo_unref (byte_fifo);
}

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/byte-fifo", byte_fifo_test);
    g_test_add_func ("/byte-fifo/restart-pos", byte_fifo_restart_pos_test);

    return g_test_run ();
}
