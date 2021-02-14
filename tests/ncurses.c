/* Example program that shows how to use a Chafa canvas with ncurses.
 *
 * To build:
 *
 * gcc ncurses.c $(pkg-config --libs --cflags chafa) $(ncursesw6-config --libs --cflags) -o ncurses
 */

#include <locale.h>
#include <ncurses.h>
#include <chafa.h>

/* Parameters for gradient pixmap. It will be scaled automatically to fit the canvas,
 * so this just needs to be big enough to avoid it getting too blurry. The number
 * of channels is always four, corresponding to CHAFA_PIXEL_RGBA8_UNASSOCIATED. */
#define PIXMAP_WIDTH 1024
#define PIXMAP_HEIGHT 1024
#define PIXMAP_N_CHANNELS 4

/* Terminal size detected in main loop */
static int screen_width, screen_height;

static ChafaCanvasMode
detect_canvas_mode (void)
{
    /* COLORS is a global variable defined by ncurses. It depends on termcap
     * for the terminal specified in TERM. In order to test the various modes, you
     * could try running this program with either of these:
     *
     * TERM=xterm
     * TERM=xterm-16color
     * TERM=xterm-256color
     * TERM=xterm-direct
     */
    return COLORS >= (1 << 24) ? CHAFA_CANVAS_MODE_TRUECOLOR
        : COLORS >= (1 << 8) ? CHAFA_CANVAS_MODE_INDEXED_240
        : COLORS >= (1 << 4) ? CHAFA_CANVAS_MODE_INDEXED_16
        : CHAFA_CANVAS_MODE_FGBG;
}

static ChafaCanvas *
create_canvas (void)
{
    ChafaSymbolMap *symbol_map;
    ChafaCanvasConfig *config;
    ChafaCanvas *canvas;
    ChafaCanvasMode mode = detect_canvas_mode ();

    /* Specify the symbols we want: Box drawing and block elements are both
     * useful and widely supported. */

    symbol_map = chafa_symbol_map_new ();
    chafa_symbol_map_add_by_tags (symbol_map, CHAFA_SYMBOL_TAG_SPACE);
    chafa_symbol_map_add_by_tags (symbol_map, CHAFA_SYMBOL_TAG_BLOCK);
    chafa_symbol_map_add_by_tags (symbol_map, CHAFA_SYMBOL_TAG_BORDER);

    /* Set up a configuration with the symbols and the canvas size in characters */

    config = chafa_canvas_config_new ();
    chafa_canvas_config_set_canvas_mode (config, mode);
    chafa_canvas_config_set_symbol_map (config, symbol_map);

    /* Reserve one row below canvas for status text */
    chafa_canvas_config_set_geometry (config, screen_width, screen_height - 1);

    /* Apply tweaks for low-color modes */

    if (mode == CHAFA_CANVAS_MODE_INDEXED_240)
    {
        /* We get better color fidelity using DIN99d in 240-color mode.
         * This is not needed in 16-color mode because it uses an extra
         * preprocessing step instead, which usually performs better. */
        chafa_canvas_config_set_color_space (config, CHAFA_COLOR_SPACE_DIN99D);
    }

    if (mode == CHAFA_CANVAS_MODE_FGBG)
    {
        /* Enable dithering in monochromatic mode so gradients become
         * somewhat legible. */
        chafa_canvas_config_set_dither_mode (config, CHAFA_DITHER_MODE_ORDERED);
    }

    /* Create canvas */

    canvas = chafa_canvas_new (config);

    chafa_symbol_map_unref (symbol_map);
    chafa_canvas_config_unref (config);
    return canvas;
}

static void
paint_canvas (ChafaCanvas *canvas)
{
    guint8 *pixmap;
    int x, y;

    /* Generate a gradient pixmap */

    pixmap = g_malloc (PIXMAP_WIDTH * PIXMAP_HEIGHT * PIXMAP_N_CHANNELS);

    for (y = 0; y < PIXMAP_HEIGHT; y++)
    {
        for (x = 0; x < PIXMAP_WIDTH; x++)
        {
            guint8 *pixel = pixmap + (y * PIXMAP_WIDTH + x) * PIXMAP_N_CHANNELS;
            pixel [0] = (x * 255) / PIXMAP_WIDTH;
            pixel [1] = (y * 255) / PIXMAP_HEIGHT;
            pixel [2] = 0;
            pixel [3] = 255;
        }
    }

    /* Paint it to the canvas */

    chafa_canvas_draw_all_pixels (canvas,
                                  CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                                  pixmap,
                                  PIXMAP_WIDTH,
                                  PIXMAP_HEIGHT,
                                  PIXMAP_WIDTH * PIXMAP_N_CHANNELS);

    g_free (pixmap);
}

static void
canvas_to_ncurses (ChafaCanvas *canvas)
{
    ChafaCanvasMode mode = detect_canvas_mode ();
    int pair = 256;  /* Reserve lower pairs for application in direct-color mode */
    int x, y;

    for (y = 0; y < screen_height - 1; y++)
    {
        for (x = 0; x < screen_width; x++)
        {
            wchar_t wc [2];
            cchar_t cch;
            int fg, bg;

            /* wchar_t is 32-bit in glibc, but this may not work on e.g. Windows */
            wc [0] = chafa_canvas_get_char_at (canvas, x, y);
            wc [1] = 0;

            if (mode == CHAFA_CANVAS_MODE_TRUECOLOR)
            {
                chafa_canvas_get_colors_at (canvas, x, y, &fg, &bg);
                init_extended_pair (pair, fg, bg);
            }
            else if (mode == CHAFA_CANVAS_MODE_FGBG)
            {
                pair = 0;
            }
            else
            {
                /* In indexed color mode, we've probably got enough pairs
                 * to just let ncurses allocate and reuse as needed. */
                chafa_canvas_get_raw_colors_at (canvas, x, y, &fg, &bg);
                pair = alloc_pair (fg, bg);
            }

            setcchar (&cch, wc, A_NORMAL, -1, &pair);
            mvadd_wch (y, x, &cch);
            pair++;
        }
    }
}

static void
show_image (void)
{
    ChafaCanvas *canvas;

    canvas = create_canvas ();

    paint_canvas (canvas);
    canvas_to_ncurses (canvas);
    mvprintw (screen_height - 1, 0, "%d colors detected. Try resizing, or press any key to exit.", COLORS);

    chafa_canvas_unref (canvas);
}

int
main (int argc, char *argv [])
{
    int running = TRUE;

    /* Set up locale to get proper Unicode */

    setlocale (LC_ALL, "");

    /* Start interactive ncurses session */

    initscr ();
    start_color ();
    use_default_colors ();
    raw ();
    keypad (stdscr, TRUE);
    noecho ();
    curs_set (0);

    /* Keep running until a key is pressed. Handle terminal resize. */

    while (running)
    {
        clear ();
        getmaxyx (stdscr, screen_height, screen_width);
        show_image ();
        refresh ();

        if (getch () != KEY_RESIZE)
            running = FALSE;
    }

    endwin ();
    return 0;
}
