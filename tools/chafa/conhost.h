
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
} ConhostRow;
gboolean safe_WriteConsoleA (HANDLE chd, const gchar *data, gsize len);
gboolean safe_WriteConsoleW (HANDLE chd, const gunichar2 *data, gsize len);
gsize canvas_to_conhost(ChafaCanvas * canvas, ConhostRow ** lines);
void write_image_conhost(const ConhostRow * lines, gsize s);
void destroy_lines(ConhostRow * lines, gsize s);
#endif 