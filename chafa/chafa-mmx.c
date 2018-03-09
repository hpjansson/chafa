#include "config.h"

#include <mmintrin.h>

#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <glib.h>
#include "chafa/chafa.h"
#include "chafa/chafa-private.h"

void
calc_colors_mmx (const ChafaPixel *pixels, ChafaColor *cols, const guint8 *cov)
{
    __m64 *m64p0 = (__m64 *) pixels;
    __m64 *cols_m64 = (__m64 *) cols;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        __m64 *m64p1;

        m64p1 = cols_m64 + cov [i];
        *m64p1 = _mm_adds_pi16 (*m64p1, m64p0 [i]);
    }

#if 0
    /* Called after outer loop is done */
    _mm_empty ();
#endif
}

void
leave_mmx (void)
{
    _mm_empty ();
}
