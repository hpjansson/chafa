/* This file is not part of upstream LodePNG. It may be distributed under the
   upstream license(s), CC0, or as Public Domain, at your option.

   Any questions should be directed to Hans Petter Jansson <hpj@hpjansson.org>.
*/

/* Animated PNG decoding. See lodepng-apng.h for the API and the APNG
 * specification for the format:
 *
 * https://wiki.mozilla.org/APNG_Specification */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include "lodepng-apng.h"
#include "lodepng-internal.h"

#define BYTES_PER_PIXEL 4

#define ACTL_LENGTH 8
#define FCTL_LENGTH 26
#define SEQUENCE_NUMBER_LENGTH 4

#define CHUNK_LEN_MAX 0x7fffffff

/* Error codes; see lodepng_error_text (). The first three are used the same
 * way as in decodeGeneric (). There is no global set of defines or enum, as
 * upstream lodepng uses numeric literals. */
#define ERROR_CHUNK_HEADER_PAST_END 30
#define ERROR_CHUNK_TOO_LONG 63
#define ERROR_CHUNK_PAST_END 64
#define ERROR_ALLOC_FAIL 83
#define ERROR_NO_PALETTE 106
#define ERROR_TOO_LARGE 1000
#define ERROR_INVALID_ACTL 1001
#define ERROR_INVALID_FCTL 1002
#define ERROR_INVALID_REGION 1003
#define ERROR_SEQUENCE 1004
#define ERROR_INVALID_FDAT 1005
#define ERROR_NO_DATA 1006
#define ERROR_NO_MORE_FRAMES 1007

struct LodePNGAnimFrame
{
    LodePNGFrameControl fctl;
    unsigned is_default_image : 1;

    /* The first data chunk (IDAT or fdAT) and the chunk following the last
     * one. Other chunks may be interspersed. */
    const unsigned char *data_begin;
    const unsigned char *data_end;

    unsigned n_data_chunks;
    size_t data_size;
};

/* ------- *
 * Helpers *
 * ------- */

static unsigned
read_u32 (const unsigned char *p)
{
    return ((unsigned) p [0] << 24) | ((unsigned) p [1] << 16)
        | ((unsigned) p [2] << 8) | (unsigned) p [3];
}

static unsigned
read_u16 (const unsigned char *p)
{
    return ((unsigned) p [0] << 8) | (unsigned) p [1];
}

/* Checks that the chunk fits in the buffer, and if so, sets *next_chunk_out
 * to the chunk following it. Overflow-safe for any chunk pointer at or past
 * in. */
static unsigned
validate_chunk (const unsigned char *chunk, const unsigned char *in, size_t in_len,
                const unsigned char **next_chunk_out)
{
    size_t chunk_ofs = (size_t) (chunk - in);
    unsigned chunk_len;
    unsigned error;

    if (chunk_ofs > in_len || in_len - chunk_ofs < 12)
    {
        error = ERROR_CHUNK_HEADER_PAST_END;
        goto out;
    }

    chunk_len = lodepng_chunk_length (chunk);
    if (chunk_len > CHUNK_LEN_MAX)
    {
        error = ERROR_CHUNK_TOO_LONG;
        goto out;
    }

    if (in_len - chunk_ofs - 12 < chunk_len)
    {
        error = ERROR_CHUNK_PAST_END;
        goto out;
    }

    *next_chunk_out = chunk + 12 + chunk_len;
    error = 0;

out:
    return error;
}

static void
free_private (LodePNGAnim *anim)
{
    free (anim->frames);
    free (anim->canvas);
    free (anim->saved);
    free (anim->frame_rgba);
    free (anim->zbuf);

    anim->frames = NULL;
    anim->frames_alloced = 0;
    anim->canvas = NULL;
    anim->saved = NULL;
    anim->frame_rgba = NULL;
    anim->zbuf = NULL;
    anim->zbuf_size = 0;
}

/* -------- *
 * Indexing *
 * -------- */

static unsigned
parse_fctl (LodePNGFrameControl *fctl, const unsigned char *data,
            unsigned canvas_width, unsigned canvas_height)
{
    unsigned dispose_op = data [24];
    unsigned blend_op = data [25];
    unsigned error = 0;

    fctl->width = read_u32 (data + 4);
    fctl->height = read_u32 (data + 8);
    fctl->x_offset = read_u32 (data + 12);
    fctl->y_offset = read_u32 (data + 16);
    fctl->delay_num = read_u16 (data + 20);
    fctl->delay_den = read_u16 (data + 22);
    fctl->dispose_op = (LodePNGDisposeOp) dispose_op;
    fctl->blend_op = (LodePNGBlendOp) blend_op;

    if (dispose_op > LODEPNG_DISPOSE_OP_PREVIOUS || blend_op > LODEPNG_BLEND_OP_OVER)
    {
        error = ERROR_INVALID_FCTL;
    }
    else if (fctl->width == 0 || fctl->height == 0
             || fctl->x_offset > canvas_width || fctl->width > canvas_width - fctl->x_offset
             || fctl->y_offset > canvas_height || fctl->height > canvas_height - fctl->y_offset)
    {
        error = ERROR_INVALID_REGION;
    }

    return error;
}

static unsigned
add_frame (LodePNGAnim *anim, const LodePNGAnimFrame *frame)
{
    size_t n_pixels = (size_t) frame->fctl.width * frame->fctl.height;
    unsigned error = ERROR_ALLOC_FAIL;

    /* Grow array? */

    if (anim->num_frames == anim->frames_alloced)
    {
        unsigned n = anim->frames_alloced ? anim->frames_alloced * 2 : 16;
        size_t n_bytes = (size_t) n * sizeof (LodePNGAnimFrame);
        LodePNGAnimFrame *frames;

        if (n < anim->frames_alloced || n_bytes / sizeof (LodePNGAnimFrame) != n)
            goto out;

        frames = realloc (anim->frames, n_bytes);
        if (!frames)
            goto out;

        anim->frames = frames;
        anim->frames_alloced = n;
    }

    /* Append */

    anim->frames [anim->num_frames++] = *frame;
    if (n_pixels > anim->max_frame_pixels)
        anim->max_frame_pixels = n_pixels;

    error = 0;

out:
    return error;
}

static void
add_data_chunk (LodePNGAnimFrame *frame, const unsigned char *chunk,
                const unsigned char *next_chunk, size_t payload_len)
{
    if (!frame->data_begin)
        frame->data_begin = chunk;

    frame->data_end = next_chunk;
    frame->n_data_chunks++;
    frame->data_size += payload_len;
}

/* Walks the chunks after the IHDR and builds the frame index. Returns an
 * error only for problems that make the PNG itself unusable. Problems with
 * the animation end the index early and are recorded in index_error. */
static unsigned
index_frames (LodePNGAnim *anim, const unsigned char *in, size_t in_len)
{
    /* Skip signature and IHDR */
    const unsigned char *chunk = in + 33;
    LodePNGAnimFrame frame;
    LodePNGAnimFrame *cur = NULL;
    unsigned have_actl = 0;
    unsigned seen_idat = 0;
    unsigned num_frames_declared = 0;
    unsigned next_seq = 0;
    unsigned error = 0;

    for (;;)
    {
        const unsigned char *next_chunk;
        const unsigned char *data;
        unsigned len;

        anim->index_error = validate_chunk (chunk, in, in_len, &next_chunk);
        if (anim->index_error)
        {
            /* Truncated. The frame in progress may still be complete. */
            if (cur && cur->n_data_chunks > 0)
                error = add_frame (anim, cur);
            break;
        }

        len = lodepng_chunk_length (chunk);
        data = lodepng_chunk_data_const (chunk);

        if (lodepng_chunk_type_equals (chunk, "IDAT"))
        {
            /* The acTL must come before the image data. Otherwise, this is
             * just a plain still PNG. */
            if (!have_actl)
                break;

            seen_idat = 1;

            if (cur && cur->is_default_image)
                add_data_chunk (cur, chunk, next_chunk, len);
        }
        else if (have_actl && lodepng_chunk_type_equals (chunk, "fdAT"))
        {
            if (!cur || cur->is_default_image || len < SEQUENCE_NUMBER_LENGTH)
            {
                anim->index_error = ERROR_INVALID_FDAT;
                break;
            }

            if (read_u32 (data) != next_seq)
            {
                anim->index_error = ERROR_SEQUENCE;
                break;
            }

            next_seq++;
            add_data_chunk (cur, chunk, next_chunk, len - SEQUENCE_NUMBER_LENGTH);
        }
        else if (have_actl && lodepng_chunk_type_equals (chunk, "fcTL"))
        {
            /* End the previous frame */
            if (cur)
            {
                if (cur->n_data_chunks == 0)
                {
                    anim->index_error = ERROR_NO_DATA;
                    break;
                }

                error = add_frame (anim, cur);
                cur = NULL;
                if (error)
                    break;
            }

            /* Frames beyond the declared count are ignored */
            if (anim->num_frames == num_frames_declared)
                break;

            if (len != FCTL_LENGTH)
            {
                anim->index_error = ERROR_INVALID_FCTL;
                break;
            }

            if (read_u32 (data) != next_seq)
            {
                anim->index_error = ERROR_SEQUENCE;
                break;
            }

            next_seq++;

            memset (&frame, 0, sizeof (frame));
            anim->index_error = parse_fctl (&frame.fctl, data, anim->width, anim->height);
            if (anim->index_error)
                break;

            /* An fcTL ahead of the image data makes the default image the
             * first frame. It must then cover the whole canvas. */
            if (!seen_idat)
            {
                if (frame.fctl.x_offset != 0
                    || frame.fctl.y_offset != 0
                    || frame.fctl.width != anim->width
                    || frame.fctl.height != anim->height)
                {
                    anim->index_error = ERROR_INVALID_REGION;
                    break;
                }

                frame.is_default_image = 1;
            }

            /* There's nothing to go back to before the first frame */
            if (anim->num_frames == 0
                && frame.fctl.dispose_op == LODEPNG_DISPOSE_OP_PREVIOUS)
            {
                frame.fctl.dispose_op = LODEPNG_DISPOSE_OP_BACKGROUND;
            }

            cur = &frame;
        }
        else if (lodepng_chunk_type_equals (chunk, "acTL"))
        {
            /* Only the first acTL counts, and only ahead of the image data */
            if (!have_actl && !seen_idat)
            {
                if (len != ACTL_LENGTH || read_u32 (data) == 0)
                {
                    /* Invalid; treat the file as a plain PNG */
                    anim->index_error = ERROR_INVALID_ACTL;
                    break;
                }

                num_frames_declared = read_u32 (data);
                anim->num_plays = read_u32 (data + 4);
                have_actl = 1;
            }
        }
        else if (lodepng_chunk_type_equals (chunk, "PLTE")
                 || lodepng_chunk_type_equals (chunk, "tRNS"))
        {
            if (!seen_idat)
            {
                error = lodepng_inspect_chunk (&anim->state, (size_t) (chunk - in), in, in_len);
                if (error)
                    break;
            }
        }
        else if (lodepng_chunk_type_equals (chunk, "IEND"))
        {
            if (cur)
            {
                if (cur->n_data_chunks > 0)
                    error = add_frame (anim, cur);
                else
                    anim->index_error = ERROR_NO_DATA;
            }
            break;
        }

        chunk = next_chunk;
    }

    if (!error && anim->num_frames > 0
        && anim->state.info_png.color.colortype == LCT_PALETTE
        && !anim->state.info_png.color.palette)
    {
        error = ERROR_NO_PALETTE;
    }

    return error;
}

/* -------- *
 * Decoding *
 * -------- */

/* Fetches the frame's zlib stream. A single chunk is used in place,
 * multiple chunks are concatenated into zbuf. */
static unsigned
get_frame_data (LodePNGAnim *anim, const LodePNGAnimFrame *frame,
                const unsigned char **zdata_out, size_t *zsize_out)
{
    const char *type = frame->is_default_image ? "IDAT" : "fdAT";
    size_t skip = frame->is_default_image ? 0 : SEQUENCE_NUMBER_LENGTH;
    const unsigned char *zdata = lodepng_chunk_data_const (frame->data_begin) + skip;
    unsigned error = ERROR_ALLOC_FAIL;

    /* If the chunks are all empty, there's nothing to concatenate, and zbuf
     * may be NULL */
    if (frame->n_data_chunks > 1 && frame->data_size > 0)
    {
        const unsigned char *chunk;
        size_t ofs = 0;

        if (anim->zbuf_size < frame->data_size)
        {
            unsigned char *zbuf = realloc (anim->zbuf, frame->data_size);

            if (!zbuf)
                goto out;

            anim->zbuf = zbuf;
            anim->zbuf_size = frame->data_size;
        }

        for (chunk = frame->data_begin;
             chunk < frame->data_end;
             chunk += 12 + lodepng_chunk_length (chunk))
        {
            size_t len;

            if (!lodepng_chunk_type_equals (chunk, type))
                continue;

            len = lodepng_chunk_length (chunk) - skip;
            memcpy (anim->zbuf + ofs, lodepng_chunk_data_const (chunk) + skip, len);
            ofs += len;
        }

        zdata = anim->zbuf;
    }

    *zdata_out = zdata;
    *zsize_out = frame->data_size;
    error = 0;

out:
    return error;
}

static unsigned
decode_frame (LodePNGAnim *anim, const LodePNGAnimFrame *frame,
              unsigned char **decoded_out)
{
    const unsigned char *zdata;
    size_t zsize;
    unsigned error;

    error = get_frame_data (anim, frame, &zdata, &zsize);
    if (!error)
        error = lodepng_decode_image_data (decoded_out, frame->fctl.width, frame->fctl.height,
                                           &anim->state, zdata, zsize);

    return error;
}

/* Converts a decoded frame to RGBA8. *pixels_out is either the frame itself,
 * if it's RGBA8 already, or frame_rgba. */
static unsigned
convert_frame (LodePNGAnim *anim, const LodePNGFrameControl *fctl,
               const unsigned char *decoded, const unsigned char **pixels_out)
{
    const LodePNGColorMode *color = &anim->state.info_png.color;
    const unsigned char *pixels = decoded;
    unsigned error = 0;

    if (color->colortype != LCT_RGBA || color->bitdepth != 8)
    {
        LodePNGColorMode rgba8 = lodepng_color_mode_make (LCT_RGBA, 8);

        if (!anim->frame_rgba)
        {
            anim->frame_rgba = malloc (anim->max_frame_pixels * BYTES_PER_PIXEL);
            if (!anim->frame_rgba)
            {
                error = ERROR_ALLOC_FAIL;
                goto out;
            }
        }

        error = lodepng_convert (anim->frame_rgba, decoded, &rgba8, color,
                                 fctl->width, fctl->height);
        if (error)
            goto out;

        pixels = anim->frame_rgba;
    }

    *pixels_out = pixels;

out:
    return error;
}

/* Returns row y of the frame's region on the canvas */
static unsigned char *
region_row (const LodePNGAnim *anim, const LodePNGFrameControl *fctl, unsigned y)
{
    return anim->canvas
        + ((size_t) (fctl->y_offset + y) * anim->width + fctl->x_offset) * BYTES_PER_PIXEL;
}

static void
clear_region (LodePNGAnim *anim, const LodePNGFrameControl *fctl)
{
    unsigned y;

    for (y = 0; y < fctl->height; y++)
        memset (region_row (anim, fctl, y), 0, (size_t) fctl->width * BYTES_PER_PIXEL);
}

static void
save_region (LodePNGAnim *anim, const LodePNGFrameControl *fctl)
{
    size_t row_len = (size_t) fctl->width * BYTES_PER_PIXEL;
    unsigned y;

    for (y = 0; y < fctl->height; y++)
        memcpy (anim->saved + y * row_len, region_row (anim, fctl, y), row_len);
}

static void
restore_region (LodePNGAnim *anim, const LodePNGFrameControl *fctl)
{
    size_t row_len = (size_t) fctl->width * BYTES_PER_PIXEL;
    unsigned y;

    for (y = 0; y < fctl->height; y++)
        memcpy (region_row (anim, fctl, y), anim->saved + y * row_len, row_len);
}

/* Composites a row of unassociated RGBA8 pixels over another */
static void
blend_over_row (unsigned char *dest, const unsigned char *src, unsigned n_pixels)
{
    unsigned i;

    for (i = 0; i < n_pixels; i++, dest += 4, src += 4)
    {
        unsigned sa = src [3], da = dest [3];
        unsigned u, v, t;

        if (sa == 0)
            continue;

        /* Opaque src or invisible dest */
        if (sa == 255 || da == 0)
        {
            memcpy (dest, src, 4);
            continue;
        }

        /* Everything else done the hard way */
        u = sa * 255;
        v = (255 - sa) * da;
        t = u + v;

        dest [0] = (unsigned char) ((src [0] * u + dest [0] * v + t / 2) / t);
        dest [1] = (unsigned char) ((src [1] * u + dest [1] * v + t / 2) / t);
        dest [2] = (unsigned char) ((src [2] * u + dest [2] * v + t / 2) / t);
        dest [3] = (unsigned char) ((t + 127) / 255);
    }
}

static void
draw_frame (LodePNGAnim *anim, const LodePNGFrameControl *fctl,
            const unsigned char *pixels)
{
    size_t row_len = (size_t) fctl->width * BYTES_PER_PIXEL;
    unsigned y;

    for (y = 0; y < fctl->height; y++)
    {
        unsigned char *dest = region_row (anim, fctl, y);
        const unsigned char *src = pixels + y * row_len;

        if (fctl->blend_op == LODEPNG_BLEND_OP_SOURCE)
            memcpy (dest, src, row_len);
        else
            blend_over_row (dest, src, fctl->width);
    }
}

/* --- *
 * API *
 * --- */

void
lodepng_anim_init (LodePNGAnim *anim)
{
    memset (anim, 0, sizeof (*anim));
    lodepng_state_init (&anim->state);
}

void
lodepng_anim_cleanup (LodePNGAnim *anim)
{
    free_private (anim);
    lodepng_state_cleanup (&anim->state);
}

unsigned
lodepng_anim_inspect (LodePNGAnim *anim, const unsigned char *in, size_t in_len)
{
    unsigned width, height;
    unsigned error;

    free_private (anim);
    anim->width = anim->height = 0;
    anim->num_frames = 0;
    anim->num_plays = 0;
    anim->index_error = 0;
    anim->max_frame_pixels = 0;
    anim->next_frame = 0;

    /* This also resets state.info_png */
    error = lodepng_inspect (&width, &height, &anim->state, in, in_len);
    if (error)
        goto out;

    if (width > LODEPNG_IMAGE_DATA_SIZE_MAX / BYTES_PER_PIXEL / height)
    {
        error = ERROR_TOO_LARGE;
        goto out;
    }

    anim->width = width;
    anim->height = height;

    error = index_frames (anim, in, in_len);

out:
    if (error)
    {
        free_private (anim);
        anim->num_frames = 0;
    }

    return error;
}

unsigned
lodepng_anim_decode_next (LodePNGAnim *anim, const unsigned char **canvas,
                          LodePNGFrameControl *fctl)
{
    const LodePNGAnimFrame *frame;
    unsigned char *decoded = NULL;
    const unsigned char *pixels;
    unsigned error;

    if (anim->next_frame >= anim->num_frames)
    {
        error = ERROR_NO_MORE_FRAMES;
        goto out;
    }

    frame = &anim->frames [anim->next_frame];

    /* Allocate everything up front, so that once we start drawing we
     * can actually finish. */
    if (!anim->canvas)
    {
        anim->canvas = calloc ((size_t) anim->width * anim->height, BYTES_PER_PIXEL);
        if (!anim->canvas)
        {
            error = ERROR_ALLOC_FAIL;
            goto out;
        }
    }

    if (frame->fctl.dispose_op == LODEPNG_DISPOSE_OP_PREVIOUS && !anim->saved)
    {
        anim->saved = malloc (anim->max_frame_pixels * BYTES_PER_PIXEL);
        if (!anim->saved)
        {
            error = ERROR_ALLOC_FAIL;
            goto out;
        }
    }

    error = decode_frame (anim, frame, &decoded);
    if (error)
        goto out;

    error = convert_frame (anim, &frame->fctl, decoded, &pixels);
    if (error)
        goto out;

    /* Dispose of the previous frame */
    if (anim->next_frame > 0)
    {
        const LodePNGFrameControl *prev = &anim->frames [anim->next_frame - 1].fctl;

        if (prev->dispose_op == LODEPNG_DISPOSE_OP_BACKGROUND)
            clear_region (anim, prev);
        else if (prev->dispose_op == LODEPNG_DISPOSE_OP_PREVIOUS)
            restore_region (anim, prev);
    }

    if (frame->fctl.dispose_op == LODEPNG_DISPOSE_OP_PREVIOUS)
        save_region (anim, &frame->fctl);

    draw_frame (anim, &frame->fctl, pixels);
    anim->next_frame++;

    *canvas = anim->canvas;
    if (fctl)
        *fctl = frame->fctl;

out:
    free (decoded);
    return error;
}

void
lodepng_anim_rewind (LodePNGAnim *anim)
{
    anim->next_frame = 0;

    if (anim->canvas)
        memset (anim->canvas, 0, (size_t) anim->width * anim->height * BYTES_PER_PIXEL);
}
