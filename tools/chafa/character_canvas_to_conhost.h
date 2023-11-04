
#ifndef CHARACTER_CANVAS_TO_CONHOST_H
#define CHARACTER_CANVAS_TO_CONHOST_H
#include <glib.h>
#include <chafa.h>
#include <windows.h>

typedef WORD attribute;
typedef struct  {
	gunichar2 * str;
	attribute * attributes;
	size_t length;
	size_t utf16_string_length;
} CONHOST_LINE;
gboolean safe_WriteConsoleA (HANDLE chd, const gchar *data, gsize len);
gboolean safe_WriteConsoleW (HANDLE chd, const gunichar2 *data, gsize len);
gsize canvas_to_conhost(ChafaCanvas * canvas, CONHOST_LINE ** lines);
void write_image_conhost(const CONHOST_LINE * lines, gsize s);
void destroy_lines(CONHOST_LINE * lines, gsize s);
#endif 