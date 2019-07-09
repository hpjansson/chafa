/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright © 2019 Hans Petter Jansson. See COPYING for details. */

#include <stdint.h>
#include "smolscale.h"

#ifndef _SMOLSCALE_PRIVATE_H_
#define _SMOLSCALE_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Enum switches must handle every value */
#ifdef __GNUC__
# pragma GCC diagnostic error "-Wswitch"
#endif

#ifndef FALSE
# define FALSE (0)
#endif
#ifndef TRUE
# define TRUE (!FALSE)
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

typedef unsigned int SmolBool;

#define SMOL_TEMP_ALIGNMENT 64
#define SMOL_ASSUME_ALIGNED(x, t, a) (x) = (t) __builtin_assume_aligned ((x), (a))
#define SMOL_ASSUME_TEMP_ALIGNED(x, t) (x) = (t) __builtin_assume_aligned ((x), SMOL_TEMP_ALIGNMENT)

#define SMOL_UNUSED(x) (void) ((x)=(x))
#define SMOL_RESTRICT __restrict
#define SMOL_INLINE __attribute__((always_inline)) inline
#define SMOL_CONST __attribute__((const))
#define SMOL_PURE __attribute__((pure))
#define SMOL_ALIGNED_4 __attribute__((aligned(4)))
#define SMOL_ALIGNED_8 __attribute__((aligned(8)))
#define SMOL_ALIGNED_16 __attribute__((aligned(16)))
#define SMOL_ALIGNED_32 __attribute__((aligned(32)))
#define SMOL_ALIGNED_64 __attribute__((aligned(64)))

#define SMALL_MUL 256U
#define BIG_MUL 65536U
#define BOXES_MULTIPLIER ((uint64_t) BIG_MUL * SMALL_MUL)
#define BILIN_MULTIPLIER ((uint64_t) BIG_MUL * BIG_MUL)

#define aligned_alloca(s, a) \
  ({ void *p = alloca ((s) + (a)); p = (void *) (((uintptr_t) (p) + (a)) & ~((a) - 1)); (p); })

typedef enum
{
    SMOL_STORAGE_64BPP,
    SMOL_STORAGE_128BPP,
    SMOL_STORAGE_MAX
}
SmolStorageType;

typedef enum
{
    SMOL_FILTER_ONE,
    SMOL_FILTER_BILINEAR_0H,
    SMOL_FILTER_BILINEAR_1H,
    SMOL_FILTER_BILINEAR_2H,
    SMOL_FILTER_BILINEAR_3H,
    SMOL_FILTER_BILINEAR_4H,
    SMOL_FILTER_BILINEAR_5H,
    SMOL_FILTER_BILINEAR_6H,
    SMOL_FILTER_BOX,

    SMOL_FILTER_MAX
}
SmolFilterType;

/* For reusing rows that have already undergone horizontal scaling */
typedef struct
{
    uint32_t in_ofs;
    uint64_t *parts_row [3];
}
SmolVerticalCtx;

typedef void (SmolUnpackRowFunc) (const uint32_t *row_in,
                                  uint64_t *row_out,
                                  uint32_t n_pixels);
typedef void (SmolPackRowFunc) (const uint64_t *row_in,
                                uint32_t *row_out,
                                uint32_t n_pixels);
typedef void (SmolHFilterFunc) (const SmolScaleCtx *scale_ctx,
                                const uint64_t *row_limbs_in,
                                uint64_t *row_limbs_out);
typedef void (SmolVFilterFunc) (const SmolScaleCtx *scale_ctx,
                                SmolVerticalCtx *vertical_ctx,
                                uint32_t outrow_index,
                                uint32_t *row_out);

#define SMOL_CONV_UNDEFINED { 0, NULL, NULL }
#define SMOL_CONV(un_from_order, un_from_type, un_to_order, un_to_type, pk_from_order, pk_from_type, pk_to_order, pk_to_type, storage_bits) \
{ storage_bits / 8, unpack_row_##un_from_order##_##un_from_type##_to_##un_to_order##_##un_to_type##_##storage_bits##bpp, \
pack_row_##pk_from_order##_##pk_from_type##_to_##pk_to_order##_##pk_to_type##_##storage_bits##bpp }

typedef struct
{
    uint8_t n_bytes_per_pixel;
    SmolUnpackRowFunc *unpack_row_func;
    SmolPackRowFunc *pack_row_func;
}
SmolConversion;

typedef struct
{
    SmolConversion conversions [SMOL_STORAGE_MAX] [SMOL_PIXEL_MAX] [SMOL_PIXEL_MAX];
}
SmolConversionTable;

typedef struct
{
    SmolHFilterFunc *hfilter_funcs [SMOL_STORAGE_MAX] [SMOL_FILTER_MAX];
    SmolVFilterFunc *vfilter_funcs [SMOL_STORAGE_MAX] [SMOL_FILTER_MAX];

    /* Can be a NULL pointer if the implementation does not override any
     * conversions. */
    const SmolConversionTable *ctab;
}
SmolImplementation;

struct SmolScaleCtx
{
    /* <private> */

    const uint32_t *pixels_in;
    uint32_t *pixels_out;
    uint32_t width_in, height_in, rowstride_in;
    uint32_t width_out, height_out, rowstride_out;

    SmolPixelType pixel_type_in, pixel_type_out;
    SmolFilterType filter_h, filter_v;
    SmolStorageType storage_type;

    SmolUnpackRowFunc *unpack_row_func;
    SmolPackRowFunc *pack_row_func;
    SmolHFilterFunc *hfilter_func;
    SmolVFilterFunc *vfilter_func;

    /* Each offset is split in two uint16s: { pixel index, fraction }. These
     * are relative to the image after halvings have taken place. */
    uint16_t *offsets_x, *offsets_y;
    uint32_t span_mul_x, span_mul_y;  /* For box filter */

    uint32_t width_bilin_out, height_bilin_out;
    unsigned int width_halvings, height_halvings;
};

#ifdef SMOL_WITH_AVX2
const SmolImplementation *_smol_get_avx2_implementation (void);
#endif

#ifdef __cplusplus
}
#endif

#endif
