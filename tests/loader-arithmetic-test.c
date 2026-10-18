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

    g_assert_false (chicle_calc_image_buffer_size (width, height, n_channels,
                                                   IMAGE_BUFFER_SIZE_MAX, &size));
}

static void
loader_arithmetic_test_zero_dimensions_rejected (void)
{
    gsize size;

    g_assert_false (chicle_calc_image_buffer_size (0, 1, 4,
                                                   IMAGE_BUFFER_SIZE_MAX, &size));
    g_assert_false (chicle_calc_image_buffer_size (1, 0, 4,
                                                   IMAGE_BUFFER_SIZE_MAX, &size));
    g_assert_false (chicle_calc_image_buffer_size (1, 1, 0,
                                                   IMAGE_BUFFER_SIZE_MAX, &size));
}

static void
loader_arithmetic_test_max_size_boundary (void)
{
    gsize size = 0;

    g_assert_true (chicle_calc_image_buffer_size (16, 16, 4,
                                                  1024, &size));
    g_assert_cmpuint (size, ==, 1024);

    g_assert_false (chicle_calc_image_buffer_size (16, 16, 4,
                                                   1023, &size));
}

static void
loader_arithmetic_test_extent_boundary (void)
{
    gsize size = 0;

    /* A side of exactly CHICLE_IMAGE_EXTENT_PX_MAX is fine. One more is not,
     * even when the buffer itself would be tiny */
    g_assert_true (chicle_calc_image_buffer_size (CHICLE_IMAGE_EXTENT_PX_MAX, 1, 1,
                                                  G_MAXUINT64, &size));
    g_assert_cmpuint (size, ==, CHICLE_IMAGE_EXTENT_PX_MAX);
    g_assert_true (chicle_calc_image_buffer_size (1, CHICLE_IMAGE_EXTENT_PX_MAX, 1,
                                                  G_MAXUINT64, &size));
    g_assert_false (chicle_calc_image_buffer_size (CHICLE_IMAGE_EXTENT_PX_MAX + 1, 1, 1,
                                                   G_MAXUINT64, &size));
    g_assert_false (chicle_calc_image_buffer_size (1, CHICLE_IMAGE_EXTENT_PX_MAX + 1, 1,
                                                   G_MAXUINT64, &size));
    g_assert_false (chicle_calc_image_buffer_size (8000000, 30, 4,
                                                   IMAGE_BUFFER_SIZE_MAX, &size));
}

static void
loader_arithmetic_test_valid_size_accepted (void)
{
    gsize size = 0;

    g_assert_true (chicle_calc_image_buffer_size (16, 8, 4,
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
    g_test_add_func ("/loader-arithmetic/extent-boundary",
                     loader_arithmetic_test_extent_boundary);
    g_test_add_func ("/loader-arithmetic/valid-size-accepted",
                     loader_arithmetic_test_valid_size_accepted);

    return g_test_run ();
}
