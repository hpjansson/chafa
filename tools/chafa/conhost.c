#include "conhost.h"

static gsize
unichar_to_utf16 (gunichar c, gunichar2 * str)
{
    if (c>=0x110000 ||
        (c<0xe000 && c>=0xd800) ||
        (c%0x10000 >= 0xfffe)) return 0;
    if (c<0x10000){
        *str = (gunichar2)c;
        return 1;
    } else {
        const gunichar temp=c-0x10000;
        str[0] = (temp>>10)+0xd800;
        str[1] = (temp&0x3ff)+0xdc00;
        return 2;
    }
}


#define FOREGROUND_ALL FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE
gsize
canvas_to_conhost (ChafaCanvas * canvas, ConhostRow ** lines)
{
    gint width, height;
    const ChafaCanvasConfig * config;
    ChafaCanvasMode canvas_mode;



    config = chafa_canvas_peek_config (canvas);
    canvas_mode = chafa_canvas_config_get_canvas_mode (config);
    if (
        canvas_mode == CHAFA_CANVAS_MODE_INDEXED_240 ||
        canvas_mode == CHAFA_CANVAS_MODE_INDEXED_256 ||
        canvas_mode == CHAFA_CANVAS_MODE_TRUECOLOR 
    ) return (gsize) -1;
    chafa_canvas_config_get_geometry (config, &width, &height);
    (*lines) = g_malloc (height*sizeof(ConhostRow));
    static const gchar color_lut[16] = {
        0,4,2,6,1,5,3,7,
        8,12,10,14,9,13,11,15
    };
    for (gint y = 0; y<height; y++){
        ConhostRow * const line = (*lines)+y;
        *line=(ConhostRow) {
            .attributes = g_malloc (width*sizeof(attribute)),
            .str = g_malloc (width*sizeof(gunichar2)*2),
            .length = width,
            .utf16_string_length = 0
        };
        
        for (int x=0; x<width; x++){
            gunichar c = chafa_canvas_get_char_at (canvas,x,y);
            gunichar2 utf16_codes[2];
            gsize s = unichar_to_utf16 (c, utf16_codes);
            if (s>0)
                line->str[line->utf16_string_length++] = *utf16_codes;
            if (s==2)
                line->str[line->utf16_string_length++] = utf16_codes[1];

            if (canvas_mode == CHAFA_CANVAS_MODE_FGBG)
                line->attributes[x]=FOREGROUND_ALL;
            else{
                gint fg_out, bg_out;
                chafa_canvas_get_raw_colors_at (canvas,x,y,&fg_out, &bg_out);
                if (canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG) 
                    line->attributes[x] = bg_out?FOREGROUND_ALL:COMMON_LVB_REVERSE_VIDEO|FOREGROUND_ALL;	
                else {
                    fg_out=color_lut[fg_out];
                    bg_out=color_lut[bg_out];
                    line->attributes[x] = (bg_out<<4)|fg_out;
                }
                
            }
        }
        line->str=g_realloc (line->str, line->utf16_string_length*sizeof(gunichar2));
    }
    return height;
}
void
write_image_conhost (const ConhostRow * lines, gsize s)
{
    HANDLE outh;
    COORD curpos;
    DWORD idc;

    outh = GetStdHandle (STD_OUTPUT_HANDLE);
    {
        CONSOLE_SCREEN_BUFFER_INFO bufinfo;
        GetConsoleScreenBufferInfo (outh, &bufinfo);
        curpos = bufinfo.dwCursorPosition;
    }
    
    for (gsize y=0 ;y<s ; y++){
        const ConhostRow line = lines[y];
        WriteConsoleOutputCharacterW (outh, line.str, line.utf16_string_length, curpos,&idc);
        WriteConsoleOutputAttribute (outh, line.attributes, line.length, curpos, &idc);
        curpos.Y++;
    }
    
    /* WriteConsoleOutput family doesn't scroll the screen
     * so we have to make another API call to scroll */

    SetConsoleCursorPosition(outh, curpos);
}

void
destroy_lines (ConhostRow * lines, gsize s)
{
    for (gsize i=0; i<s; i++){
        g_free (lines[i].attributes);
        g_free (lines[i].str);
    }
    g_free (lines);
}

#include <stdio.h>
gboolean win32_stdout_is_file = FALSE;
gboolean
safe_WriteConsoleA (HANDLE chd, const gchar *data, gsize len)
{
    gsize total_written = 0;

    if (chd == INVALID_HANDLE_VALUE)
        return FALSE;

    while (total_written < len)
    {
        DWORD n_written = 0;

        if (win32_stdout_is_file)
        {
            /* WriteFile() and fwrite() seem to work equally well despite various
             * claims that the former does poorly in a UTF-8 environment. The
             * resulting files look good in my tests, but note that catting them
             * out with 'type' introduces lots of artefacts. */
#if 0
            if (!WriteFile (chd, data, len - total_written, &n_written, NULL))
                return FALSE;
#else
            if ((n_written = fwrite (data, 1, len - total_written, stdout)) < 1)
                return FALSE;
#endif
        }
        else
        {
            if (!WriteConsoleA (chd, data, len - total_written, &n_written, NULL))
                return FALSE;
        }

        data += n_written;
        total_written += n_written;
    }

    return TRUE;
}
gboolean
safe_WriteConsoleW (HANDLE chd, const gunichar2 *data, gsize len)
{
    gsize total_written = 0;
    if (chd == INVALID_HANDLE_VALUE)
        return FALSE;

    while (total_written < len)
    {
        DWORD n_written = 0;
        if (win32_stdout_is_file)
        {
            /* WriteFile() and fwrite() seem to work equally well despite various
             * claims that the former does poorly in a UTF-8 environment. The
             * resulting files look good in my tests, but note that catting them
             * out with 'type' introduces lots of artefacts. */
#if 0
            if (!WriteFile (chd, data, (len - total_written)*2, &n_written, NULL))
                return FALSE;
#else
            if ((n_written = fwrite (data, 2, len - total_written, stdout)) < 1)
                return FALSE;
#endif
        }
        else
        {
            if (!WriteConsoleW (chd, data, len - total_written, &n_written, NULL))
                return FALSE;
        }

        data += n_written;
        total_written += n_written;
    }

    return TRUE;
}