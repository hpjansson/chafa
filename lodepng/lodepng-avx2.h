/* This file is not part of upstream LodePNG. It may be distributed under the
   upstream license(s), CC0, or as Public Domain, at your option.

   Any questions should be directed to Hans Petter Jansson <hpj@hpjansson.org>.
*/

#ifndef LODEPNG_AVX2_H
#define LODEPNG_AVX2_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
Unfilter one scanline. Returns 1 if the row was handled, 0 if this combination
of filter type and bytewidth has no AVX2 implementation and the caller should
fall back to the generic code. recon and scanline may overlap. precon cannot
overlap any input or output.
*/
int lodepng_unfilter_scanline_avx2(unsigned char* recon, const unsigned char* scanline,
                                   const unsigned char* precon, size_t bytewidth,
                                   unsigned char filterType, size_t length);

#ifdef __cplusplus
}
#endif

#endif
