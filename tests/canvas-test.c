#include "config.h"

#include <chafa.h>
#include <stdio.h>

static void
symbols_test (void)
{
    const guint8 black_pixel [4] =
    {
        0x00, 0x00, 0x00, 0xff
    };
    const guint8 white_pixel [4] =
    {
        0xff, 0xff, 0xff, 0xff
    };
    ChafaSymbolMap *symbol_map;
    ChafaCanvasConfig *config;
    ChafaCanvas *canvas;
    gint i, j;

    symbol_map = chafa_symbol_map_new ();
    chafa_symbol_map_apply_selectors (symbol_map, "[ a]", NULL);

    config = chafa_canvas_config_new ();
    chafa_canvas_config_set_canvas_mode (config, CHAFA_CANVAS_MODE_FGBG_BGFG);
    chafa_canvas_config_set_symbol_map (config, symbol_map);
    chafa_canvas_config_set_geometry (config, 100, 100);
    chafa_canvas_config_set_fg_only_enabled (config, TRUE);

    canvas = chafa_canvas_new (config);

    chafa_canvas_draw_all_pixels (canvas,
                                  CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                                  black_pixel,
                                  1, 1, 4);
    for (i = 0; i < 100; i++)
    {
        for (j = 0; j < 100; j++)
        {
            gunichar c = chafa_canvas_get_char_at (canvas, j, i);
            g_assert (c == ' ');
        }
    }

    chafa_canvas_draw_all_pixels (canvas,
                                  CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                                  white_pixel,
                                  1, 1, 4);
    for (i = 0; i < 100; i++)
    {
        for (j = 0; j < 100; j++)
        {
            gunichar c = chafa_canvas_get_char_at (canvas, j, i);
            g_assert (c == 'a');
        }
    }

    chafa_canvas_unref (canvas);
    chafa_canvas_config_unref (config);
    chafa_symbol_map_unref (symbol_map);
}

int
main (int argc, char *argv [])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/canvas/symbols", symbols_test);

    return g_test_run ();
}
