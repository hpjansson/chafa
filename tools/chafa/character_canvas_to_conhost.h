#include <chafa.h>
#include <windows.h>
gboolean safe_WriteConsoleA (HANDLE chd, const gchar *data, gsize len);
gboolean safe_WriteConsoleW (HANDLE chd, const guintchar2 *data, gsize len);
void print_canvas_conhost(const ChafaCanvas * canvas, gbool is_utf16);