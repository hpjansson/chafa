/* This file is not part of upstream LodePNG. It may be distributed under the
   upstream license(s), CC0, or as Public Domain, at your option.

   Any questions should be directed to Hans Petter Jansson <hpj@hpjansson.org>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <immintrin.h>
#include <string.h>
#include "lodepng-avx2.h"

#define LOAD_32(p) _mm_cvtsi32_si128 ((int) load_u32 (p))
#define WIDEN_8_TO_16(x) _mm_cvtepu8_epi16 (x)
#define STORE_32(p, x) store_u32 (p, (unsigned) _mm_cvtsi128_si32 (x))

static unsigned
load_u32 (const unsigned char *p)
{
    unsigned v;
    memcpy (&v, p, 4);
    return v;
}

static void
store_u32 (unsigned char *p, unsigned v)
{
    memcpy (p, &v, 4);
}

/* --- Up: recon[i] = scanline[i] + precon[i] --- */

static void
unfilter_up (unsigned char *recon, const unsigned char *scanline,
             const unsigned char *precon, size_t length)
{
    size_t i = 0;

    for ( ; i + 32 <= length; i += 32)
    {
        __m256i s = _mm256_loadu_si256 ((const __m256i *) (scanline + i));
        __m256i p = _mm256_loadu_si256 ((const __m256i *) (precon + i));
        _mm256_storeu_si256 ((__m256i *) (recon + i), _mm256_add_epi8 (s, p));
    }

    for ( ; i + 16 <= length; i += 16)
    {
        __m128i s = _mm_loadu_si128 ((const __m128i *) (scanline + i));
        __m128i p = _mm_loadu_si128 ((const __m128i *) (precon + i));
        _mm_storeu_si128 ((__m128i *) (recon + i), _mm_add_epi8 (s, p));
    }

    for ( ; i != length; ++i)
        recon [i] = (unsigned char) (scanline [i] + precon [i]);
}

/* --- Sub: recon[i] = scanline[i] + recon[i - bytewidth] --- */

static void
unfilter_sub (unsigned char *recon, const unsigned char *scanline,
              size_t bytewidth, size_t length)
{
    __m128i a = _mm_setzero_si128 ();
    size_t i = 0;

    if (bytewidth == 4)
    {
        for ( ; i + 16 <= length; i += 16)
        {
            __m128i x = _mm_loadu_si128 ((const __m128i *) (scanline + i));
            x = _mm_add_epi8 (x, _mm_slli_si128 (x, 4));
            x = _mm_add_epi8 (x, _mm_slli_si128 (x, 8));
            x = _mm_add_epi8 (x, a);
            _mm_storeu_si128 ((__m128i *) (recon + i), x);
            a = _mm_shuffle_epi32 (x, 0xff);
        }
    }
    else  /* bytewidth == 3 */
    {
        const __m128i lastpx = _mm_setr_epi8 (9, 10, 11, 9, 10, 11, 9, 10, 11,
                                              9, 10, 11, -1, -1, -1, -1);

        for ( ; i + 16 <= length; i += 12)
        {
            __m128i x = _mm_loadu_si128 ((const __m128i *) (scanline + i));
            x = _mm_add_epi8 (x, _mm_slli_si128 (x, 3));
            x = _mm_add_epi8 (x, _mm_slli_si128 (x, 6));
            x = _mm_add_epi8 (x, a);
            _mm_storel_epi64 ((__m128i *) (recon + i), x);
            STORE_32 (recon + i + 8, _mm_srli_si128 (x, 8));
            a = _mm_shuffle_epi8 (x, lastpx);
        }
    }

    for ( ; i + 4 <= length; i += bytewidth)
    {
        a = _mm_add_epi8 (LOAD_32 (scanline + i), a);
        STORE_32 (recon + i, a);
    }

    for ( ; i != length; ++i)
        recon [i] = (unsigned char) (scanline [i]
            + (i >= bytewidth ? recon [i - bytewidth] : 0));
}

/* --- Average: recon[i] = scanline[i] + ((recon[i - bytewidth] + precon[i]) >> 1) --- */

static void
unfilter_average (unsigned char *recon, const unsigned char *scanline,
                  const unsigned char *precon, size_t bytewidth, size_t length)
{
    __m128i a = _mm_setzero_si128 ();
    const __m128i lo8 = _mm_set1_epi16 (0xff);
    size_t i = 0;

    for ( ; i + 4 <= length; i += bytewidth)
    {
        __m128i b = WIDEN_8_TO_16 (LOAD_32 (precon + i));
        __m128i s = WIDEN_8_TO_16 (LOAD_32 (scanline + i));
        __m128i avg = _mm_srli_epi16 (_mm_add_epi16 (a, b), 1);
        __m128i d = _mm_and_si128 (_mm_add_epi16 (avg, s), lo8);

        STORE_32 (recon + i, _mm_packus_epi16 (d, d));
        a = d;
    }

    for ( ; i != length; i++)
    {
        recon [i] = (unsigned char) (
            scanline [i]
            + (((i >= bytewidth ? recon [i - bytewidth] : 0)
                + precon [i]) >> 1));
    }
}

/* --- Paeth --- */

/* Must match paethPredictor() in lodepng.c. Used for the epilogue. */
static unsigned char
paeth_predictor (int a, int b, int c)
{
    int d1 = b - c;
    int d2 = a - c;
    int pa = d1 < 0 ? -d1 : d1;
    int pb = d2 < 0 ? -d2 : d2;
    int pc = (d1 + d2) < 0 ? -(d1 + d2) : (d1 + d2);
    int r = (pb < pa) ? b : a;
    int mn = (pa < pb) ? pa : pb;

    return (unsigned char) ((pc < mn) ? c : r);
}

static void
unfilter_paeth (unsigned char *recon, const unsigned char *scanline,
                const unsigned char *precon, size_t bytewidth, size_t length)
{
    __m128i a = _mm_setzero_si128 ();
    __m128i c = _mm_setzero_si128 ();
    const __m128i lo8 = _mm_set1_epi16 (0xff);
    size_t i = 0;

    for ( ; i + 4 <= length; i += bytewidth)
    {
        __m128i b = WIDEN_8_TO_16 (LOAD_32 (precon + i));
        __m128i s = WIDEN_8_TO_16 (LOAD_32 (scanline + i));
        __m128i d1 = _mm_sub_epi16 (b, c);
        __m128i d2 = _mm_sub_epi16 (a, c);
        __m128i pa = _mm_abs_epi16 (d1);
        __m128i pb = _mm_abs_epi16 (d2);
        __m128i pc = _mm_abs_epi16 (_mm_add_epi16 (d1, d2));
        __m128i r = _mm_blendv_epi8 (a, b, _mm_cmplt_epi16 (pb, pa));
        __m128i nearest = _mm_blendv_epi8 (r, c,
            _mm_cmplt_epi16 (pc, _mm_min_epi16 (pa, pb)));
        __m128i d = _mm_and_si128 (_mm_add_epi16 (nearest, s), lo8);

        STORE_32 (recon + i, _mm_packus_epi16 (d, d));
        a = d;
        c = b;
    }

    for ( ; i != length; i++)
    {
        recon [i] = (unsigned char) (
            scanline [i]
            + (i >= bytewidth
               ? paeth_predictor (recon [i - bytewidth],
                                  precon [i],
                                  precon [i - bytewidth])
               : precon [i]));
    }
}

int
lodepng_unfilter_scanline_avx2 (unsigned char *recon, const unsigned char *scanline,
                                const unsigned char *precon, size_t bytewidth,
                                unsigned char filterType, size_t length)
{
    int result = 0;

    if (filterType != 2 && bytewidth != 3 && bytewidth != 4)
        goto out;

    switch (filterType)
    {
        case 1:
            unfilter_sub (recon, scanline, bytewidth, length);
            break;

        case 2:
            if (!precon)
                goto out;
            unfilter_up (recon, scanline, precon, length);
            break;

        case 3:
            if (!precon)
                goto out;
            unfilter_average (recon, scanline, precon, bytewidth, length);
            break;

        case 4:
            if (!precon)
                goto out;
            unfilter_paeth (recon, scanline, precon, bytewidth, length);
            break;

        default:
            goto out;
    }

    result = 1;

out:
    return result;
}
