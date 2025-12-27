#include "config.h"

#include <chafa.h>
#include <stdio.h>

static void
dump_char_buf (const gunichar *char_buf, gint width, gint height)
{
    gint x, y;
    gchar *row, *p;

    row = g_malloc (width * 6 + 1);

    for (y = 0; y < height; y++)
    {
        p = row;

        for (x = 0; x < width; x++)
        {
            gint len;

            len = g_unichar_to_utf8 (char_buf [(y * width) + x], p);
            g_assert (len > 0);
            p += len;
        }

        *(p++) = '\n';
        fwrite (row, sizeof (gchar), p - row, stdout);
    }

    g_free (row);
}

static gunichar *
extract_char_buf (ChafaCanvas *canvas, gint width, gint height)
{
    gunichar *char_buf;
    gint x, y;

    char_buf = g_malloc (width * height * sizeof (gunichar));

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            char_buf [(y * width) + x] = chafa_canvas_get_char_at (canvas, x, y);
        }
    }

    return char_buf;
}

static gboolean
validate_char_buf (gunichar *char_buf, gint width, gint height,
                   gunichar expected_char)
{
    gint x, y;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            if (char_buf [(y * width) + x] != expected_char)
                return FALSE;
        }
    }

    return TRUE;
}

static gboolean
test_color_char (ChafaCanvas *canvas, ChafaPixelType pixel_type,
                 const guint8 *pixel, gunichar expected_char)
{
    gint width, height;
    gunichar *char_buf;
    gboolean result;

    chafa_canvas_config_get_geometry (chafa_canvas_peek_config (canvas),
                                      &width, &height);
    g_assert (width > 0);
    g_assert (height > 0);

    chafa_canvas_draw_all_pixels (canvas,
                                  pixel_type,
                                  pixel,
                                  1, 1, 4);
    char_buf = extract_char_buf (canvas, width, height);

    result = validate_char_buf (char_buf, width, height, expected_char);

    if (!result)
    {
        g_print ("Unexpected canvas buffer (%dx%d, want '%c', ptype %d):\n",
                 width, height,
                 (gchar) expected_char,
                 pixel_type);
        dump_char_buf (char_buf, width, height);
    }

    g_free (char_buf);
    return result;
}

static void
symbols_fgbg_test_bw_canvas (ChafaCanvas *canvas, gchar black_char, gchar white_char)
{
    const guint8 black_pixel_rgba8 [4] = { 0x00, 0x00, 0x00, 0xff };
    const guint8 black_pixel_argb8 [4] = { 0xff, 0x00, 0x00, 0x00 };
    const guint8 white_pixel [4] = { 0xff, 0xff, 0xff, 0xff };
    gboolean result = TRUE;

    result &= test_color_char (canvas,
                               CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                               black_pixel_rgba8,
                               black_char);
    result &= test_color_char (canvas,
                               CHAFA_PIXEL_ARGB8_UNASSOCIATED,
                               black_pixel_argb8,
                               black_char);
    result &= test_color_char (canvas,
                               CHAFA_PIXEL_RGBA8_PREMULTIPLIED,
                               black_pixel_rgba8,
                               black_char);
    result &= test_color_char (canvas,
                               CHAFA_PIXEL_ARGB8_PREMULTIPLIED,
                               black_pixel_argb8,
                               black_char);
    result &= test_color_char (canvas,
                               CHAFA_PIXEL_RGB8,
                               black_pixel_rgba8,
                               black_char);
    result &= test_color_char (canvas,
                               CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                               white_pixel,
                               white_char);
    result &= test_color_char (canvas,
                               CHAFA_PIXEL_RGBA8_PREMULTIPLIED,
                               white_pixel,
                               white_char);
    result &= test_color_char (canvas,
                               CHAFA_PIXEL_BGR8,
                               white_pixel,
                               white_char);

    g_assert (result == TRUE);
}

static void
symbols_fgbg_test_bw_params (ChafaCanvasMode canvas_mode, gboolean fg_only,
                             gint width, gint height,
                             const gchar *selectors, gchar black_char, gchar white_char,
                             gfloat work_factor)
{
    ChafaSymbolMap *symbol_map;
    ChafaCanvasConfig *config;
    ChafaCanvas *canvas, *canvas2;

    symbol_map = chafa_symbol_map_new ();
    chafa_symbol_map_apply_selectors (symbol_map, selectors, NULL);

    config = chafa_canvas_config_new ();
    chafa_canvas_config_set_canvas_mode (config, canvas_mode);
    chafa_canvas_config_set_symbol_map (config, symbol_map);
    chafa_canvas_config_set_geometry (config, width, height);
    chafa_canvas_config_set_fg_only_enabled (config, fg_only);
    chafa_canvas_config_set_work_factor (config, work_factor);

    canvas = chafa_canvas_new (config);
    symbols_fgbg_test_bw_canvas (canvas, black_char, white_char);

    canvas2 = chafa_canvas_new_similar (canvas);
    chafa_canvas_unref (canvas);
    symbols_fgbg_test_bw_canvas (canvas2, black_char, white_char);
    chafa_canvas_unref (canvas2);

    chafa_canvas_config_unref (config);
    chafa_symbol_map_unref (symbol_map);
}

static void
symbols_fgbg_test (void)
{
    symbols_fgbg_test_bw_params (CHAFA_CANVAS_MODE_FGBG_BGFG, TRUE,
                                 10, 10, "[ a]", ' ', 'a', 0.5f);
    symbols_fgbg_test_bw_params (CHAFA_CANVAS_MODE_FGBG_BGFG, TRUE,
                                 17, 17, "[ .]", ' ', '.', 0.2f);
    symbols_fgbg_test_bw_params (CHAFA_CANVAS_MODE_FGBG_BGFG, TRUE,
                                 1, 1, "[.Q]", '.', 'Q', 0.8f);
    symbols_fgbg_test_bw_params (CHAFA_CANVAS_MODE_FGBG, FALSE,
                                 10, 10, "[ ']", ' ', '\'', 1.0f);
    symbols_fgbg_test_bw_params (CHAFA_CANVAS_MODE_FGBG, TRUE,
                                 23, 23, "[ .]", ' ', '.', 0.05f);
    symbols_fgbg_test_bw_params (CHAFA_CANVAS_MODE_FGBG, FALSE,
                                 3, 3, "[ /]", ' ', '/', 0.2f);
    symbols_fgbg_test_bw_params (CHAFA_CANVAS_MODE_FGBG_BGFG, TRUE,
                                 41, 41, "[ .Q]", ' ', 'Q', 0.5f);
    symbols_fgbg_test_bw_params (CHAFA_CANVAS_MODE_FGBG_BGFG, TRUE,
                                 100, 100, "[ a]", ' ', 'a', 0.2f);

static void
symbols_fgbg_test_st (void)
{
    chafa_set_n_threads (1);
    symbols_fgbg_test ();
    chafa_set_n_threads (-1);
}

static void
symbols_fgbg_test_mt (void)
{
    chafa_set_n_threads (-1);
    symbols_fgbg_test ();
}

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/canvas/symbols/fgbg/st", symbols_fgbg_test_st);
    g_test_add_func ("/canvas/symbols/fgbg/mt", symbols_fgbg_test_mt);

    return g_test_run ();
}
