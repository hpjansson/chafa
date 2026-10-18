/* This file is not part of upstream LodePNG. It may be distributed under the
   upstream license(s), CC0, or as Public Domain, at your option.

   Any questions should be directed to Hans Petter Jansson <hpj@hpjansson.org>.
*/

/* Interfaces shared between lodepng.c and the other units in this directory.
 * Not part of the public API. */

#ifndef LODEPNG_INTERNAL_H
#define LODEPNG_INTERNAL_H

#include "lodepng.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Decode the zlib-compressed image data of a w*h image -- the concatenated
 * payload of IDAT chunks, or of an APNG frame's fdAT chunks -- using the color
 * mode and interlace method in state->info_png and the decompression settings
 * in state->decoder.zlibsettings. info_png must already hold the PLTE and tRNS
 * data, if any.
 *
 * *out receives the unfiltered, deinterlaced pixels in the PNG's own color
 * mode, allocated with lodepng_malloc. On error, *out may still be non-NULL and
 * must be freed. */
unsigned lodepng_decode_image_data (unsigned char** out, unsigned w, unsigned h,
                                    const LodePNGState* state,
                                    const unsigned char* zdata, size_t zsize);

#ifdef __cplusplus
}
#endif

#endif
