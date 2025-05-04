/* This file is in the public domain, and you are free to use it as you see fit.
 *
 * Compile with:
 *
 * cc example.c -o example $(pkg-config --libs --cflags chafa)
 */

#include <chafa.h>
#include <stdio.h>

#define PIX_WIDTH 3
#define PIX_HEIGHT 3
#define N_CHANNELS 4

int
main (int argc, char *argv [])
{
    const guint8 pixels [PIX_WIDTH * PIX_HEIGHT * N_CHANNELS] =
    {
        0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff,
        0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff,
        0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0xff
    };
    ChafaSymbolMap *symbol_map;
    ChafaCanvasConfig *config;
    ChafaCanvas *canvas;
    GString *gs;

    /* Specify the symbols we want */
    symbol_map = chafa_symbol_map_new ();
    chafa_symbol_map_add_by_tags (symbol_map, CHAFA_SYMBOL_TAG_ALL);

    /* Set up a configuration with the symbols and the canvas size in characters */
    config = chafa_canvas_config_new ();
    chafa_canvas_config_set_geometry (config, 40, 20);
    chafa_canvas_config_set_symbol_map (config, symbol_map);

    /* Create canvas */
    canvas = chafa_canvas_new (config);

    /* Draw pixels and build ANSI string */

    /* Test a deprecated function */
    chafa_canvas_set_contents_rgba8 (canvas,
                                     pixels,
                                     PIX_WIDTH,
                                     PIX_HEIGHT,
                                     PIX_WIDTH * N_CHANNELS);

    chafa_canvas_draw_all_pixels (canvas,
                                  CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                                  pixels,
                                  PIX_WIDTH,
                                  PIX_HEIGHT,
                                  PIX_WIDTH * N_CHANNELS);

    gs = chafa_canvas_build_ansi (canvas);

    /* Print the string */
    fwrite (gs->str, sizeof (char), gs->len, stdout);
    fputc ('\n', stdout);

    /* Free resources */
    g_string_free (gs, TRUE);
    chafa_canvas_unref (canvas);
    chafa_canvas_config_unref (config);
    chafa_symbol_map_unref (symbol_map);

    return 0;
}
