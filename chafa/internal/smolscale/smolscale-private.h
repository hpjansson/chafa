/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright © 2019-2025 Hans Petter Jansson. See COPYING for details. */

/* If you're just going to use Smolscale in your project, you don't have to
 * worry about anything in here. The public API and documentation, such as
 * it is, lives in smolscale.h.
 *
 * If, on the other hand, you're here to hack on Smolscale itself, this file
 * contains all the internal shared declarations. */

#undef SMOL_ENABLE_ASSERTS

#include <stdint.h>
#include "smolscale.h"

#ifndef _SMOLSCALE_PRIVATE_H_
#define _SMOLSCALE_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SMOL_ENABLE_ASSERTS
# include <assert.h>
# define SMOL_ASSERT(x) assert (x)
#else
# define SMOL_ASSERT(x)
#endif

/* We'll use at most ~4MB of scratch space. That won't fit on the stack
 * everywhere, so we default to malloc(). If you know better, you can define
 * SMOL_USE_ALLOCA. */
#ifdef SMOL_USE_ALLOCA
# define _SMOL_ALLOC(n) alloca (n)
# define _SMOL_FREE(p)
#else
# define _SMOL_ALLOC(n) malloc (n)
# define _SMOL_FREE(p) free (p)
#endif

/* Enum switches must handle every value */
#ifdef __GNUC__
# pragma GCC diagnostic error "-Wswitch"
#endif

/* Compensate for GCC missing intrinsics */
#ifdef __GNUC__
# if __GNUC__ < 8
#  define _mm256_set_m128i(h, l) \
    _mm256_insertf128_si256 (_mm256_castsi128_si256 (l), (h), 1)
# endif
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

#define SMOL_4X2BIT(a, b, c, d)                                         \
    (((a) << 6) | ((b) << 4) | ((c) << 2) | (d))

#define SMOL_8X1BIT(a,b,c,d,e,f,g,h)                                    \
    (((a) << 7) | ((b) << 6) | ((c) << 5) | ((d) << 4)                  \
     | ((e) << 3) | ((f) << 2) | ((g) << 1) | ((h) << 0))

#define SMOL_UNUSED(x) (void) ((x)=(x))
#define SMOL_RESTRICT __restrict
#define SMOL_INLINE __attribute__((always_inline)) inline
#define SMOL_CONST __attribute__((const))
#define SMOL_PURE __attribute__((pure))

#define SMOL_SMALL_MUL 256U
#define SMOL_BIG_MUL 65536U
#define SMOL_BOXES_MULTIPLIER ((uint64_t) SMOL_BIG_MUL * SMOL_SMALL_MUL)
#define SMOL_BILIN_MULTIPLIER ((uint64_t) SMOL_BIG_MUL * SMOL_BIG_MUL)

#define SMOL_ALIGNMENT 64

#ifdef SMOL_DISABLE_ASSUME_ALIGNED
# define SMOL_ASSIGN_ALIGNED_TO(x, t, n) (t) (x)
#else
# define SMOL_ASSIGN_ALIGNED_TO(x, t, n) (t) __builtin_assume_aligned ((x), (n))
#endif

#define SMOL_ASSIGN_ALIGNED(x, t) SMOL_ASSIGN_ALIGNED_TO ((x), t, SMOL_ALIGNMENT)

#define SMOL_ASSUME_ALIGNED_TO(x, t, n) (x) = SMOL_ASSIGN_ALIGNED_TO ((x), t, (n))
#define SMOL_ASSUME_ALIGNED(x, t) SMOL_ASSUME_ALIGNED_TO ((x), t, SMOL_ALIGNMENT)

/* Pointer to beginning of storage is stored in *r. This must be passed to smol_free() later. */
#define smol_alloc_aligned_to(s, a, r) \
  ({ void *p; *(r) = _SMOL_ALLOC ((s) + (a)); p = (void *) (((uintptr_t) (*(r)) + (a)) & ~((a) - 1)); (p); })
#define smol_alloc_aligned(s, r) smol_alloc_aligned_to ((s), SMOL_ALIGNMENT, (r))
#define smol_free(p) _SMOL_FREE(p)

typedef enum
{
    SMOL_STORAGE_24BPP,
    SMOL_STORAGE_32BPP,
    SMOL_STORAGE_64BPP,
    SMOL_STORAGE_128BPP,

    SMOL_STORAGE_MAX
}
SmolStorageType;

typedef enum
{
    SMOL_FILTER_COPY,
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

typedef enum
{
    SMOL_REORDER_1234_TO_1234,

    SMOL_REORDER_1234_TO_2341,
    SMOL_REORDER_1234_TO_3214,
    SMOL_REORDER_1234_TO_4123,
    SMOL_REORDER_1234_TO_4321,
    SMOL_REORDER_1234_TO_123,
    SMOL_REORDER_1234_TO_321,
    SMOL_REORDER_123_TO_1234,

    SMOL_REORDER_1234_TO_1324,
    SMOL_REORDER_1234_TO_2314,
    SMOL_REORDER_1234_TO_2431,
    SMOL_REORDER_1234_TO_4132,
    SMOL_REORDER_1234_TO_4231,
    SMOL_REORDER_1234_TO_132,
    SMOL_REORDER_1234_TO_231,
    SMOL_REORDER_123_TO_1324,

    SMOL_REORDER_1234_TO_324,
    SMOL_REORDER_1234_TO_423,

    SMOL_REORDER_1234_TO_1423,
    SMOL_REORDER_1234_TO_3241,

    SMOL_REORDER_MAX
}
SmolReorderType;

typedef enum
{
    SMOL_ALPHA_UNASSOCIATED,
    SMOL_ALPHA_PREMUL8,
    SMOL_ALPHA_PREMUL16,

    SMOL_ALPHA_MAX
}
SmolAlphaType;

typedef enum
{
    SMOL_GAMMA_SRGB_COMPRESSED,
    SMOL_GAMMA_SRGB_LINEAR,

    SMOL_GAMMA_MAX
}
SmolGammaType;

typedef struct
{
    unsigned char src [4];
    unsigned char dest [4];
}
SmolReorderMeta;

typedef struct
{
    unsigned char storage;
    unsigned char pixel_stride;
    unsigned char alpha;
    unsigned char order [4];
}
SmolPixelTypeMeta;

/* For reusing rows that have already undergone horizontal scaling */
typedef struct
{
    uint32_t src_ofs;
    uint64_t *parts_row [4];
    uint64_t *row_storage [4];
    uint32_t *src_aligned;
    uint32_t *src_aligned_storage;
}
SmolLocalCtx;

typedef void (SmolInitFunc) (SmolScaleCtx *scale_ctx);
typedef void (SmolRepackRowFunc) (const void *src_row,
                                  void *dest_row,
                                  uint32_t n_pixels);
typedef void (SmolHFilterFunc) (const SmolScaleCtx *scale_ctx,
                                const uint64_t *src_row_limbs,
                                uint64_t *dest_row_limbs);
typedef int (SmolVFilterFunc) (const SmolScaleCtx *scale_ctx,
                               SmolLocalCtx *local_ctx,
                               uint32_t dest_row_index);
typedef void (SmolCompositeOverColorFunc) (uint64_t *srcdest_row,
                                           const uint64_t *color_pixel,
                                           uint32_t n_pixels);
typedef void (SmolCompositeOverDestFunc) (const uint64_t *src_row,
                                          uint64_t *dest_row,
                                          uint32_t n_pixels);
typedef void (SmolClearFunc) (const void *src_pixel_batch,
                              void *dest_row,
                              uint32_t n_pixels);

#define SMOL_REPACK_SIGNATURE_GET_REORDER(sig) ((sig) >> (2 * (SMOL_GAMMA_BITS + SMOL_ALPHA_BITS + SMOL_STORAGE_BITS)))

#define SMOL_REORDER_BITS 6
#define SMOL_STORAGE_BITS 2
#define SMOL_ALPHA_BITS 2
#define SMOL_GAMMA_BITS 1

#define SMOL_MAKE_REPACK_SIGNATURE_ANY_ORDER(src_storage, src_alpha, src_gamma, \
                                             dest_storage, dest_alpha, dest_gamma) \
    (((src_storage) << (SMOL_GAMMA_BITS + SMOL_ALPHA_BITS + SMOL_STORAGE_BITS + SMOL_GAMMA_BITS + SMOL_ALPHA_BITS)) \
     | ((src_alpha) << (SMOL_GAMMA_BITS + SMOL_ALPHA_BITS + SMOL_STORAGE_BITS + SMOL_GAMMA_BITS)) \
     | ((src_gamma) << (SMOL_GAMMA_BITS + SMOL_ALPHA_BITS + SMOL_STORAGE_BITS)) \
     | ((dest_storage) << (SMOL_GAMMA_BITS + SMOL_ALPHA_BITS))           \
     | ((dest_alpha) << (SMOL_GAMMA_BITS))                               \
     | ((dest_gamma) << 0))                                              \

#define MASK_ITEM(m, n_bits) ((m) ? (1 << (n_bits)) - 1 : 0)

#define SMOL_REPACK_SIGNATURE_ANY_ORDER_MASK(src_storage, src_alpha, src_gamma, \
                                             dest_storage, dest_alpha, dest_gamma) \
    SMOL_MAKE_REPACK_SIGNATURE_ANY_ORDER(MASK_ITEM (src_storage, SMOL_STORAGE_BITS), \
                                         MASK_ITEM (src_alpha, SMOL_ALPHA_BITS), \
                                         MASK_ITEM (src_gamma, SMOL_GAMMA_BITS), \
                                         MASK_ITEM (dest_storage, SMOL_STORAGE_BITS), \
                                         MASK_ITEM (dest_alpha, SMOL_ALPHA_BITS), \
                                         MASK_ITEM (dest_gamma, SMOL_GAMMA_BITS))

#define SMOL_REPACK_META(src_order, src_storage, src_alpha, src_gamma,      \
                         dest_order, dest_storage, dest_alpha, dest_gamma)  \
    { (((SMOL_REORDER_##src_order##_TO_##dest_order) << 10)               \
       | ((SMOL_STORAGE_##src_storage##BPP) << 8) | ((SMOL_ALPHA_##src_alpha) << 6) \
       | ((SMOL_GAMMA_SRGB_##src_gamma) << 5)                            \
       | ((SMOL_STORAGE_##dest_storage##BPP) << 3) | ((SMOL_ALPHA_##dest_alpha) << 1) \
       | ((SMOL_GAMMA_SRGB_##dest_gamma) << 0)), \
    (SmolRepackRowFunc *) repack_row_##src_order##_##src_storage##_##src_alpha##_##src_gamma##_to_##dest_order##_##dest_storage##_##dest_alpha##_##dest_gamma }

#define SMOL_REPACK_META_LAST { 0xffff, NULL }

typedef struct
{
    uint16_t signature;
    SmolRepackRowFunc *repack_row_func;
}
SmolRepackMeta;

#define SMOL_REPACK_ROW_DEF(src_order, src_storage, src_limb_bits, src_alpha, src_gamma, \
                            dest_order, dest_storage, dest_limb_bits, dest_alpha, dest_gamma) \
    static void repack_row_##src_order##_##src_storage##_##src_alpha##_##src_gamma##_to_##dest_order##_##dest_storage##_##dest_alpha##_##dest_gamma \
    (const uint##src_limb_bits##_t * SMOL_RESTRICT src_row,               \
     uint##dest_limb_bits##_t * SMOL_RESTRICT dest_row,                   \
     uint32_t n_pixels)                                                 \
    {                                                                   \
        uint##dest_limb_bits##_t *dest_row_max = dest_row + n_pixels * (dest_storage / dest_limb_bits); \
        SMOL_ASSUME_ALIGNED_TO (src_row, const uint##src_limb_bits##_t *, src_limb_bits / 8); \
        SMOL_ASSUME_ALIGNED_TO (dest_row, uint##dest_limb_bits##_t *, dest_limb_bits / 8);

#define SMOL_REPACK_ROW_DEF_END }

typedef struct
{
    SmolInitFunc *init_h_func;
    SmolInitFunc *init_v_func;
    SmolHFilterFunc *hfilter_funcs [SMOL_STORAGE_MAX] [SMOL_FILTER_MAX];
    SmolVFilterFunc *vfilter_funcs [SMOL_STORAGE_MAX] [SMOL_FILTER_MAX];
    SmolCompositeOverColorFunc *composite_over_color_funcs [SMOL_STORAGE_MAX] [SMOL_GAMMA_MAX] [SMOL_ALPHA_MAX];
    SmolCompositeOverDestFunc *composite_over_dest_funcs [SMOL_STORAGE_MAX];
    SmolClearFunc *clear_funcs [SMOL_STORAGE_MAX];
    const SmolRepackMeta *repack_meta;
}
SmolImplementation;

typedef struct
{
    void *precalc;
    SmolFilterType filter_type;

    uint32_t src_size_px, src_size_spx;
    uint32_t dest_size_px, dest_size_spx;

    unsigned int n_halvings;

    int32_t placement_ofs_px, placement_ofs_spx;
    uint32_t placement_size_px, placement_size_spx;
    uint32_t placement_size_prehalving_px, placement_size_prehalving_spx;

    uint32_t span_step;  /* For box filter, in spx */
    uint32_t span_mul;  /* For box filter */

    /* Opacity of first and last column or row. Used for subpixel placement
     * and applied after each scaling step. */
    uint16_t first_opacity, last_opacity;

    /* Rows or cols to add consisting of unbroken pixel_color. This is done
     * after scaling but before conversion to output pixel format. */
    uint16_t clear_before_px, clear_after_px;

    uint16_t clip_before_px, clip_after_px;
}
SmolDim;

#define SMOL_CLEAR_BATCH_SIZE 96

struct SmolScaleCtx
{
    /* <private> */

    const char *src_pixels;
    char *dest_pixels;

    uint32_t src_rowstride;
    uint32_t dest_rowstride;

    SmolPixelType src_pixel_type, dest_pixel_type;
    SmolStorageType storage_type;
    SmolGammaType gamma_type;
    SmolCompositeOp composite_op;

    /* Raw flags passed in by user */
    SmolFlags flags;

    SmolRepackRowFunc *src_unpack_row_func;
    SmolRepackRowFunc *dest_unpack_row_func;
    SmolRepackRowFunc *pack_row_func;
    SmolHFilterFunc *hfilter_func;
    SmolVFilterFunc *vfilter_func;
    SmolCompositeOverColorFunc *composite_over_color_func;
    SmolCompositeOverDestFunc *composite_over_dest_func;
    SmolClearFunc *clear_dest_func;

    /* User specified, can be NULL */
    SmolPostRowFunc *post_row_func;
    void *user_data;

    /* Storage for dimensions' precalc arrays. Single allocation. */
    void *precalc_storage;

    /* Specifics for each dimension */
    SmolDim hdim, vdim;

    /* TRUE if input rows can be copied directly to output. */
    unsigned int is_noop : 1;

    /* TRUE if we have a color_pixel to composite on. */
    unsigned int have_composite_color : 1;

    /* Unpacked color to composite on */
    uint64_t color_pixel [2];

    /* A batch of color pixels in dest storage format. The batch size
     * is in bytes, and chosen as an even multiple of 3, allowing 32 bytes wide
     * operations (e.g. AVX2) to be used to clear packed RGB pixels. */
    unsigned char color_pixels_clear_batch [SMOL_CLEAR_BATCH_SIZE];
};

/* Number of pixels to convert per batch. For some conversions, we perform
 * an alpha test per batch to avoid the expensive premul path when the image
 * data is opaque.
 *
 * FIXME: Unimplemented. */
#define PIXEL_BATCH_SIZE 32

#define SRGB_LINEAR_BITS 11
#define SRGB_LINEAR_MAX (1 << (SRGB_LINEAR_BITS))

extern const uint16_t _smol_from_srgb_lut [256];
extern const uint8_t _smol_to_srgb_lut [SRGB_LINEAR_MAX];

#define INVERTED_DIV_SHIFT_P8 (21 - 8)
#define INVERTED_DIV_SHIFT_P8L (22 - SRGB_LINEAR_BITS)
#define INVERTED_DIV_SHIFT_P16 (24 - 8)
#define INVERTED_DIV_SHIFT_P16L (30 - SRGB_LINEAR_BITS)

extern const uint32_t _smol_inv_div_p8_lut [256];
extern const uint32_t _smol_inv_div_p8l_lut [256];
extern const uint32_t _smol_inv_div_p16_lut [256];
extern const uint32_t _smol_inv_div_p16l_lut [256];

const SmolImplementation *_smol_get_generic_implementation (void);
#ifdef SMOL_WITH_AVX2
const SmolImplementation *_smol_get_avx2_implementation (void);
#endif

#ifdef __cplusplus
}
#endif

#endif
