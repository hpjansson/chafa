/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright © 2019-2025 Hans Petter Jansson. See COPYING for details. */

/* If you're just going to use Smolscale in your project, you don't have to
 * worry about anything in here. The public API and documentation, such as
 * it is, lives in smolscale.h.
 *
 * If, on the other hand, you're here to hack on Smolscale itself, this file
 * contains all the internal shared declarations. */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
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

#ifdef _MSC_VER
# define SMOL_ALIGN __declspec (align (SMOL_ALIGNMENT))
#else
# define SMOL_ALIGN __attribute__((aligned (SMOL_ALIGNMENT)))
#endif

#ifdef SMOL_DISABLE_ASSUME_ALIGNED
# define SMOL_ASSIGN_ALIGNED_TO(x, t, n) (t) (x)
#else
# define SMOL_ASSIGN_ALIGNED_TO(x, t, n) (t) __builtin_assume_aligned ((x), (n))
#endif

#define SMOL_ASSIGN_ALIGNED(x, t) SMOL_ASSIGN_ALIGNED_TO ((x), t, SMOL_ALIGNMENT)

#define SMOL_ASSUME_ALIGNED_TO(x, t, n) (x) = SMOL_ASSIGN_ALIGNED_TO ((x), t, (n))
#define SMOL_ASSUME_ALIGNED(x, t) SMOL_ASSUME_ALIGNED_TO ((x), t, SMOL_ALIGNMENT)

/* Allocates size bytes aligned to at least the requested power-of-two
 * alignment. The pointer to the beginning of the storage is stored in
 * *storage_out; that pointer must be passed to free() later. On
 * allocation failure, both the returned pointer and *storage_out are
 * NULL. */
static inline void *
smol_alloc_aligned_to (size_t size, size_t alignment, void **storage_out)
{
    void *m = malloc (size + alignment - 1);

    *storage_out = m;
    return (void *) (((uintptr_t) m + alignment - 1) & ~((uintptr_t) alignment - 1));
}

static inline void *
smol_alloc_aligned (size_t size, void **storage_out)
{
    return smol_alloc_aligned_to (size, SMOL_ALIGNMENT, storage_out);
}

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
    SMOL_FILTER_BOX,
    SMOL_FILTER_NEAREST,

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

    SMOL_REORDER_1234_TO_3124,
    SMOL_REORDER_123_TO_3214,
    SMOL_REORDER_123_TO_3124,

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

typedef enum
{
    /* Mixed must be zero and test FALSE */
    SMOL_BATCH_MIXED = 0,
    SMOL_BATCH_OPAQUE,
    SMOL_BATCH_TRANSPARENT
}
SmolBatchOpacity;

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
    void *row_storage [4];
    uint32_t *src_aligned;
    void *src_aligned_storage;

    /* Scratch row a compositor writes into; the unpacked destination
     * pixels under the placement rectangle for SMOL_COMPOSITE_SRC_OVER_DEST,
     * or the blended result for the over-color ops. NULL when no compositor
     * runs. */
    uint64_t *dest_parts_row;
    void *dest_parts_storage;
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
/* Composites a scaled source parts row over a solid color, writing src OVER
 * color to dest_row. dest_row may either be equal to src_row (composite in
 * place) or a distinct row that does not overlap it.
 *
 * @opacity is a layer opacity in [0, SMOL_OPACITY_MAX] applied to the
 * source's coverage; 0 yields the pure color. */
typedef void (SmolCompositeOverColorFunc) (const uint64_t *src_row,
                                           uint64_t *dest_row,
                                           const uint64_t *color_pixel,
                                           uint32_t n_pixels,
                                           uint16_t opacity);
/* Composites a scaled source parts row over a destination parts row in
 * place (dest_row receives src OVER dest). @opacity is a layer opacity in
 * [0, SMOL_OPACITY_MAX] applied to the source's coverage. */
typedef void (SmolCompositeOverDestFunc) (const uint64_t *src_row,
                                          uint64_t *dest_row,
                                          uint32_t n_pixels,
                                          uint16_t opacity);
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
    SmolCompositeOverColorFunc *composite_over_color_src_alpha_funcs [SMOL_STORAGE_MAX] [SMOL_GAMMA_MAX] [SMOL_ALPHA_MAX];
    SmolCompositeOverDestFunc *composite_over_dest_funcs [SMOL_STORAGE_MAX] [SMOL_GAMMA_MAX] [SMOL_ALPHA_MAX];
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

    /* placement_size_px and placement_ofs_px describe only the visible part
     * of the placement (its intersection with the destination); the _spx
     * fields and the prehalving sizes keep the virtual (unclipped) geometry.
     * Sampling parameters (filter choice, precision guards, precalc sample
     * positions) derive from the virtual geometry so that rendered content
     * doesn't depend on clipping; only the visible window is precalculated
     * and scaled. */
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
    int32_t clear_before_px, clear_after_px;

    /* Rows or cols of the virtual placement falling outside the destination.
     * clip_before_px can be large (placement offsets are in 32-bit subpixels,
     * so up to ~2^23 px). It must not be stored narrower. */
    int32_t clip_before_px, clip_after_px;
}
SmolDim;

#define SMOL_CLEAR_BATCH_SIZE 96

struct SmolScaleCtx
{
    /* <private> */

    /* Actual start of this SmolScaleCtx' allocation, so we can align within */
    void *self_storage;

    const char *src_pixels;
    char *dest_pixels;

    size_t src_rowstride;
    size_t dest_rowstride;

    SmolPixelType src_pixel_type, dest_pixel_type;
    SmolStorageType storage_type;
    SmolGammaType gamma_type;
    SmolCompositeOp composite_op;

    /* Layer opacity applied to the source when compositing, in
     * [0, SMOL_OPACITY_MAX]. SMOL_OPACITY_MAX is fully opaque. */
    uint16_t composite_opacity;

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
    SMOL_ALIGN unsigned char color_pixels_clear_batch [SMOL_CLEAR_BATCH_SIZE];
};

#define SRGB_LINEAR_BITS 11
#define SRGB_LINEAR_MAX (1 << (SRGB_LINEAR_BITS))

extern const uint16_t _smol_from_srgb_lut [256];
extern const uint8_t _smol_to_srgb_lut [SRGB_LINEAR_MAX + 4];  /* +4: SIMD gather padding */

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

/* --------------------- *
 * Batched opacity tests *
 * --------------------- */

/* Repackers and compositors have the option of processing each row in
 * a series of batches, testing each batch for its opacity class before
 * proceeding with conversions. This can accelerate processing, as opaque
 * or fully transparent stretches are common cases and it's often faster
 * to perform a mask and test than un/premultiplication.
 *
 * Two variants exist: 3-way (opaque, transparent, mixed) and 2-way (opaque,
 * mixed). The latter is used when fully transparent pixels need "mixed"
 * handling, e.g. for preserving their color values; the 3-way will clear
 * them instead.
 *
 * The mixed branch must handle opaque and transparent pixels identically
 * to their corresponding special cases. */

/* Test builds can force every batch onto the mixed path */
#ifdef SMOL_TEST_HOOKS
extern int _smol_disable_opacity_fastpath;
# define SMOL_OPAQUE_TEST(expr) (_smol_disable_opacity_fastpath ? 0 : (expr))
#else
# define SMOL_OPAQUE_TEST(expr) (expr)
#endif

/* Number of pixels per batch. SIMD code assumes this count implicitly,
 * so the define cannot be changed. It corresponds to 128 bytes, 256 bytes
 * and 512 bytes with 32bpp, 64bpp and 128bpp storage respectivelty. */
#define PIXEL_BATCH_SIZE 32

/* The last digit of a repack's destination channel order names the source
 * channel that becomes alpha. In a 32bpp source limb channel 1 is most
 * significant, so channel k occupies bits (4 - k) * 8. */
#define SMOL_32BPP_ALPHA_MASK(dest_alpha_ch) (0xffU << ((4 - (dest_alpha_ch)) * 8))

/* Alpha field within the limb that carries it: the single limb of a 64bpp
 * pixel, or limb 1 of a 128bpp one. PREMUL8 COMPRESSED stores a plain
 * byte; p8l, p16 and p16l all store (alpha << 8) | 0xff. */
#define SMOL_ALPHA_MASK_P8 0x00ffULL
#define SMOL_ALPHA_MASK_INFLATED 0xff00ULL

/* --- Batch opacity classifiers --- */

/* Backends may redefine these to point at optimized impls */
#define SMOL_BATCH_IS_OPAQUE_32BPP(src, mask) \
    smol_batch_is_opaque_32bpp ((src), (mask))
#define SMOL_BATCH_ALPHA_CLASS_32BPP(src, mask) \
    smol_batch_alpha_class_32bpp ((src), (mask))
#define SMOL_BATCH_IS_OPAQUE_128BPP(src, mask) \
    smol_batch_is_opaque_128bpp ((src), (mask))

static SMOL_INLINE SmolBatchOpacity
smol_batch_is_opaque_32bpp (const uint32_t *src, uint32_t alpha_mask)
{
    uint32_t acc [4];
    uint32_t i;

    /* Probe the first pixel */
    if ((src [0] & alpha_mask) != alpha_mask)
        return SMOL_BATCH_MIXED;

    acc [0] = acc [1] = acc [2] = acc [3] = ~(uint32_t) 0;

    for (i = 0; i < PIXEL_BATCH_SIZE; i += 4)
    {
        /* Four accumulators reduce dependencies */
        acc [0] &= src [i];
        acc [1] &= src [i + 1];
        acc [2] &= src [i + 2];
        acc [3] &= src [i + 3];
    }

    return ((((acc [0] & acc [1]) & (acc [2] & acc [3])) & alpha_mask)
            == alpha_mask) ? SMOL_BATCH_OPAQUE : SMOL_BATCH_MIXED;
}

static SMOL_INLINE SmolBatchOpacity
smol_batch_alpha_class_32bpp (const uint32_t *src, uint32_t alpha_mask)
{
    uint32_t acc [4];
    uint32_t a;
    uint32_t i;

    /* The first pixel selects which reduction to run */
    a = src [0] & alpha_mask;

    if (a == alpha_mask)
    {
        acc [0] = acc [1] = acc [2] = acc [3] = ~(uint32_t) 0;

        for (i = 0; i < PIXEL_BATCH_SIZE; i += 4)
        {
            acc [0] &= src [i];
            acc [1] &= src [i + 1];
            acc [2] &= src [i + 2];
            acc [3] &= src [i + 3];
        }

        return ((((acc [0] & acc [1]) & (acc [2] & acc [3])) & alpha_mask)
                == alpha_mask) ? SMOL_BATCH_OPAQUE : SMOL_BATCH_MIXED;
    }

    if (a == 0)
    {
        acc [0] = acc [1] = acc [2] = acc [3] = 0;

        for (i = 0; i < PIXEL_BATCH_SIZE; i += 4)
        {
            acc [0] |= src [i];
            acc [1] |= src [i + 1];
            acc [2] |= src [i + 2];
            acc [3] |= src [i + 3];
        }

        return ((((acc [0] | acc [1]) | (acc [2] | acc [3])) & alpha_mask)
                == 0) ? SMOL_BATCH_TRANSPARENT : SMOL_BATCH_MIXED;
    }

    return SMOL_BATCH_MIXED;
}

static SMOL_INLINE SmolBatchOpacity
smol_batch_alpha_class_128bpp (const uint64_t *src, uint64_t alpha_mask)
{
    uint64_t acc [4];
    uint64_t a;
    uint32_t i;

    /* The first pixel selects the reduction to run */
    a = src [1] & alpha_mask;

    if (a == alpha_mask)
    {
        acc [0] = acc [1] = acc [2] = acc [3] = ~(uint64_t) 0;

        for (i = 0; i < PIXEL_BATCH_SIZE; i += 4)
        {
            acc [0] &= src [i * 2 + 1];
            acc [1] &= src [i * 2 + 3];
            acc [2] &= src [i * 2 + 5];
            acc [3] &= src [i * 2 + 7];
        }

        return ((((acc [0] & acc [1]) & (acc [2] & acc [3])) & alpha_mask)
                == alpha_mask) ? SMOL_BATCH_OPAQUE : SMOL_BATCH_MIXED;
    }

    if (a == 0)
    {
        acc [0] = acc [1] = acc [2] = acc [3] = 0;

        for (i = 0; i < PIXEL_BATCH_SIZE; i += 4)
        {
            acc [0] |= src [i * 2 + 1];
            acc [1] |= src [i * 2 + 3];
            acc [2] |= src [i * 2 + 5];
            acc [3] |= src [i * 2 + 7];
        }

        return ((((acc [0] | acc [1]) | (acc [2] | acc [3])) & alpha_mask)
                == 0) ? SMOL_BATCH_TRANSPARENT : SMOL_BATCH_MIXED;
    }

    return SMOL_BATCH_MIXED;
}

static SMOL_INLINE SmolBatchOpacity
smol_batch_alpha_class_64bpp (const uint64_t *src)
{
    uint64_t acc [4];
    uint64_t a;
    uint32_t i;

    /* The first pixel selects the reduction to run */
    a = src [0] & SMOL_ALPHA_MASK_P8;

    if (a == SMOL_ALPHA_MASK_P8)
    {
        acc [0] = acc [1] = acc [2] = acc [3] = ~(uint64_t) 0;

        for (i = 0; i < PIXEL_BATCH_SIZE; i += 4)
        {
            acc [0] &= src [i];
            acc [1] &= src [i + 1];
            acc [2] &= src [i + 2];
            acc [3] &= src [i + 3];
        }

        return ((((acc [0] & acc [1]) & (acc [2] & acc [3])) & SMOL_ALPHA_MASK_P8)
                == SMOL_ALPHA_MASK_P8) ? SMOL_BATCH_OPAQUE : SMOL_BATCH_MIXED;
    }

    if (a == 0)
    {
        acc [0] = acc [1] = acc [2] = acc [3] = 0;

        for (i = 0; i < PIXEL_BATCH_SIZE; i += 4)
        {
            acc [0] |= src [i];
            acc [1] |= src [i + 1];
            acc [2] |= src [i + 2];
            acc [3] |= src [i + 3];
        }

        return ((((acc [0] | acc [1]) | (acc [2] | acc [3])) & SMOL_ALPHA_MASK_P8)
                == 0) ? SMOL_BATCH_TRANSPARENT : SMOL_BATCH_MIXED;
    }

    return SMOL_BATCH_MIXED;
}

static SMOL_INLINE SmolBatchOpacity
smol_batch_is_opaque_128bpp (const uint64_t *src, uint64_t alpha_mask)
{
    uint64_t acc [4];
    uint32_t i;

    /* Probe the first pixel */
    if ((src [1] & alpha_mask) != alpha_mask)
        return FALSE;

    acc [0] = acc [1] = acc [2] = acc [3] = ~(uint64_t) 0;

    for (i = 0; i < PIXEL_BATCH_SIZE; i += 4)
    {
        acc [0] &= src [i * 2 + 1];
        acc [1] &= src [i * 2 + 3];
        acc [2] &= src [i * 2 + 5];
        acc [3] &= src [i * 2 + 7];
    }

    return ((((acc [0] & acc [1]) & (acc [2] & acc [3])) & alpha_mask)
            == alpha_mask) ? SMOL_BATCH_OPAQUE : SMOL_BATCH_MIXED;
}

/* --- Batch repack helpers --- */

/* Driver for the repack row loops. Chunks the row into PIXEL_BATCH_SIZE
 * pixels, classifying and running the body on each batch. The body sees n
 * pixels at src_row and dest_row and indexes them with i; it must not advance
 * either pointer. */
#define SMOL_REPACK_BATCH_LOOP(src_advance, dest_px_limbs, opaque_test, ...) \
    do { \
        uint32_t n_left = (uint32_t) ((dest_row_max - dest_row) \
                                      / (dest_px_limbs)); \
        while (n_left >= PIXEL_BATCH_SIZE) \
        { \
            const uint32_t n = PIXEL_BATCH_SIZE; \
            SmolBatchOpacity batch_opacity = SMOL_OPAQUE_TEST (opaque_test); \
            uint32_t i = 0; \
            __VA_ARGS__; \
            (void) i; \
            src_row += (uint32_t) PIXEL_BATCH_SIZE * (src_advance); \
            dest_row += (uint32_t) PIXEL_BATCH_SIZE * (dest_px_limbs); \
            n_left -= PIXEL_BATCH_SIZE; \
        } \
        if (n_left) \
        { \
            /* Epilogue; not worth classifying */ \
            const uint32_t n = n_left; \
            int batch_opacity = 0; \
            uint32_t i = 0; \
            __VA_ARGS__; \
            (void) i; \
            (void) batch_opacity; \
        } \
    } while (0)

#define SMOL_REPACK_BATCHED_2WAY(src_advance, dest_px_limbs, opaque_expr, \
                                 opaque_stmt, general_stmt) \
    SMOL_REPACK_BATCH_LOOP ( \
        src_advance, dest_px_limbs, opaque_expr, \
        if (batch_opacity == SMOL_BATCH_OPAQUE) \
            do { opaque_stmt; } while (0); \
        else \
            do { general_stmt; } while (0))

#define SMOL_REPACK_BATCHED_3WAY(src_advance, dest_px_limbs, class_expr, \
                                 clear_bytes, opaque_stmt, general_stmt) \
    SMOL_REPACK_BATCH_LOOP ( \
        src_advance, dest_px_limbs, class_expr, \
        if (batch_opacity == SMOL_BATCH_OPAQUE) \
            do { opaque_stmt; } while (0); \
        else if (batch_opacity == SMOL_BATCH_TRANSPARENT) \
            memset (dest_row, 0, clear_bytes); \
        else \
            do { general_stmt; } while (0))

#define SMOL_UNPACK_32BPP_TO_64BPP_BATCHED(pixel_func, alpha_ch) \
    SMOL_REPACK_BATCHED_3WAY ( \
        1, 1, SMOL_BATCH_ALPHA_CLASS_32BPP (src_row, \
                                            SMOL_32BPP_ALPHA_MASK (alpha_ch)), \
        n * sizeof (uint64_t), \
        for (i = 0; i < n; i++) \
            dest_row [i] = pixel_func (src_row [i], TRUE), \
        for (i = 0; i < n; i++) \
            dest_row [i] = pixel_func (src_row [i], FALSE))

/* Used by unassoc-to-unassoc paths where color is preserved in transparent pixels */
#define SMOL_UNPACK_32BPP_TO_128BPP_BATCHED(pixel_func, alpha_ch) \
    SMOL_REPACK_BATCHED_2WAY ( \
        1, 2, SMOL_BATCH_IS_OPAQUE_32BPP (src_row, \
                                          SMOL_32BPP_ALPHA_MASK (alpha_ch)), \
     for (i = 0; i < n; i++) \
         pixel_func (src_row [i], dest_row + i * 2, TRUE), \
     for (i = 0; i < n; i++) \
         pixel_func (src_row [i], dest_row + i * 2, FALSE))

/* Same, but for premul destinations where transparent color gets wiped */
#define SMOL_UNPACK_32BPP_TO_P8_128BPP_BATCHED(pixel_func, alpha_ch) \
    SMOL_REPACK_BATCHED_3WAY ( \
        1, 2, SMOL_BATCH_ALPHA_CLASS_32BPP (src_row, \
                                            SMOL_32BPP_ALPHA_MASK (alpha_ch)), \
        (size_t) n * 2 * sizeof (uint64_t), \
        for (i = 0; i < n; i++) \
            pixel_func (src_row [i], dest_row + i * 2, TRUE), \
        for (i = 0; i < n; i++) \
            pixel_func (src_row [i], dest_row + i * 2, FALSE))

#define SMOL_PACK_64BPP_TO_32BPP_BATCHED(pixel_func) \
    SMOL_REPACK_BATCHED_3WAY ( \
        1, 1, smol_batch_alpha_class_64bpp (src_row), \
        n * sizeof (uint32_t), \
        for (i = 0; i < n; i++) \
            dest_row [i] = pixel_func (src_row [i], TRUE), \
        for (i = 0; i < n; i++) \
            dest_row [i] = pixel_func (src_row [i], FALSE))

#define SMOL_PACK_128BPP_TO_32BPP_BATCHED(pixel_func, alpha_mask) \
    SMOL_REPACK_BATCHED_3WAY ( \
        2, 1, smol_batch_alpha_class_128bpp (src_row, alpha_mask), \
        n * sizeof (uint32_t), \
        for (i = 0; i < n; i++) \
            dest_row [i] = pixel_func (src_row + i * 2, TRUE), \
        for (i = 0; i < n; i++) \
            dest_row [i] = pixel_func (src_row + i * 2, FALSE))

#define SMOL_PACK_128BPP_TO_24BPP_BATCHED(pixel_func, alpha_mask) \
    SMOL_REPACK_BATCHED_3WAY ( \
        2, 3, smol_batch_alpha_class_128bpp (src_row, alpha_mask), \
        n * 3, \
        for (i = 0; i < n; i++) \
            pixel_func (src_row + i * 2, dest_row + i * 3, TRUE), \
        for (i = 0; i < n; i++) \
            pixel_func (src_row + i * 2, dest_row + i * 3, FALSE))

#ifdef __cplusplus
}
#endif

#endif
