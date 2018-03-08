#include "config.h"

#include <smmintrin.h>

#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <glib.h>
#include "chafa/chafa.h"
#include "chafa/chafa-private.h"

gint
calc_error_sse41 (const ChafaPixel *pixels, const ChafaColor *cols, const guint8 *cov)
{
    __m64 *m64p0 = (__m64 *) pixels;
    __m64 *m64p1 = (__m64 *) cols;
    __m128i err4 = { 0 };
    const gint32 *e = (gint32 *) &err4;
    gint i;

    for (i = 0; i < CHAFA_SYMBOL_N_PIXELS; i++)
    {
        __m128i t0, t1, t;

        t0 = _mm_cvtepi16_epi32 (_mm_loadl_epi64 ((__m128i *) &m64p0 [i]));
        t1 = _mm_cvtepi16_epi32 (_mm_loadl_epi64 ((__m128i *) &m64p1 [cov [i]]));

        t = t0 - t1;
        t = _mm_mullo_epi32 (t, t);
        err4 += t;
    }

    return e [0] + e [1] + e [2];
}
