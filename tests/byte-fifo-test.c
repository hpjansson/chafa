#include "config.h"

#include <chafa.h>
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

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/byte-fifo", byte_fifo_test);

    return g_test_run ();
}
