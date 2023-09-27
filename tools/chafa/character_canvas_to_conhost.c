#include "character_canvas_to_conhost.h"

static gsize unichar_to_utf16(gunichar c, gunichar2 * str){
	if (c>=0x110000 ||
		(c<0xe000 && c>=0xd800) ||
		(c%0x10000 >= 0xfffe)) return 0;
	if (c<0x10000){
		*str=(gunichar2)c;
		return 1;
	} else {
		const gunichar temp=c-0x10000;
		str[0]=(temp>>10)+0xd800;
		str[1]=(temp&0x3ff)+0xdc00;
		return 2;
	}
}

#define FOREGROUND_ALL FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE
void print_canvas_conhost(const ChafaCanvas * canvas, gbool is_utf16){
	HANDLE outh = GetStdHandle(STD_OUTPUT_HANDLE);
	WORD prev_attribute = -1;
	const ChafaCanvasConfig * config = chafa_canvas_peek_config(canvas);
	const ChafaCanvasMode canvas_mode = chafa_canvas_config_get_canvas_mode(config);
	gint width, height;
	gchar str[4]; 
	bool (* safe_WriteConsole)(HANDLE,const void *, DWORD, LPDWORD, LPVOID);
	gsize (* unichar_to_utf)(gunichar, void *);
	const void * newline;

	if (is_utf16){
		WriteConsoleUTF=safe_WriteConsoleW;
		unichar_to_utf=unichar_to_utf16;
		newline = L"\r\n";
	} else {
		WriteConsoleUTF=safe_WriteConsoleA;
		unichar_to_utf=g_unichar_to_utf8;
		newline = "\r\n";
	}
	chafa_canvas_config_get_geometry(config, &width, &height);
	for (int y=0; y<height; y++){
		for (int x=0; x<width; x++){
			gunichar c=chafa_canvas_get_char_at(canvas,x,y);
			if (canvas_mode != CHAFA_CANVAS_MODE_FGBG){
				WORD cur_attribute;
				gint fg_out, bg_out;
				chafa_canvas_get_raw_colors_at(canvas,x,y,&fg_out, &bg_out);
				if (canvas_mode == CHAFA_CANVAS_MODE_FGBG_BGFG) 
					cur_attribute=bg_out?BACKGROUND_INVERT|FOREGROUND_ALL:FOREGROUND_ALL;	
				else 
					cur_attribute=(bg_out<<4)|fg_out;
				if (cur_attribute==prev_attribute){
					SetConsoleTextAttribute(outh,cur_attribute);
					prev_attribute=cur_attribute;
				}
				
			}

			gint length=unichar_to_utf(c, str);
			WriteConsoleUTF(outh,str,length,NULL, NULL);

		}
		WriteConsoleUTF(outh, newline,2, NULL, NULL);
	}
	SetConsoleTextAttribute(outh,FOREGROUND_ALL);
}