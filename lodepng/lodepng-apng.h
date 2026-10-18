/* This file is not part of upstream LodePNG. It may be distributed under the
   upstream license(s), CC0, or as Public Domain, at your option.

   Any questions should be directed to Hans Petter Jansson <hpj@hpjansson.org>.
*/

#ifndef LODEPNG_APNG_H
#define LODEPNG_APNG_H

#include "lodepng.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LodePNGDisposeOp
{
  LODEPNG_DISPOSE_OP_NONE,
  LODEPNG_DISPOSE_OP_BACKGROUND,
  LODEPNG_DISPOSE_OP_PREVIOUS
}
LodePNGDisposeOp;

typedef enum LodePNGBlendOp
{
  LODEPNG_BLEND_OP_SOURCE,
  LODEPNG_BLEND_OP_OVER
}
LodePNGBlendOp;

/* Describes an fcTL chunk */
typedef struct LodePNGFrameControl
{
  unsigned width, height;
  unsigned x_offset, y_offset;
  unsigned delay_num, delay_den; /* Show for delay_num / delay_den seconds; delay_den 0 -> 100 */
  LodePNGDisposeOp dispose_op;
  LodePNGBlendOp blend_op;
}
LodePNGFrameControl;

typedef struct LodePNGAnimFrame LodePNGAnimFrame;

typedef struct LodePNGAnim
{
  /* In: Decoder settings (state.decoder). Out: IHDR, PLTE and tRNS
   * (state.info_png). state.info_raw is not used. */
  LodePNGState state;

  /* Out: set by lodepng_anim_inspect */
  unsigned width, height;
  unsigned num_frames;
  unsigned num_plays;
  unsigned index_error;

  /* Private */
  LodePNGAnimFrame *frames;
  unsigned frames_alloced;
  size_t max_frame_pixels;
  unsigned next_frame;
  unsigned char *canvas;
  unsigned char *saved;
  unsigned char *frame_rgba;
  unsigned char *zbuf;
  size_t zbuf_size;
}
LodePNGAnim;

void lodepng_anim_init (LodePNGAnim *anim);
void lodepng_anim_cleanup (LodePNGAnim *anim);

/* Read the header and index the frames without decompressing anything.
 *
 * Returns an error if the PNG is unusable. A PNG that isn't an APNG succeeds
 * with num_frames 0. */
unsigned lodepng_anim_inspect (LodePNGAnim *anim,
                               const unsigned char *in, size_t in_len);

/* Decode the next frame and draw it onto the canvas. On success, *canvas points
 * to the canvas (width * height * 4 bytes), which stays valid and unchanged
 * until the next call to lodepng_anim_decode_next(), lodepng_anim_rewind() or
 * lodepng_anim_cleanup(). If fctl is not NULL, it receives the frame's fcTL.
 *
 * Returns 1007 after the last frame. On any error the canvas is left as it was. */
unsigned lodepng_anim_decode_next (LodePNGAnim *anim,
                                   const unsigned char **canvas,
                                   LodePNGFrameControl *fctl);

/* Clear the canvas and go back to the first frame. */
void lodepng_anim_rewind (LodePNGAnim *anim);

#ifdef __cplusplus
}
#endif

#endif
