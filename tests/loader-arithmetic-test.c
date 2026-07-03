#include "config.h"

#include <chafa.h>
#include "chicle-util.h"

#define IMAGE_BUFFER_SIZE_MAX (0xffffffffU >> 2)

static void
loader_arithmetic_test_overflow_rejected (void)
{
    guint width = G_MAXUINT;
    guint height = G_MAXUINT;
    guint n_channels = 4;
    gsize size;

    g_assert_false (chicle_checked_image_buffer_size (width, height, n_channels,
                                                     IMAGE_BUFFER_SIZE_MAX, &size));
}

static void
loader_arithmetic_test_zero_dimensions_rejected (void)
{
    gsize size;

    g_assert_false (chicle_checked_image_buffer_size (0, 1, 4,
                                                     IMAGE_BUFFER_SIZE_MAX, &size));
    g_assert_false (chicle_checked_image_buffer_size (1, 0, 4,
                                                     IMAGE_BUFFER_SIZE_MAX, &size));
    g_assert_false (chicle_checked_image_buffer_size (1, 1, 0,
                                                     IMAGE_BUFFER_SIZE_MAX, &size));
}

static void
loader_arithmetic_test_max_size_boundary (void)
{
    gsize size = 0;

    g_assert_true (chicle_checked_image_buffer_size (16, 16, 4,
                                                    1024, &size));
    g_assert_cmpuint (size, ==, 1024);

    g_assert_false (chicle_checked_image_buffer_size (16, 16, 4,
                                                     1023, &size));
}

static void
loader_arithmetic_test_gsize_channel_boundary (void)
{
    gsize size = 0;

    g_assert_true (chicle_checked_image_buffer_size (G_MAXSIZE / 4, 1, 4,
                                                    G_MAXSIZE, &size));
    g_assert_cmpuint (size, ==, G_MAXSIZE - (G_MAXSIZE % 4));

    g_assert_false (chicle_checked_image_buffer_size ((G_MAXSIZE / 4) + 1, 1, 4,
                                                     G_MAXSIZE, &size));
}

static void
loader_arithmetic_test_valid_size_accepted (void)
{
    gsize size = 0;

    g_assert_true (chicle_checked_image_buffer_size (16, 8, 4,
                                                    IMAGE_BUFFER_SIZE_MAX, &size));
    g_assert_cmpuint (size, ==, 512);
}

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/loader-arithmetic/overflow-rejected",
                     loader_arithmetic_test_overflow_rejected);
    g_test_add_func ("/loader-arithmetic/zero-dimensions-rejected",
                     loader_arithmetic_test_zero_dimensions_rejected);
    g_test_add_func ("/loader-arithmetic/max-size-boundary",
                     loader_arithmetic_test_max_size_boundary);
    g_test_add_func ("/loader-arithmetic/gsize-channel-boundary",
                     loader_arithmetic_test_gsize_channel_boundary);
    g_test_add_func ("/loader-arithmetic/valid-size-accepted",
                     loader_arithmetic_test_valid_size_accepted);

    return g_test_run ();
}
