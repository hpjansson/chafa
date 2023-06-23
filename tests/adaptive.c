/* This file is in the public domain, and you are free to use it as you see fit.
 *
 * Compile with:
 *
 * cc adaptive.c -o adaptive $(pkg-config --libs --cflags chafa)
 */

#include <chafa.h>
#include <stdio.h>
#include <unistd.h>

/* Include after chafa.h for G_OS_WIN32 */
#ifdef G_OS_WIN32
# ifdef HAVE_WINDOWS_H
#  include <windows.h>
# endif
# include <io.h>
#else
# include <sys/ioctl.h>  /* ioctl */
#endif

typedef struct
{
    gint width_cells, height_cells;
    gint width_pixels, height_pixels;
}
TermSize;

static void
detect_terminal (ChafaTermInfo **term_info_out, ChafaCanvasMode *mode_out, ChafaPixelMode *pixel_mode_out)
{
    ChafaCanvasMode mode;
    ChafaPixelMode pixel_mode;
    ChafaTermInfo *term_info;
    ChafaTermInfo *fallback_info;
    gchar **envp;

    /* Examine the environment variables and guess what the terminal can do */

    envp = g_get_environ ();
    term_info = chafa_term_db_detect (chafa_term_db_get_default (), envp);

    /* See which control sequences were defined, and use that to pick the most
     * high-quality rendering possible */

    if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_BEGIN_KITTY_IMMEDIATE_IMAGE_V1))
    {
        pixel_mode = CHAFA_PIXEL_MODE_KITTY;
        mode = CHAFA_CANVAS_MODE_TRUECOLOR;
    }
    else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_BEGIN_SIXELS))
    {
        pixel_mode = CHAFA_PIXEL_MODE_SIXELS;
        mode = CHAFA_CANVAS_MODE_TRUECOLOR;
    }
    else
    {
        pixel_mode = CHAFA_PIXEL_MODE_SYMBOLS;

        if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT)
            && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FG_DIRECT)
            && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_BG_DIRECT))
            mode = CHAFA_CANVAS_MODE_TRUECOLOR;
        else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_256)
                 && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FG_256)
                 && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_BG_256))
            mode = CHAFA_CANVAS_MODE_INDEXED_240;
        else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_16)
                 && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FG_16)
                 && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_BG_16))
            mode = CHAFA_CANVAS_MODE_INDEXED_16;
        else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_INVERT_COLORS)
                 && chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_RESET_ATTRIBUTES))
            mode = CHAFA_CANVAS_MODE_FGBG_BGFG;
        else
            mode = CHAFA_CANVAS_MODE_FGBG;
    }

    /* Hand over the information to caller */

    *term_info_out = term_info;
    *mode_out = mode;
    *pixel_mode_out = pixel_mode;

    /* Cleanup */

    g_strfreev (envp);
}

static void
get_tty_size (TermSize *term_size_out)
{
    TermSize term_size;

    term_size.width_cells
        = term_size.height_cells
        = term_size.width_pixels
        = term_size.height_pixels
        = -1;

#ifdef G_OS_WIN32
    {
        HANDLE chd = GetStdHandle (STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csb_info;

        if (chd != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo (chd, &csb_info))
        {
            term_size.width_cells = csb_info.srWindow.Right - csb_info.srWindow.Left + 1;
            term_size.height_cells = csb_info.srWindow.Bottom - csb_info.srWindow.Top + 1;
        }
    }
#else
    {
        struct winsize w;
        gboolean have_winsz = FALSE;

        if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &w) >= 0
            || ioctl (STDERR_FILENO, TIOCGWINSZ, &w) >= 0
            || ioctl (STDIN_FILENO, TIOCGWINSZ, &w) >= 0)
            have_winsz = TRUE;

        if (have_winsz)
        {
            term_size.width_cells = w.ws_col;
            term_size.height_cells = w.ws_row;
            term_size.width_pixels = w.ws_xpixel;
            term_size.height_pixels = w.ws_ypixel;
        }
    }
#endif

    if (term_size.width_cells <= 0)
        term_size.width_cells = -1;
    if (term_size.height_cells <= 2)
        term_size.height_cells = -1;

    /* If .ws_xpixel and .ws_ypixel are filled out, we can calculate
     * aspect information for the font used. Sixel-capable terminals
     * like mlterm set these fields, but most others do not. */

    if (term_size.width_pixels <= 0 || term_size.height_pixels <= 0)
    {
        term_size.width_pixels = -1;
        term_size.height_pixels = -1;
    }

    *term_size_out = term_size;
}

static void
tty_init (void)
{
#ifdef G_OS_WIN32
    {
        HANDLE chd = GetStdHandle (STD_OUTPUT_HANDLE);

        saved_console_output_cp = GetConsoleOutputCP ();
        saved_console_input_cp = GetConsoleCP ();

        /* Enable ANSI escape sequence parsing etc. on MS Windows command prompt */

        if (chd != INVALID_HANDLE_VALUE)
        {
            if (!SetConsoleMode (chd,
                                 ENABLE_PROCESSED_OUTPUT
                                 | ENABLE_WRAP_AT_EOL_OUTPUT
                                 | ENABLE_VIRTUAL_TERMINAL_PROCESSING
                                 | DISABLE_NEWLINE_AUTO_RETURN))
                win32_stdout_is_file = TRUE;
        }

        /* Set UTF-8 code page I/O */

        SetConsoleOutputCP (65001);
        SetConsoleCP (65001);
    }
#endif
}

static GString *
convert_image (const void *pixels, gint pix_width, gint pix_height,
               gint pix_rowstride, ChafaPixelType pixel_type,
               gint width_cells, gint height_cells,
               gint cell_width, gint cell_height)
{
    ChafaTermInfo *term_info;
    ChafaCanvasMode mode;
    ChafaPixelMode pixel_mode;
    ChafaSymbolMap *symbol_map;
    ChafaCanvasConfig *config;
    ChafaCanvas *canvas;
    GString *printable;

    detect_terminal (&term_info, &mode, &pixel_mode);

    /* Specify the symbols we want */

    symbol_map = chafa_symbol_map_new ();
    chafa_symbol_map_add_by_tags (symbol_map, CHAFA_SYMBOL_TAG_BLOCK);

    /* Set up a configuration with the symbols and the canvas size in characters */

    config = chafa_canvas_config_new ();
    chafa_canvas_config_set_canvas_mode (config, mode);
    chafa_canvas_config_set_pixel_mode (config, pixel_mode);
    chafa_canvas_config_set_geometry (config, width_cells, height_cells);
    chafa_canvas_config_set_symbol_map (config, symbol_map);

    if (cell_width > 0 && cell_height > 0)
    {
        /* We know the pixel dimensions of each cell. Store it in the config. */

        chafa_canvas_config_set_cell_geometry (config, cell_width, cell_height);
    }

    /* Create canvas */

    canvas = chafa_canvas_new (config);

    /* Draw pixels to the canvas */

    chafa_canvas_draw_all_pixels (canvas,
                                  pixel_type,
                                  pixels,
                                  pix_width,
                                  pix_height,
                                  pix_rowstride);

    /* Build printable string */

    printable = chafa_canvas_print (canvas, term_info);

    /* Clean up and return */

    chafa_canvas_unref (canvas);
    chafa_canvas_config_unref (config);
    chafa_symbol_map_unref (symbol_map);
    chafa_term_info_unref (term_info);

    return printable;
}

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
    TermSize term_size;
    GString *printable;
    gfloat font_ratio = 0.5;
    gint cell_width = -1, cell_height = -1;  /* Size of each character cell, in pixels */
    gint width_cells, height_cells;  /* Size of output image, in cells */

    /* Initialize the tty device if needed */

    tty_init ();

    /* Get the terminal dimensions and determine the output size, preserving
     * aspect ratio */

    get_tty_size (&term_size);

    if (term_size.width_cells > 0
        && term_size.height_cells > 0
        && term_size.width_pixels > 0
        && term_size.height_pixels > 0)
    {
        cell_width = term_size.width_pixels / term_size.width_cells;
        cell_height = term_size.height_pixels / term_size.height_cells;
        font_ratio = (gdouble) cell_width / (gdouble) cell_height;
    }

    width_cells = term_size.width_cells;
    height_cells = term_size.height_cells;

    chafa_calc_canvas_geometry (PIX_WIDTH,
                                PIX_HEIGHT,
                                &width_cells,
                                &height_cells,
                                font_ratio,
                                TRUE,
                                FALSE);

    /* Convert the image to a printable string */

    printable = convert_image (pixels,
                               PIX_WIDTH,
                               PIX_HEIGHT,
                               PIX_WIDTH * N_CHANNELS,
                               CHAFA_PIXEL_RGBA8_UNASSOCIATED,
                               width_cells,
                               height_cells,
                               cell_width,
                               cell_height);

    /* Print the string */

    fwrite (printable->str, sizeof (char), printable->len, stdout);
    fputc ('\n', stdout);

    /* Free resources and exit */

    g_string_free (printable, TRUE);

    return 0;
}
