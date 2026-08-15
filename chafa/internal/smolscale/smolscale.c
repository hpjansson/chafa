/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright © 2019-2025 Hans Petter Jansson. See COPYING for details. */

#include <assert.h> /* assert */
#include <stdlib.h> /* malloc, free */
#include <string.h> /* memset */
#include <limits.h>
#include "smolscale-private.h"

/* ---------------------- *
 * Configurable constants *
 * ---------------------- */

/* The box algorithms are only sufficiently precise when
 * src_dim > dest_dim * 5, and box_64bpp only starts outperforming
 * bilinear+halving at much higher ratios (with SIMD). We hand off
 * bilinear at a non-^2 factor for continuity; an exact area average
 * over an R-pixel span at fractional subpixel phase needs R+1 taps,
 * while the 2H kernel's support is R*3/4 + 2, so it falls short by
 * R/4 - 1 pixels. The missing fraction shows up as phase-dependent
 * shimmer under subpixel placement and motion, worst near the top of
 * the band (approaching a full pixel at 8x, where a Nyquist grating
 * can swing the output by 180/255). Box carries fractional span ends
 * natively, making it both phase-exact and position-continuous, so it
 * takes over where the tent kernel's tap budget runs out.
 *
 * The cutoff cannot exceed 16, as we don't support bilinear filters
 * with more than three halvings (8x2 taps). */

#ifndef SMOL_BILIN_BOX_CUTOFF
# define SMOL_BILIN_BOX_CUTOFF 14
#endif

/* ----------------------- *
 * Misc. conversion tables *
 * ----------------------- */

/* Table of channel reorderings. Each entry describes an available shuffle
 * implementation indexed by its SmolReorderType. Channel indexes are 1-based.
 * A zero index denotes that the channel is not present (e.g. 3-channel RGB).
 *
 * Keep in sync with the private SmolReorderType enum. */
static const SmolReorderMeta reorder_meta [SMOL_REORDER_MAX] =
{
    { { 1, 2, 3, 4 }, { 1, 2, 3, 4 } },

    { { 1, 2, 3, 4 }, { 2, 3, 4, 1 } },
    { { 1, 2, 3, 4 }, { 3, 2, 1, 4 } },
    { { 1, 2, 3, 4 }, { 4, 1, 2, 3 } },
    { { 1, 2, 3, 4 }, { 4, 3, 2, 1 } },
    { { 1, 2, 3, 4 }, { 1, 2, 3, 0 } },
    { { 1, 2, 3, 4 }, { 3, 2, 1, 0 } },
    { { 1, 2, 3, 0 }, { 1, 2, 3, 4 } },

    { { 1, 2, 3, 4 }, { 1, 3, 2, 4 } },
    { { 1, 2, 3, 4 }, { 2, 3, 1, 4 } },
    { { 1, 2, 3, 4 }, { 2, 4, 3, 1 } },
    { { 1, 2, 3, 4 }, { 4, 1, 3, 2 } },
    { { 1, 2, 3, 4 }, { 4, 2, 3, 1 } },
    { { 1, 2, 3, 4 }, { 1, 3, 2, 0 } },
    { { 1, 2, 3, 4 }, { 2, 3, 1, 0 } },
    { { 1, 2, 3, 0 }, { 1, 3, 2, 4 } },

    { { 1, 2, 3, 4 }, { 3, 2, 4, 0 } },
    { { 1, 2, 3, 4 }, { 4, 2, 3, 0 } },

    { { 1, 2, 3, 4 }, { 1, 4, 2, 3 } },
    { { 1, 2, 3, 4 }, { 3, 2, 4, 1 } },

    { { 1, 2, 3, 4 }, { 3, 1, 2, 4 } },
    { { 1, 2, 3, 0 }, { 3, 2, 1, 4 } },
    { { 1, 2, 3, 0 }, { 3, 1, 2, 4 } }
};

/* Metadata for each pixel type. Storage type, number of channels, alpha type,
 * channel ordering. Channel indexes are 1-based, and 4 is always alpha. A
 * zero index denotes that the channel is not present.
 *
 * RGBA = 1, 2, 3, 4.
 *
 * Keep in sync with the public SmolPixelType enum. */
static const SmolPixelTypeMeta pixel_type_meta [SMOL_PIXEL_MAX] =
{
    { SMOL_STORAGE_32BPP, 4, SMOL_ALPHA_PREMUL8,      { 1, 2, 3, 4 } },
    { SMOL_STORAGE_32BPP, 4, SMOL_ALPHA_PREMUL8,      { 3, 2, 1, 4 } },
    { SMOL_STORAGE_32BPP, 4, SMOL_ALPHA_PREMUL8,      { 4, 1, 2, 3 } },
    { SMOL_STORAGE_32BPP, 4, SMOL_ALPHA_PREMUL8,      { 4, 3, 2, 1 } },
    { SMOL_STORAGE_32BPP, 4, SMOL_ALPHA_UNASSOCIATED, { 1, 2, 3, 4 } },
    { SMOL_STORAGE_32BPP, 4, SMOL_ALPHA_UNASSOCIATED, { 3, 2, 1, 4 } },
    { SMOL_STORAGE_32BPP, 4, SMOL_ALPHA_UNASSOCIATED, { 4, 1, 2, 3 } },
    { SMOL_STORAGE_32BPP, 4, SMOL_ALPHA_UNASSOCIATED, { 4, 3, 2, 1 } },
    { SMOL_STORAGE_24BPP, 3, SMOL_ALPHA_PREMUL8,      { 1, 2, 3, 0 } },
    { SMOL_STORAGE_24BPP, 3, SMOL_ALPHA_PREMUL8,      { 3, 2, 1, 0 } }
};

/* Channel ordering corrected for little endian. Only applies when fetching
 * entire pixels as dwords (i.e. u32), so 3-byte variants don't require any
 * correction.
 *
 * Keep in sync with the public SmolPixelType enum. */
static const SmolPixelType pixel_type_u32_le [SMOL_PIXEL_MAX] =
{
    SMOL_PIXEL_ABGR8_PREMULTIPLIED,
    SMOL_PIXEL_ARGB8_PREMULTIPLIED,
    SMOL_PIXEL_BGRA8_PREMULTIPLIED,
    SMOL_PIXEL_RGBA8_PREMULTIPLIED,
    SMOL_PIXEL_ABGR8_UNASSOCIATED,
    SMOL_PIXEL_ARGB8_UNASSOCIATED,
    SMOL_PIXEL_BGRA8_UNASSOCIATED,
    SMOL_PIXEL_RGBA8_UNASSOCIATED,
    SMOL_PIXEL_RGB8,
    SMOL_PIXEL_BGR8
};

/* ----------------------------------- *
 * sRGB/linear conversion: Shared code *
 * ----------------------------------- */

/* These tables are manually tweaked to be reversible without information
 * loss; _smol_to_srgb_lut [_smol_from_srgb_lut [i]] == i.
 *
 * As a side effect, the values are off true sRGB by < 4.5% (gamma 2.0 vs 2.4). */

const uint16_t _smol_from_srgb_lut [256] =
{
       0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11, 
      12,   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,   23, 
      24,   25,   26,   27,   28,   29,   30,   31,   32,   34,   36,   38, 
      41,   43,   45,   48,   50,   53,   55,   58,   61,   63,   66,   69, 
      72,   75,   78,   81,   85,   88,   91,   95,   98,  102,  105,  109, 
     113,  116,  120,  124,  129,  133,  137,  141,  146,  150,  154,  159, 
     163,  168,  172,  177,  182,  186,  191,  196,  201,  206,  211,  216, 
     222,  227,  232,  238,  243,  249,  254,  261,  267,  272,  278,  284, 
     290,  296,  302,  308,  315,  321,  327,  334,  340,  347,  353,  360, 
     367,  373,  380,  388,  395,  402,  409,  416,  424,  431,  438,  446, 
     453,  461,  468,  476,  484,  491,  499,  507,  516,  524,  532,  540, 
     549,  557,  565,  574,  582,  591,  599,  608,  617,  625,  634,  643, 
     653,  662,  671,  680,  690,  699,  708,  718,  727,  737,  746,  756, 
     766,  776,  786,  796,  806,  816,  826,  836,  847,  857,  867,  878, 
     888,  899,  910,  921,  932,  942,  953,  964,  975,  986,  997, 1008, 
    1020, 1032, 1043, 1055, 1066, 1078, 1089, 1101, 1113, 1124, 1136, 1148, 
    1161, 1173, 1185, 1197, 1210, 1222, 1234, 1247, 1259, 1272, 1284, 1298, 
    1311, 1323, 1336, 1349, 1362, 1375, 1388, 1401, 1415, 1429, 1442, 1456, 
    1469, 1483, 1496, 1510, 1524, 1537, 1552, 1566, 1580, 1594, 1608, 1622, 
    1637, 1651, 1665, 1681, 1695, 1710, 1724, 1739, 1754, 1768, 1783, 1798, 
    1814, 1829, 1844, 1859, 1875, 1890, 1905, 1921, 1937, 1953, 1968, 1984, 
    2000, 2015, 2031, 2047, 
};

/* Four bytes of padding so 32-bit vector gathers can read the last
 * entries safely. */
const uint8_t _smol_to_srgb_lut [SRGB_LINEAR_MAX + 4] =
{
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13, 
     14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27, 
     28,  29,  30,  31,  32,  32,  33,  33,  34,  34,  35,  35,  36,  36, 
     37,  37,  37,  38,  38,  39,  39,  39,  40,  40,  41,  41,  41,  42, 
     42,  43,  43,  43,  44,  44,  44,  45,  45,  45,  46,  46,  46,  47, 
     47,  47,  48,  48,  48,  49,  49,  49,  50,  50,  50,  51,  51,  51, 
     52,  52,  52,  53,  53,  53,  53,  54,  54,  54,  55,  55,  55,  56, 
     56,  56,  56,  57,  57,  57,  57,  58,  58,  58,  59,  59,  59,  59, 
     60,  60,  60,  60,  61,  61,  61,  61,  62,  62,  62,  63,  63,  63, 
     63,  64,  64,  64,  64,  65,  65,  65,  65,  65,  66,  66,  66,  66, 
     67,  67,  67,  67,  68,  68,  68,  68,  69,  69,  69,  69,  69,  70, 
     70,  70,  70,  71,  71,  71,  71,  72,  72,  72,  72,  72,  73,  73, 
     73,  73,  73,  74,  74,  74,  74,  75,  75,  75,  75,  75,  76,  76, 
     76,  76,  76,  77,  77,  77,  77,  77,  78,  78,  78,  78,  79,  79, 
     79,  79,  79,  80,  80,  80,  80,  80,  81,  81,  81,  81,  81,  81, 
     82,  82,  82,  82,  82,  83,  83,  83,  83,  83,  84,  84,  84,  84, 
     84,  85,  85,  85,  85,  85,  85,  86,  86,  86,  86,  86,  87,  87, 
     87,  87,  87,  87,  88,  88,  88,  88,  88,  89,  89,  89,  89,  89, 
     89,  90,  90,  90,  90,  90,  91,  91,  91,  91,  91,  91,  92,  92, 
     92,  92,  92,  92,  93,  93,  93,  93,  93,  93,  94,  94,  94,  94, 
     94,  94,  95,  95,  95,  95,  95,  95,  96,  96,  96,  96,  96,  96, 
     97,  97,  97,  97,  97,  97,  98,  98,  98,  98,  98,  98,  99,  99, 
     99,  99,  99,  99, 100, 100, 100, 100, 100, 100, 101, 101, 101, 101, 
    101, 101, 101, 102, 102, 102, 102, 102, 102, 103, 103, 103, 103, 103, 
    103, 103, 104, 104, 104, 104, 104, 104, 105, 105, 105, 105, 105, 105, 
    105, 106, 106, 106, 106, 106, 106, 106, 107, 107, 107, 107, 107, 107, 
    108, 108, 108, 108, 108, 108, 108, 109, 109, 109, 109, 109, 109, 109, 
    110, 110, 110, 110, 110, 110, 110, 111, 111, 111, 111, 111, 111, 111, 
    112, 112, 112, 112, 112, 112, 112, 113, 113, 113, 113, 113, 113, 113, 
    114, 114, 114, 114, 114, 114, 114, 115, 115, 115, 115, 115, 115, 115, 
    116, 116, 116, 116, 116, 116, 116, 116, 117, 117, 117, 117, 117, 117, 
    117, 118, 118, 118, 118, 118, 118, 118, 118, 119, 119, 119, 119, 119, 
    119, 119, 120, 120, 120, 120, 120, 120, 120, 120, 121, 121, 121, 121, 
    121, 121, 121, 122, 122, 122, 122, 122, 122, 122, 122, 123, 123, 123, 
    123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 125, 125, 
    125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 
    127, 127, 127, 127, 127, 127, 127, 127, 128, 128, 128, 128, 128, 128, 
    128, 128, 129, 129, 129, 129, 129, 129, 129, 129, 130, 130, 130, 130, 
    130, 130, 130, 130, 130, 131, 131, 131, 131, 131, 131, 131, 131, 132, 
    132, 132, 132, 132, 132, 132, 132, 133, 133, 133, 133, 133, 133, 133, 
    133, 133, 134, 134, 134, 134, 134, 134, 134, 134, 135, 135, 135, 135, 
    135, 135, 135, 135, 136, 136, 136, 136, 136, 136, 136, 136, 136, 137, 
    137, 137, 137, 137, 137, 137, 137, 137, 138, 138, 138, 138, 138, 138, 
    138, 138, 139, 139, 139, 139, 139, 139, 139, 139, 139, 140, 140, 140, 
    140, 140, 140, 140, 140, 140, 141, 141, 141, 141, 141, 141, 141, 141, 
    141, 142, 142, 142, 142, 142, 142, 142, 142, 142, 143, 143, 143, 143, 
    143, 143, 143, 143, 143, 144, 144, 144, 144, 144, 144, 144, 144, 144, 
    145, 145, 145, 145, 145, 145, 145, 145, 145, 146, 146, 146, 146, 146, 
    146, 146, 146, 146, 147, 147, 147, 147, 147, 147, 147, 147, 147, 148, 
    148, 148, 148, 148, 148, 148, 148, 148, 148, 149, 149, 149, 149, 149, 
    149, 149, 149, 149, 150, 150, 150, 150, 150, 150, 150, 150, 150, 151, 
    151, 151, 151, 151, 151, 151, 151, 151, 151, 152, 152, 152, 152, 152, 
    152, 152, 152, 152, 152, 153, 153, 153, 153, 153, 153, 153, 153, 153, 
    154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 155, 155, 155, 155, 
    155, 155, 155, 155, 155, 155, 156, 156, 156, 156, 156, 156, 156, 156, 
    156, 157, 157, 157, 157, 157, 157, 157, 157, 157, 157, 158, 158, 158, 
    158, 158, 158, 158, 158, 158, 158, 159, 159, 159, 159, 159, 159, 159, 
    159, 159, 159, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 161, 
    161, 161, 161, 161, 161, 161, 161, 161, 161, 161, 162, 162, 162, 162, 
    162, 162, 162, 162, 162, 162, 163, 163, 163, 163, 163, 163, 163, 163, 
    163, 163, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 165, 166, 166, 166, 166, 166, 
    166, 166, 166, 166, 166, 167, 167, 167, 167, 167, 167, 167, 167, 167, 
    167, 167, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 169, 169, 
    169, 169, 169, 169, 169, 169, 169, 169, 169, 170, 170, 170, 170, 170, 
    170, 170, 170, 170, 170, 170, 171, 171, 171, 171, 171, 171, 171, 171, 
    171, 171, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 173, 
    173, 173, 173, 173, 173, 173, 173, 173, 173, 173, 174, 174, 174, 174, 
    174, 174, 174, 174, 174, 174, 174, 175, 175, 175, 175, 175, 175, 175, 
    175, 175, 175, 175, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 
    176, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 178, 178, 
    178, 178, 178, 178, 178, 178, 178, 178, 178, 179, 179, 179, 179, 179, 
    179, 179, 179, 179, 179, 179, 179, 180, 180, 180, 180, 180, 180, 180, 
    180, 180, 180, 180, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 
    181, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 183, 
    183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 184, 184, 184, 184, 
    184, 184, 184, 184, 184, 184, 184, 184, 185, 185, 185, 185, 185, 185, 
    185, 185, 185, 185, 185, 185, 186, 186, 186, 186, 186, 186, 186, 186, 
    186, 186, 186, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 187, 
    187, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 189, 
    189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 190, 190, 190, 
    190, 190, 190, 190, 190, 190, 190, 190, 190, 191, 191, 191, 191, 191, 
    191, 191, 191, 191, 191, 191, 191, 192, 192, 192, 192, 192, 192, 192, 
    192, 192, 192, 192, 192, 193, 193, 193, 193, 193, 193, 193, 193, 193, 
    193, 193, 193, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 194, 
    194, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 195, 
    196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 196, 197, 197, 
    197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 198, 198, 198, 198, 
    198, 198, 198, 198, 198, 198, 198, 198, 198, 199, 199, 199, 199, 199, 
    199, 199, 199, 199, 199, 199, 199, 200, 200, 200, 200, 200, 200, 200, 
    200, 200, 200, 200, 200, 200, 201, 201, 201, 201, 201, 201, 201, 201, 
    201, 201, 201, 201, 201, 202, 202, 202, 202, 202, 202, 202, 202, 202, 
    202, 202, 202, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 
    203, 203, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 
    204, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 
    206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 207, 
    207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 208, 208, 
    208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 209, 209, 209, 
    209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 210, 210, 210, 210, 
    210, 210, 210, 210, 210, 210, 210, 210, 210, 211, 211, 211, 211, 211, 
    211, 211, 211, 211, 211, 211, 211, 211, 211, 212, 212, 212, 212, 212, 
    212, 212, 212, 212, 212, 212, 212, 212, 213, 213, 213, 213, 213, 213, 
    213, 213, 213, 213, 213, 213, 213, 214, 214, 214, 214, 214, 214, 214, 
    214, 214, 214, 214, 214, 214, 214, 215, 215, 215, 215, 215, 215, 215, 
    215, 215, 215, 215, 215, 215, 216, 216, 216, 216, 216, 216, 216, 216, 
    216, 216, 216, 216, 216, 216, 217, 217, 217, 217, 217, 217, 217, 217, 
    217, 217, 217, 217, 217, 217, 218, 218, 218, 218, 218, 218, 218, 218, 
    218, 218, 218, 218, 218, 219, 219, 219, 219, 219, 219, 219, 219, 219, 
    219, 219, 219, 219, 219, 220, 220, 220, 220, 220, 220, 220, 220, 220, 
    220, 220, 220, 220, 220, 221, 221, 221, 221, 221, 221, 221, 221, 221, 
    221, 221, 221, 221, 221, 222, 222, 222, 222, 222, 222, 222, 222, 222, 
    222, 222, 222, 222, 222, 223, 223, 223, 223, 223, 223, 223, 223, 223, 
    223, 223, 223, 223, 223, 224, 224, 224, 224, 224, 224, 224, 224, 224, 
    224, 224, 224, 224, 224, 225, 225, 225, 225, 225, 225, 225, 225, 225, 
    225, 225, 225, 225, 225, 226, 226, 226, 226, 226, 226, 226, 226, 226, 
    226, 226, 226, 226, 226, 227, 227, 227, 227, 227, 227, 227, 227, 227, 
    227, 227, 227, 227, 227, 227, 228, 228, 228, 228, 228, 228, 228, 228, 
    228, 228, 228, 228, 228, 228, 229, 229, 229, 229, 229, 229, 229, 229, 
    229, 229, 229, 229, 229, 229, 230, 230, 230, 230, 230, 230, 230, 230, 
    230, 230, 230, 230, 230, 230, 230, 231, 231, 231, 231, 231, 231, 231, 
    231, 231, 231, 231, 231, 231, 231, 231, 232, 232, 232, 232, 232, 232, 
    232, 232, 232, 232, 232, 232, 232, 232, 233, 233, 233, 233, 233, 233, 
    233, 233, 233, 233, 233, 233, 233, 233, 233, 234, 234, 234, 234, 234, 
    234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 235, 235, 235, 235, 
    235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 236, 236, 236, 236, 
    236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 237, 237, 237, 
    237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 238, 238, 
    238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 239, 
    239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 
    240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 
    240, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 
    241, 241, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 
    242, 242, 242, 242, 243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 
    243, 243, 243, 243, 243, 244, 244, 244, 244, 244, 244, 244, 244, 244, 
    244, 244, 244, 244, 244, 244, 245, 245, 245, 245, 245, 245, 245, 245, 
    245, 245, 245, 245, 245, 245, 245, 245, 246, 246, 246, 246, 246, 246, 
    246, 246, 246, 246, 246, 246, 246, 246, 246, 247, 247, 247, 247, 247, 
    247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 247, 248, 248, 248, 
    248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 249, 249, 
    249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 
    250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 
    250, 250, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 
    251, 251, 251, 251, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 
    252, 252, 252, 252, 252, 253, 253, 253, 253, 253, 253, 253, 253, 253, 
    253, 253, 253, 253, 253, 253, 253, 254, 254, 254, 254, 254, 254, 254, 
    254, 254, 254, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 
    255, 255, 255, 255, 
};

/* ------------------------------ *
 * Premultiplication: Shared code *
 * ------------------------------ */

/* These tables are used to divide by an integer [1..255] using only a lookup,
 * multiplication and a shift. This is faster than plain division on most
 * architectures.
 *
 * The values are tuned to minimize the error and overhead when turning
 * premultiplied (8-bit, 11-bit, 16-bit, 19-bit) into 8-bit unassociated alpha. */

/* Lossy premultiplication: 8-bit * alpha -> 8-bit. Not perfectly reversible.
 * Tuned by brute-force search: for each alpha, the entry minimizes total
 * round-trip absolute error |c - unpremul(premul(c, a))| summed over c in
 * [0, 255], where premul = ((c+1)*(a+1) - 1) >> 8 and unpremul =
 * (premul * inv_div) >> 13 (both masked to 8 bits). The unpremul output
 * is <= 0xff. */
const uint32_t _smol_inv_div_p8_lut [256] =
{
    0x00000000, 0x00140000, 0x000dc000, 0x0009a000, 0x00076000, 0x00060000, 0x00052000, 0x00046000,
    0x0003e000, 0x00037000, 0x00032000, 0x0002d556, 0x0002999a, 0x00026667, 0x00024000, 0x00021800,
    0x0001f556, 0x0001d99a, 0x0001c000, 0x0001a800, 0x0001924a, 0x00018000, 0x00017000, 0x00016000,
    0x00015000, 0x0001438f, 0x000136dc, 0x00012aab, 0x00012000, 0x000116dc, 0x00010db7, 0x000105d2,
    0x0000fd56, 0x0000f556, 0x0000ee39, 0x0000e800, 0x0000e000, 0x0000db6e, 0x0000d556, 0x0000d000,
    0x0000caab, 0x0000c5d2, 0x0000c000, 0x0000bccd, 0x0000b879, 0x0000b45e, 0x0000b000, 0x0000accd,
    0x0000a925, 0x0000a5d2, 0x0000a277, 0x00009f29, 0x00009c72, 0x0000999a, 0x00009667, 0x00009400,
    0x0000913c, 0x00008e8c, 0x00008c00, 0x000089d9, 0x00008788, 0x00008556, 0x0000830d, 0x00008175,
    0x00007f00, 0x00007d38, 0x00007b43, 0x0000799a, 0x000077b5, 0x00007600, 0x00007436, 0x000072ab,
    0x000070f1, 0x00006f73, 0x00006e00, 0x00006c7d, 0x00006b14, 0x0000699a, 0x0000684c, 0x0000671d,
    0x000065a6, 0x0000647b, 0x00006334, 0x0000620b, 0x000060c4, 0x00005fa0, 0x00005e9c, 0x00005d8a,
    0x00005c72, 0x00005b6e, 0x00005a70, 0x0000597c, 0x00005879, 0x00005788, 0x00005697, 0x000055c3,
    0x000054cd, 0x00005400, 0x00005314, 0x0000524a, 0x00005175, 0x000050a4, 0x00004fc3, 0x00004f1d,
    0x00004e59, 0x00004d9a, 0x00004ccd, 0x00004c24, 0x00004b6e, 0x00004aab, 0x00004a1b, 0x0000496a,
    0x000048bb, 0x00004800, 0x00004788, 0x000046dc, 0x00004632, 0x000045a6, 0x0000450e, 0x0000446a,
    0x000043d8, 0x00004334, 0x000042ab, 0x00004223, 0x0000419a, 0x00004109, 0x00004083, 0x00004000,
    0x00003f80, 0x00003f04, 0x00003e89, 0x00003e10, 0x00003d98, 0x00003d23, 0x00003cae, 0x00003c3d,
    0x00003bcb, 0x00003b5d, 0x00003aef, 0x00003a84, 0x00003a19, 0x000039b1, 0x00003949, 0x000038e4,
    0x0000387f, 0x0000381d, 0x000037ba, 0x0000375a, 0x000036fa, 0x0000369e, 0x00003640, 0x000035e6,
    0x0000358a, 0x00003532, 0x000034da, 0x00003484, 0x0000342d, 0x000033da, 0x00003386, 0x00003334,
    0x000032e2, 0x00003292, 0x00003242, 0x000031f4, 0x000031a6, 0x0000315a, 0x0000310e, 0x000030c4,
    0x00003079, 0x00003031, 0x00002fe8, 0x00002fa1, 0x00002f5a, 0x00002f15, 0x00002ed0, 0x00002e8c,
    0x00002e48, 0x00002e06, 0x00002dc4, 0x00002d83, 0x00002d42, 0x00002d03, 0x00002cc4, 0x00002c86,
    0x00002c48, 0x00002c0c, 0x00002bcf, 0x00002b94, 0x00002b58, 0x00002b1e, 0x00002ae4, 0x00002aab,
    0x00002a72, 0x00002a3b, 0x00002a00, 0x000029cc, 0x00002996, 0x00002960, 0x00002925, 0x000028f6,
    0x000028c2, 0x0000288e, 0x0000285a, 0x00002829, 0x000027f6, 0x000027c5, 0x00002793, 0x00002763,
    0x00002732, 0x00002703, 0x000026d3, 0x000026a5, 0x00002676, 0x00002648, 0x00002619, 0x000025ee,
    0x000025c0, 0x00002594, 0x00002568, 0x0000253d, 0x00002512, 0x000024e7, 0x000024bd, 0x00002493,
    0x00002469, 0x00002440, 0x00002416, 0x000023ef, 0x000023c4, 0x0000239f, 0x00002376, 0x00002350,
    0x00002329, 0x00002303, 0x000022dc, 0x000022b7, 0x00002290, 0x0000226c, 0x00002247, 0x00002223,
    0x000021fe, 0x000021da, 0x000021b7, 0x00002193, 0x00002170, 0x0000214e, 0x0000212a, 0x00002109,
    0x000020e7, 0x000020c5, 0x000020a4, 0x00002083, 0x00002061, 0x00002041, 0x00002000, 0x00002000,
};

/* Lossy premultiplication: 11-bit * alpha -> 11-bit. Not perfectly reversible.
 * Tuned by brute-force search: for each alpha, the entry minimizes total
 * round-trip absolute error |c_lin - unpremul(premul(c_lin, a))| summed over
 * c_lin in [0, 2047], where premul = (c_lin * (a+1)) >> 8 and unpremul =
 * (premul * inv_div) >> 11 (both masked to 11 bits). The unpremul output
 * is <= 0x7ff. */
const uint32_t _smol_inv_div_p8l_lut [256] =
{
    0x00000000, 0x00043000, 0x0002c000, 0x00020c00, 0x0001a19a, 0x00015aab, 0x00012893, 0x00010300,
    0x0000e600, 0x0000cedc, 0x0000bc00, 0x0000ac00, 0x00009eab, 0x00009356, 0x00008975, 0x000080bb,
    0x00007925, 0x00007267, 0x00006c5e, 0x000066e9, 0x00006200, 0x00005d80, 0x00005975, 0x000055b7,
    0x00005240, 0x00004f16, 0x00004c26, 0x0000496a, 0x000046dc, 0x00004480, 0x0000424a, 0x00004030,
    0x00003e43, 0x00003c6c, 0x00003aab, 0x0000390e, 0x00003780, 0x0000360c, 0x000034ab, 0x00003356,
    0x00003214, 0x000030e4, 0x00002fc0, 0x00002eab, 0x00002da0, 0x00002ca2, 0x00002bad, 0x00002ac2,
    0x000029e2, 0x0000290d, 0x00002840, 0x00002778, 0x000026b9, 0x00002600, 0x00002550, 0x000024a4,
    0x00002400, 0x00002361, 0x000022c7, 0x00002233, 0x000021a3, 0x00002118, 0x00002091, 0x0000200c,
    0x00001f90, 0x00001f16, 0x00001e9f, 0x00001e2c, 0x00001dbb, 0x00001d4f, 0x00001ce5, 0x00001c7d,
    0x00001c1a, 0x00001bb8, 0x00001b5a, 0x00001afd, 0x00001aa4, 0x00001a4c, 0x000019f7, 0x000019a3,
    0x00001952, 0x00001903, 0x000018b6, 0x0000186a, 0x00001821, 0x000017d9, 0x00001792, 0x0000174e,
    0x0000170b, 0x000016c9, 0x00001689, 0x0000164a, 0x0000160d, 0x000015d0, 0x00001596, 0x0000155c,
    0x00001524, 0x000014ec, 0x000014b6, 0x00001481, 0x0000144d, 0x0000141a, 0x000013e8, 0x000013b7,
    0x00001387, 0x00001357, 0x00001329, 0x000012fc, 0x000012cf, 0x000012a3, 0x00001278, 0x0000124e,
    0x00001224, 0x000011fc, 0x000011d3, 0x000011ac, 0x00001185, 0x0000115f, 0x0000113a, 0x00001115,
    0x000010f1, 0x000010cd, 0x000010aa, 0x00001088, 0x00001066, 0x00001045, 0x00001024, 0x00001000,
    0x00000fe4, 0x00000fc4, 0x00000fa6, 0x00000f87, 0x00000f69, 0x00000f4c, 0x00000f2f, 0x00000f12,
    0x00000ef6, 0x00000eda, 0x00000ebf, 0x00000ea4, 0x00000e89, 0x00000e6f, 0x00000e55, 0x00000e3c,
    0x00000e22, 0x00000e0a, 0x00000df1, 0x00000dd9, 0x00000dc1, 0x00000daa, 0x00000d92, 0x00000d7c,
    0x00000d65, 0x00000d4f, 0x00000d39, 0x00000d23, 0x00000d0e, 0x00000cf8, 0x00000ce4, 0x00000ccf,
    0x00000cba, 0x00000ca6, 0x00000c92, 0x00000c7f, 0x00000c6b, 0x00000c58, 0x00000c45, 0x00000c33,
    0x00000c20, 0x00000c0e, 0x00000bfc, 0x00000bea, 0x00000bd8, 0x00000bc7, 0x00000bb6, 0x00000ba5,
    0x00000b94, 0x00000b83, 0x00000b73, 0x00000b62, 0x00000b52, 0x00000b42, 0x00000b32, 0x00000b23,
    0x00000b13, 0x00000b04, 0x00000af5, 0x00000ae6, 0x00000ad7, 0x00000ac9, 0x00000aba, 0x00000aac,
    0x00000a9e, 0x00000a90, 0x00000a82, 0x00000a74, 0x00000a67, 0x00000a59, 0x00000a4c, 0x00000a3f,
    0x00000a32, 0x00000a25, 0x00000a18, 0x00000a0b, 0x000009ff, 0x000009f2, 0x000009e6, 0x000009da,
    0x000009ce, 0x000009c2, 0x000009b6, 0x000009aa, 0x0000099e, 0x00000993, 0x00000987, 0x0000097c,
    0x00000971, 0x00000966, 0x0000095b, 0x00000950, 0x00000945, 0x0000093a, 0x00000930, 0x00000925,
    0x0000091b, 0x00000911, 0x00000906, 0x000008fc, 0x000008f2, 0x000008e8, 0x000008de, 0x000008d5,
    0x000008cb, 0x000008c1, 0x000008b8, 0x000008ae, 0x000008a5, 0x0000089c, 0x00000892, 0x00000889,
    0x00000880, 0x00000877, 0x0000086e, 0x00000865, 0x0000085d, 0x00000854, 0x0000084b, 0x00000843,
    0x0000083a, 0x00000832, 0x00000829, 0x00000821, 0x00000819, 0x00000811, 0x00000809, 0x00000800,
};

/* Lossless premultiplication: 8-bit * (alpha + 1) -> 16-bit. Each entry is
 * ceil(2^16 / (alpha + 1)). With the truncating shift in unpremul_p16_to_u_128bpp
 * this round-trips every valid (c, alpha) pair exactly. Entry 0 = 2^16 inverts
 * the alpha=0 multiplier of 1, preserving the channel value. Inputs are not
 * required to satisfy c <= alpha — real unassociated input from PNG/JPEG/etc.
 * routinely doesn't, and the filter still produces in-range output: the
 * companion (alpha << 8) | 0xff alpha encoding (set by the unpack helpers)
 * makes the bilinear filter round its mixed alpha up on extraction, so mixing
 * followed by unpremul never overshoots 0xff. */
const uint32_t _smol_inv_div_p16_lut [256] =
{
    0x00010000, 0x00008000, 0x00005556, 0x00004000, 0x00003334, 0x00002aab, 0x00002493, 0x00002000,
    0x00001c72, 0x0000199a, 0x00001746, 0x00001556, 0x000013b2, 0x0000124a, 0x00001112, 0x00001000,
    0x00000f10, 0x00000e39, 0x00000d7a, 0x00000ccd, 0x00000c31, 0x00000ba3, 0x00000b22, 0x00000aab,
    0x00000a3e, 0x000009d9, 0x0000097c, 0x00000925, 0x000008d4, 0x00000889, 0x00000843, 0x00000800,
    0x000007c2, 0x00000788, 0x00000751, 0x0000071d, 0x000006ec, 0x000006bd, 0x00000691, 0x00000667,
    0x0000063f, 0x00000619, 0x000005f5, 0x000005d2, 0x000005b1, 0x00000591, 0x00000573, 0x00000556,
    0x0000053a, 0x0000051f, 0x00000506, 0x000004ed, 0x000004d5, 0x000004be, 0x000004a8, 0x00000493,
    0x0000047e, 0x0000046a, 0x00000457, 0x00000445, 0x00000433, 0x00000422, 0x00000411, 0x00000400,
    0x000003f1, 0x000003e1, 0x000003d3, 0x000003c4, 0x000003b6, 0x000003a9, 0x0000039c, 0x0000038f,
    0x00000382, 0x00000376, 0x0000036a, 0x0000035f, 0x00000354, 0x00000349, 0x0000033e, 0x00000334,
    0x0000032a, 0x00000320, 0x00000316, 0x0000030d, 0x00000304, 0x000002fb, 0x000002f2, 0x000002e9,
    0x000002e1, 0x000002d9, 0x000002d1, 0x000002c9, 0x000002c1, 0x000002ba, 0x000002b2, 0x000002ab,
    0x000002a4, 0x0000029d, 0x00000296, 0x00000290, 0x00000289, 0x00000283, 0x0000027d, 0x00000277,
    0x00000271, 0x0000026b, 0x00000265, 0x0000025f, 0x0000025a, 0x00000254, 0x0000024f, 0x0000024a,
    0x00000244, 0x0000023f, 0x0000023a, 0x00000235, 0x00000231, 0x0000022c, 0x00000227, 0x00000223,
    0x0000021e, 0x0000021a, 0x00000215, 0x00000211, 0x0000020d, 0x00000209, 0x00000205, 0x00000200,
    0x000001fd, 0x000001f9, 0x000001f5, 0x000001f1, 0x000001ed, 0x000001ea, 0x000001e6, 0x000001e2,
    0x000001df, 0x000001db, 0x000001d8, 0x000001d5, 0x000001d1, 0x000001ce, 0x000001cb, 0x000001c8,
    0x000001c4, 0x000001c1, 0x000001be, 0x000001bb, 0x000001b8, 0x000001b5, 0x000001b3, 0x000001b0,
    0x000001ad, 0x000001aa, 0x000001a7, 0x000001a5, 0x000001a2, 0x0000019f, 0x0000019d, 0x0000019a,
    0x00000198, 0x00000195, 0x00000193, 0x00000190, 0x0000018e, 0x0000018b, 0x00000189, 0x00000187,
    0x00000184, 0x00000182, 0x00000180, 0x0000017e, 0x0000017b, 0x00000179, 0x00000177, 0x00000175,
    0x00000173, 0x00000171, 0x0000016f, 0x0000016d, 0x0000016b, 0x00000169, 0x00000167, 0x00000165,
    0x00000163, 0x00000161, 0x0000015f, 0x0000015d, 0x0000015b, 0x00000159, 0x00000158, 0x00000156,
    0x00000154, 0x00000152, 0x00000151, 0x0000014f, 0x0000014d, 0x0000014b, 0x0000014a, 0x00000148,
    0x00000147, 0x00000145, 0x00000143, 0x00000142, 0x00000140, 0x0000013f, 0x0000013d, 0x0000013c,
    0x0000013a, 0x00000139, 0x00000137, 0x00000136, 0x00000134, 0x00000133, 0x00000131, 0x00000130,
    0x0000012f, 0x0000012d, 0x0000012c, 0x0000012a, 0x00000129, 0x00000128, 0x00000126, 0x00000125,
    0x00000124, 0x00000122, 0x00000121, 0x00000120, 0x0000011f, 0x0000011d, 0x0000011c, 0x0000011b,
    0x0000011a, 0x00000119, 0x00000117, 0x00000116, 0x00000115, 0x00000114, 0x00000113, 0x00000112,
    0x00000110, 0x0000010f, 0x0000010e, 0x0000010d, 0x0000010c, 0x0000010b, 0x0000010a, 0x00000109,
    0x00000108, 0x00000107, 0x00000106, 0x00000105, 0x00000104, 0x00000103, 0x00000102, 0x00000100
};

/* Lossless premultiplication for the 11-bit linear pipeline:
 * 11-bit * (alpha + 1) -> 19-bit. Each entry is ceil(2^19 / (alpha + 1)).
 * With the truncating shift in unpremul_p16l_to_ul_128bpp this round-trips
 * every valid (c_lin, alpha) pair exactly. Entry 0 = 2^19 inverts the
 * alpha=0 multiplier of 1, preserving the channel value. Same input
 * tolerance as _smol_inv_div_p16_lut: the (alpha << 8) | 0xff encoding
 * keeps unpremul output in [0, 0xff] even when c > alpha. */
const uint32_t _smol_inv_div_p16l_lut [256] =
{
    0x00080000, 0x00040000, 0x0002aaab, 0x00020000, 0x0001999a, 0x00015556, 0x00012493, 0x00010000,
    0x0000e38f, 0x0000cccd, 0x0000ba2f, 0x0000aaab, 0x00009d8a, 0x0000924a, 0x00008889, 0x00008000,
    0x00007879, 0x000071c8, 0x00006bcb, 0x00006667, 0x00006187, 0x00005d18, 0x0000590c, 0x00005556,
    0x000051ec, 0x00004ec5, 0x00004bdb, 0x00004925, 0x0000469f, 0x00004445, 0x00004211, 0x00004000,
    0x00003e10, 0x00003c3d, 0x00003a84, 0x000038e4, 0x0000375a, 0x000035e6, 0x00003484, 0x00003334,
    0x000031f4, 0x000030c4, 0x00002fa1, 0x00002e8c, 0x00002d83, 0x00002c86, 0x00002b94, 0x00002aab,
    0x000029cc, 0x000028f6, 0x00002829, 0x00002763, 0x000026a5, 0x000025ee, 0x0000253d, 0x00002493,
    0x000023ef, 0x00002350, 0x000022b7, 0x00002223, 0x00002193, 0x00002109, 0x00002083, 0x00002000,
    0x00001f82, 0x00001f08, 0x00001e92, 0x00001e1f, 0x00001daf, 0x00001d42, 0x00001cd9, 0x00001c72,
    0x00001c0f, 0x00001bad, 0x00001b4f, 0x00001af3, 0x00001a99, 0x00001a42, 0x000019ed, 0x0000199a,
    0x00001949, 0x000018fa, 0x000018ad, 0x00001862, 0x00001819, 0x000017d1, 0x0000178b, 0x00001746,
    0x00001703, 0x000016c2, 0x00001682, 0x00001643, 0x00001606, 0x000015ca, 0x0000158f, 0x00001556,
    0x0000151e, 0x000014e6, 0x000014b0, 0x0000147b, 0x00001447, 0x00001415, 0x000013e3, 0x000013b2,
    0x00001382, 0x00001353, 0x00001324, 0x000012f7, 0x000012ca, 0x0000129f, 0x00001274, 0x0000124a,
    0x00001220, 0x000011f8, 0x000011d0, 0x000011a8, 0x00001182, 0x0000115c, 0x00001136, 0x00001112,
    0x000010ed, 0x000010ca, 0x000010a7, 0x00001085, 0x00001063, 0x00001042, 0x00001021, 0x00001000,
    0x00000fe1, 0x00000fc1, 0x00000fa3, 0x00000f84, 0x00000f67, 0x00000f49, 0x00000f2c, 0x00000f10,
    0x00000ef3, 0x00000ed8, 0x00000ebc, 0x00000ea1, 0x00000e87, 0x00000e6d, 0x00000e53, 0x00000e39,
    0x00000e20, 0x00000e08, 0x00000def, 0x00000dd7, 0x00000dbf, 0x00000da8, 0x00000d91, 0x00000d7a,
    0x00000d63, 0x00000d4d, 0x00000d37, 0x00000d21, 0x00000d0c, 0x00000cf7, 0x00000ce2, 0x00000ccd,
    0x00000cb9, 0x00000ca5, 0x00000c91, 0x00000c7d, 0x00000c6a, 0x00000c57, 0x00000c44, 0x00000c31,
    0x00000c1f, 0x00000c0d, 0x00000bfb, 0x00000be9, 0x00000bd7, 0x00000bc6, 0x00000bb4, 0x00000ba3,
    0x00000b93, 0x00000b82, 0x00000b71, 0x00000b61, 0x00000b51, 0x00000b41, 0x00000b31, 0x00000b22,
    0x00000b12, 0x00000b03, 0x00000af4, 0x00000ae5, 0x00000ad7, 0x00000ac8, 0x00000ab9, 0x00000aab,
    0x00000a9d, 0x00000a8f, 0x00000a81, 0x00000a73, 0x00000a66, 0x00000a58, 0x00000a4b, 0x00000a3e,
    0x00000a31, 0x00000a24, 0x00000a17, 0x00000a0b, 0x000009fe, 0x000009f2, 0x000009e5, 0x000009d9,
    0x000009cd, 0x000009c1, 0x000009b5, 0x000009aa, 0x0000099e, 0x00000992, 0x00000987, 0x0000097c,
    0x00000971, 0x00000965, 0x0000095b, 0x00000950, 0x00000945, 0x0000093a, 0x00000930, 0x00000925,
    0x0000091b, 0x00000910, 0x00000906, 0x000008fc, 0x000008f2, 0x000008e8, 0x000008de, 0x000008d4,
    0x000008cb, 0x000008c1, 0x000008b8, 0x000008ae, 0x000008a5, 0x0000089b, 0x00000892, 0x00000889,
    0x00000880, 0x00000877, 0x0000086e, 0x00000865, 0x0000085c, 0x00000854, 0x0000084b, 0x00000843,
    0x0000083a, 0x00000832, 0x00000829, 0x00000821, 0x00000819, 0x00000811, 0x00000809, 0x00000800
};

/* ------- *
 * Helpers *
 * ------- */

static SMOL_INLINE int
check_row_range (const SmolScaleCtx *scale_ctx,
                 int32_t *first_dest_row,
                 int32_t *n_dest_rows)
{
    if (*first_dest_row < 0)
    {
        *n_dest_rows += *first_dest_row;
        *first_dest_row = 0;
    }
    else if (*first_dest_row >= (int32_t) scale_ctx->vdim.dest_size_px)
    {
        return 0;
    }

    if (*n_dest_rows < 0
        || *n_dest_rows > (int32_t) scale_ctx->vdim.dest_size_px - *first_dest_row)
    {
        *n_dest_rows = scale_ctx->vdim.dest_size_px - *first_dest_row;
    }
    else if (*n_dest_rows == 0)
    {
        return 0;
    }

    return 1;
}

/* ------------------- *
 * Scaling: Outer loop *
 * ------------------- */

static SMOL_INLINE const char *
src_row_ofs_to_pointer (const SmolScaleCtx *scale_ctx,
                        uint32_t src_row_ofs)
{
    return scale_ctx->src_pixels + scale_ctx->src_rowstride * src_row_ofs;
}

static SMOL_INLINE char *
dest_row_ofs_to_pointer (const SmolScaleCtx *scale_ctx,
                         uint32_t dest_row_ofs)
{
    return scale_ctx->dest_pixels + scale_ctx->dest_rowstride * dest_row_ofs;
}

static SMOL_INLINE void *
dest_hofs_to_pointer (const SmolScaleCtx *scale_ctx,
                      void *dest_row_ptr,
                      uint32_t dest_hofs)
{
    uint8_t *dest_row_ptr_u8 = dest_row_ptr;
    return dest_row_ptr_u8 + dest_hofs * pixel_type_meta [scale_ctx->dest_pixel_type].pixel_stride;
}

static void
copy_row (const SmolScaleCtx *scale_ctx,
          uint32_t dest_row_index,
          uint32_t *row_out)
{
    memcpy (row_out,
            src_row_ofs_to_pointer (scale_ctx, dest_row_index),
            scale_ctx->hdim.dest_size_px * pixel_type_meta [scale_ctx->dest_pixel_type].pixel_stride);
}

static void
scale_dest_row (const SmolScaleCtx *scale_ctx,
                SmolLocalCtx *local_ctx,
                uint32_t dest_row_index,
                void *row_out)
{
    if (dest_row_index < (uint32_t) scale_ctx->vdim.clear_before_px
        || dest_row_index >= scale_ctx->vdim.dest_size_px - (uint32_t) scale_ctx->vdim.clear_after_px)
    {
        /* Row doesn't intersect placement */

        if (scale_ctx->flags & SMOL_CLEAR_DEST)
        {
            /* Clear entire row */
            scale_ctx->clear_dest_func (scale_ctx->color_pixels_clear_batch,
                                        row_out,
                                        scale_ctx->hdim.dest_size_px);
        }
    }
    else
    {
        if (scale_ctx->flags & SMOL_CLEAR_DEST)
        {
            /* Clear left */
            scale_ctx->clear_dest_func (scale_ctx->color_pixels_clear_batch,
                                        row_out,
                                        scale_ctx->hdim.clear_before_px);
        }

        if (scale_ctx->is_noop)
        {
            copy_row (scale_ctx, dest_row_index, row_out);
        }
        else if (scale_ctx->composite_op == SMOL_COMPOSITE_SRC_OVER_DEST)
        {
            int scaled_row_index;
            void *dest_ptr;

            /* Source-over onto the existing destination content:
             *
             * - Unpack and scale the source row.
             * - Unpack the destination pixels (placement rect).
             * - Composite src OVER dest in parts space.
             * - Pack the blended result back to dest. */

            scaled_row_index = scale_ctx->vfilter_func (scale_ctx,
                                                        local_ctx,
                                                        dest_row_index - scale_ctx->vdim.clear_before_px);

            dest_ptr = dest_hofs_to_pointer (scale_ctx, row_out,
                                             scale_ctx->hdim.placement_ofs_px);

            scale_ctx->dest_unpack_row_func (dest_ptr,
                                             local_ctx->dest_parts_row,
                                             scale_ctx->hdim.placement_size_px);

            scale_ctx->composite_over_dest_func (local_ctx->parts_row [scaled_row_index],
                                                 local_ctx->dest_parts_row,
                                                 scale_ctx->hdim.placement_size_px,
                                                 scale_ctx->composite_opacity);

            scale_ctx->pack_row_func (local_ctx->dest_parts_row,
                                      dest_ptr,
                                      scale_ctx->hdim.placement_size_px);
        }
        else
        {
            int scaled_row_index;

            scaled_row_index = scale_ctx->vfilter_func (scale_ctx,
                                                        local_ctx,
                                                        dest_row_index - scale_ctx->vdim.clear_before_px);

            if (scale_ctx->have_composite_color
                || scale_ctx->composite_opacity < SMOL_SUBPIXEL_MUL)
            {
                scale_ctx->composite_over_color_func (local_ctx->parts_row [scaled_row_index],
                                                      scale_ctx->color_pixel,
                                                      scale_ctx->hdim.placement_size_px,
                                                      scale_ctx->composite_opacity);
            }

            scale_ctx->pack_row_func (local_ctx->parts_row [scaled_row_index],
                                      dest_hofs_to_pointer (scale_ctx, row_out, scale_ctx->hdim.placement_ofs_px),
                                      scale_ctx->hdim.placement_size_px);

        }

        if (scale_ctx->flags & SMOL_CLEAR_DEST)
        {
            /* Clear right */
            scale_ctx->clear_dest_func (scale_ctx->color_pixels_clear_batch,
                                        dest_hofs_to_pointer (scale_ctx, row_out,
                                                              scale_ctx->hdim.placement_ofs_px
                                                              + scale_ctx->hdim.placement_size_px),
                                        scale_ctx->hdim.clear_after_px);
        }
    }

    if (scale_ctx->post_row_func)
        scale_ctx->post_row_func (row_out, scale_ctx->hdim.dest_size_px, scale_ctx->user_data);
}

static int
do_rows (const SmolScaleCtx *scale_ctx,
         void *dest,
         uint32_t row_dest_index,
         uint32_t n_rows)
{
    SmolLocalCtx local_ctx = { 0 };
    uint32_t n_parts_per_pixel = 1;
    uint32_t n_stored_rows = 4;
    uint32_t i;
    int result = 0;

    if (scale_ctx->storage_type == SMOL_STORAGE_128BPP)
        n_parts_per_pixel = 2;

    /* Must be one less, or this test in update_local_ctx() will wrap around:
     * if (new_src_ofs == local_ctx->src_ofs + 1) { ... } */
    local_ctx.src_ofs = UINT_MAX - 1;

    for (i = 0; i < n_stored_rows; i++)
    {
        /* Allocate space for an extra pixel at the rightmost edge. This pixel
         * allows bilinear horizontal sampling to exceed the input width and
         * produce transparency when the output is smaller than its whole-pixel
         * count. This is especially noticeable with halving, which can
         * produce 2^n such samples (the extra pixel is sampled repeatedly in
         * those cases).
         *
         * FIXME: This is no longer true, and the extra storage is probably not
         * needed. The edge transparency is now handled by applying a precalculated
         * opacity directly. We should verify that the extra storage can be
         * eliminated without overruns. */

        local_ctx.parts_row [i] =
            smol_alloc_aligned (MAX (scale_ctx->hdim.src_size_px + 1, scale_ctx->hdim.placement_size_px)
                                * n_parts_per_pixel * sizeof (uint64_t),
                                &local_ctx.row_storage [i]);
        if (!local_ctx.row_storage [i])
        {
            /* Allocation failed */
            goto out;
        }

        local_ctx.parts_row [i] [scale_ctx->hdim.src_size_px * n_parts_per_pixel] = 0;
        if (n_parts_per_pixel == 2)
            local_ctx.parts_row [i] [scale_ctx->hdim.src_size_px * n_parts_per_pixel + 1] = 0;
    }

    if (scale_ctx->src_pixel_type != SMOL_PIXEL_RGB8
        && scale_ctx->src_pixel_type != SMOL_PIXEL_BGR8
        && (((uintptr_t) scale_ctx->src_pixels & 3) || (scale_ctx->src_rowstride & 3)))
    {
        /* 32-bit unpackers need 32-bit alignment. Used in scale_horizontal(). */
        local_ctx.src_aligned =
            smol_alloc_aligned (scale_ctx->hdim.src_size_px * sizeof (uint32_t),
                                &local_ctx.src_aligned_storage);
        if (!local_ctx.src_aligned_storage)
            goto out;
    }

    /* The SMOL_COMPOSITE_SRC_OVER_DEST path needs a scratch row to hold the
     * unpacked destination pixels under the placement rectangle. */
    if (scale_ctx->composite_op == SMOL_COMPOSITE_SRC_OVER_DEST)
    {
        local_ctx.dest_parts_row =
            smol_alloc_aligned (MAX (scale_ctx->hdim.placement_size_px, 1)
                                * n_parts_per_pixel * sizeof (uint64_t),
                                &local_ctx.dest_parts_storage);
    }

    for (i = row_dest_index; i < row_dest_index + n_rows; i++)
    {
        scale_dest_row (scale_ctx, &local_ctx, i, dest);
        dest = (char *) dest + scale_ctx->dest_rowstride;
    }

    result = 1;

out:
    for (i = 0; i < n_stored_rows; i++)
        free (local_ctx.row_storage [i]);

    if (local_ctx.dest_parts_storage)
        free (local_ctx.dest_parts_storage);

    if (local_ctx.src_aligned)
        free (local_ctx.src_aligned_storage);

    return result;
}

/* -------------------- *
 * Architecture support *
 * -------------------- */

#ifdef SMOL_WITH_AVX2

static SmolBool
have_avx2 (void)
{
    __builtin_cpu_init ();

    if (__builtin_cpu_supports ("avx2"))
        return TRUE;

    return FALSE;
}

#endif

static SmolBool
host_is_little_endian (void)
{
    static const union
    {
        uint8_t u8 [4];
        uint32_t u32;
    }
    host_bytes = { { 0, 1, 2, 3 } };

    if (host_bytes.u32 == 0x03020100UL)
        return TRUE;

    return FALSE;
}

/* The generic unpack/pack functions fetch and store pixels as u32.
 * This means the byte order will be reversed on little endian, with
 * consequences for the alpha channel and reordering logic. We deal
 * with this by using the apparent byte order internally. */
static SmolPixelType
get_host_pixel_type (SmolPixelType pixel_type)
{
    if (host_is_little_endian ())
        return pixel_type_u32_le [pixel_type];

    return pixel_type;
}

/* ---------------------- *
 * Context initialization *
 * ---------------------- */

static void
pick_filter_params (uint32_t src_dim,
                    uint32_t src_dim_spx,
                    int32_t dest_ofs_spx,
                    uint32_t dest_dim,
                    uint32_t dest_dim_spx,
                    uint32_t *dest_halvings,
                    uint32_t *dest_dim_prehalving,
                    uint32_t *dest_dim_prehalving_spx,
                    SmolFilterType *dest_filter,
                    SmolStorageType *dest_storage,
                    uint16_t *first_opacity,
                    uint16_t *last_opacity,
                    SmolFlags flags)
{
    *dest_dim_prehalving = dest_dim;
    *dest_storage = (flags & SMOL_DISABLE_SRGB_LINEARIZATION) ? SMOL_STORAGE_64BPP : SMOL_STORAGE_128BPP;

    /* 64-bit intermediates: dest_ofs_spx can be INT32_MIN and dest_dim_spx
     * close to INT32_MAX, so the negation and the sum must not wrap. */
    *first_opacity = SMOL_SUBPIXEL_MOD (-((int64_t) dest_ofs_spx) - 1) + 1;
    *last_opacity = SMOL_SUBPIXEL_MOD ((int64_t) dest_ofs_spx + dest_dim_spx - 1) + 1;

    /* Special handling when the output is a single pixel */

    if (dest_dim == 1)
    {
        *first_opacity = dest_dim_spx;
        *last_opacity = 256;
    }

    /* A zero-size placement (which can result from clipping) renders nothing.
     * None of the scaling filters support a zero-size output, so pick the one
     * with no precalc and bail out. */

    if (dest_dim_spx == 0)
    {
        *dest_dim_prehalving_spx = 0;
        *dest_halvings = 0;
        *dest_filter = SMOL_FILTER_ONE;
        return;
    }

    if ((uint64_t) src_dim_spx > (uint64_t) MAX (dest_dim_spx, SMOL_SUBPIXEL_MUL) * 255)
    {
        /* The box span per output pixel is src_dim_spx / dest_dim_spx
         * (with the precalc's one-pixel output floor), so the 16-bit
         * lanes of 64bpp storage can overflow beyond 255x. Fractional
         * and sub-pixel placement sizes push the true span above the
         * whole-pixel ratio, so this must be measured in subpixels. */
        *dest_storage = SMOL_STORAGE_128BPP;
        *dest_filter = SMOL_FILTER_BOX;
    }
    else if ((uint64_t) src_dim_spx
             >= (uint64_t) MAX (dest_dim_spx, SMOL_SUBPIXEL_MUL) * SMOL_BILIN_BOX_CUTOFF)
    {
        /* Like all ratio decisions, measured in subpixels with a one-
         * output-pixel floor, so fractional placement sizes hand off at
         * the same true ratio as whole-pixel ones. */
        *dest_filter = SMOL_FILTER_BOX;
    }
    else if (src_dim <= 1)
    {
        /* last_opacity keeps the default computed above; re-deriving it
         * here would double-attenuate single-pixel placements, whose
         * entire coverage is already in first_opacity (dest_dim == 1
         * case above). */
        *dest_filter = SMOL_FILTER_ONE;
    }
    else if ((dest_ofs_spx & 0xff) == 0 && src_dim_spx == dest_dim_spx)
    {
        *dest_filter = SMOL_FILTER_COPY;
        *first_opacity = 256;
        *last_opacity = 256;
    }
    else
    {
        uint32_t n_halvings = 0;

        /* Sampling depth is picked as if the output were at least one full
         * pixel wide. Sample positions still use the true fractional geometry. */
        uint64_t d = MAX (dest_dim_spx, SMOL_SUBPIXEL_MUL);

        for (;;)
        {
            d *= 2;
            if (d >= src_dim_spx)
                break;
            n_halvings++;
        }

        /* We hand off to the box filter beyond SMOL_BILIN_BOX_CUTOFF. This
         * clamp is extra safety from running off the end of the filter table. */
        n_halvings = MIN (n_halvings, 3);

        *dest_dim_prehalving = dest_dim << n_halvings;
        *dest_dim_prehalving_spx = dest_dim_spx << n_halvings;
        *dest_filter = SMOL_FILTER_BILINEAR_0H + n_halvings;
        *dest_halvings = n_halvings;
    }

}

static const SmolRepackMeta *
find_repack_match (const SmolRepackMeta *meta, uint16_t sig, uint16_t mask)
{
    sig &= mask;

    for (;; meta++)
    {
        if (!meta->repack_row_func)
        {
            meta = NULL;
            break;
        }

        if (sig == (meta->signature & mask))
            break;
    }

    return meta;
}

static void
do_reorder (const uint8_t *order_in, uint8_t *order_out, const uint8_t *reorder)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        uint8_t r = reorder [i];
        uint8_t o;

        if (r == 0)
        {
            o = 0;
        }
        else
        {
            o = order_in [r - 1];
            if (o == 0)
                o = i + 1;
        }

        order_out [i] = o;
    }
}

static void
find_repacks (const SmolImplementation **implementations,
              SmolStorageType src_storage, SmolStorageType mid_storage, SmolStorageType dest_storage,
              SmolAlphaType src_alpha, SmolAlphaType mid_alpha, SmolAlphaType dest_alpha,
              SmolGammaType src_gamma, SmolGammaType mid_gamma, SmolGammaType dest_gamma,
              const SmolPixelTypeMeta *src_pmeta, const SmolPixelTypeMeta *dest_pmeta,
              const SmolRepackMeta **src_repack, const SmolRepackMeta **dest_repack)
{
    int src_impl, dest_impl;
    const SmolRepackMeta *src_meta = NULL, *dest_meta = NULL;
    uint16_t src_to_mid_sig, mid_to_dest_sig;
    uint16_t sig_mask;
    int reorder_dest_alpha_ch;

    sig_mask = SMOL_REPACK_SIGNATURE_ANY_ORDER_MASK (1, 1, 1, 1, 1, 1);
    src_to_mid_sig = SMOL_MAKE_REPACK_SIGNATURE_ANY_ORDER (src_storage, src_alpha, src_gamma,
                                                           mid_storage, mid_alpha, mid_gamma);
    mid_to_dest_sig = SMOL_MAKE_REPACK_SIGNATURE_ANY_ORDER (mid_storage, mid_alpha, mid_gamma,
                                                            dest_storage, dest_alpha, dest_gamma);

    /* The initial conversion must always leave alpha in position #4, so further
     * processing knows where to find it. The order of the other channels
     * doesn't matter, as long as there's a repack chain that ultimately
     * produces the desired result. */
    reorder_dest_alpha_ch = src_pmeta->order [0] == 4 ? 1 : 4;

    for (src_impl = 0; implementations [src_impl]; src_impl++)
    {
        src_meta = &implementations [src_impl]->repack_meta [0];

        for (;; src_meta++)
        {
            uint8_t mid_order [4];

            src_meta = find_repack_match (src_meta, src_to_mid_sig, sig_mask);
            if (!src_meta)
                break;

            if (reorder_meta [SMOL_REPACK_SIGNATURE_GET_REORDER (src_meta->signature)].dest [3] != reorder_dest_alpha_ch)
                continue;

            do_reorder (src_pmeta->order, mid_order,
                        reorder_meta [SMOL_REPACK_SIGNATURE_GET_REORDER (src_meta->signature)].dest);

            for (dest_impl = 0; implementations [dest_impl]; dest_impl++)
            {
                dest_meta = &implementations [dest_impl]->repack_meta [0];

                for (;; dest_meta++)
                {
                    uint8_t dest_order [4];

                    dest_meta = find_repack_match (dest_meta, mid_to_dest_sig, sig_mask);
                    if (!dest_meta)
                        break;

                    do_reorder (mid_order, dest_order,
                                reorder_meta [SMOL_REPACK_SIGNATURE_GET_REORDER (dest_meta->signature)].dest);

                    if (!memcmp (&dest_order, &dest_pmeta->order, sizeof (dest_order)))
                    {
                        /* Success */
                        goto out;
                    }
                }
            }
        }
    }

out:
    if (src_repack)
        *src_repack = src_meta;
    if (dest_repack)
        *dest_repack = dest_meta;
}

/* Finds a repack whose reorder, applied to src_pmeta's channel order,
 * produces exactly dest_order. Used to unpack destination rows into the
 * same internal channel order as the scaled source parts, so
 * SMOL_COMPOSITE_SRC_OVER_DEST blends matching channels. */
static const SmolRepackMeta *
find_repack_to_order (const SmolImplementation **implementations,
                      SmolStorageType src_storage, SmolStorageType dest_storage,
                      SmolAlphaType src_alpha, SmolAlphaType dest_alpha,
                      SmolGammaType src_gamma, SmolGammaType dest_gamma,
                      const SmolPixelTypeMeta *src_pmeta,
                      const uint8_t *dest_order)
{
    uint16_t sig, sig_mask;
    int impl;

    sig_mask = SMOL_REPACK_SIGNATURE_ANY_ORDER_MASK (1, 1, 1, 1, 1, 1);
    sig = SMOL_MAKE_REPACK_SIGNATURE_ANY_ORDER (src_storage, src_alpha, src_gamma,
                                                dest_storage, dest_alpha, dest_gamma);

    for (impl = 0; implementations [impl]; impl++)
    {
        const SmolRepackMeta *meta = &implementations [impl]->repack_meta [0];

        for (;; meta++)
        {
            uint8_t order [4];

            meta = find_repack_match (meta, sig, sig_mask);
            if (!meta)
                break;

            do_reorder (src_pmeta->order, order,
                        reorder_meta [SMOL_REPACK_SIGNATURE_GET_REORDER (meta->signature)].dest);

            if (!memcmp (order, dest_order, sizeof (order)))
                return meta;
        }
    }

    return NULL;
}

/* Converts a caller-provided color pixel of any supported type to
 * unassociated RGBA bytes. */
static void
color_pixel_to_rgba (const void *color_pixel,
                     SmolPixelType color_pixel_type,
                     uint8_t *rgba_out)
{
    const SmolPixelTypeMeta *pmeta = &pixel_type_meta [color_pixel_type];
    const uint8_t *in = color_pixel;
    uint8_t a;
    int i;

    /* Ensure 3-channel input is opaque */

    rgba_out [3] = 0xff;

    /* Reorder */

    for (i = 0; i < pmeta->pixel_stride; i++)
        rgba_out [pmeta->order [i] - 1] = in [i];

    a = rgba_out [3];

    /* Unpremultiply */

    if (pmeta->alpha == SMOL_ALPHA_PREMUL8)
    {
        for (i = 0; i < 3; i++)
        {
            uint16_t u = a ? (rgba_out [i] * 255 + a / 2) / a : 0;
            rgba_out [i] = u < 0xff ? u : 0xff;
        }
    }
}

static void
populate_clear_batch (SmolScaleCtx *scale_ctx)
{
    SMOL_ALIGN uint8_t dest_color [16];
    int pixel_stride;
    int i;

    scale_ctx->pack_row_func (scale_ctx->color_pixel, dest_color, 1);
    pixel_stride = pixel_type_meta [scale_ctx->dest_pixel_type].pixel_stride;

    for (i = 0; i != SMOL_CLEAR_BATCH_SIZE; i += pixel_stride)
    {
        /* Must be an exact fit */
        SMOL_ASSERT (i + pixel_stride <= SMOL_CLEAR_BATCH_SIZE);

        memcpy (scale_ctx->color_pixels_clear_batch + i, dest_color, pixel_stride);
    }
}

#define IMPLEMENTATION_MAX 8

/* scale_ctx->storage_type must be initialized first by pick_filter_params() */
static void
get_implementations (SmolScaleCtx *scale_ctx, const void *color_pixel, SmolPixelType color_pixel_type)
{
    SmolPixelType src_ptype, dest_ptype;
    const SmolPixelTypeMeta *src_pmeta, *dest_pmeta;
    const SmolRepackMeta *src_rmeta, *dest_rmeta;
    SmolAlphaType internal_alpha = SMOL_ALPHA_PREMUL8;
    const SmolImplementation *implementations [IMPLEMENTATION_MAX];
    SMOL_ALIGN uint8_t color_rgba [4];
    int i = 0;

    if (color_pixel)
    {
        /* Convert color pixel to canonical unassoc RGBA. A color that
         * comes out exactly transparent black is indistinguishable from
         * no color at all; treat it as such, keeping the copy fast path
         * available. */

        color_pixel_to_rgba (color_pixel, color_pixel_type, color_rgba);

        if (color_rgba [0] || color_rgba [1] || color_rgba [2] || color_rgba [3])
            scale_ctx->have_composite_color = TRUE;
    }

    /* Check for noop (direct copy). Only valid when the placement covers
     * the destination exactly; a smaller, offset or clipped placement still
     * needs the scaling path even at a 1:1 size ratio. A composite color
     * or a layer opacity below 1.0 also disqualifies - the source must be
     * blended. */

    if (scale_ctx->hdim.src_size_spx == scale_ctx->hdim.dest_size_spx
        && scale_ctx->vdim.src_size_spx == scale_ctx->vdim.dest_size_spx
        && scale_ctx->hdim.placement_size_spx == scale_ctx->hdim.dest_size_spx
        && scale_ctx->vdim.placement_size_spx == scale_ctx->vdim.dest_size_spx
        && scale_ctx->hdim.placement_ofs_spx == 0
        && scale_ctx->vdim.placement_ofs_spx == 0
        && scale_ctx->src_pixel_type == scale_ctx->dest_pixel_type
        && scale_ctx->composite_op != SMOL_COMPOSITE_SRC_OVER_DEST
        && !scale_ctx->have_composite_color
        && scale_ctx->composite_opacity == SMOL_SUBPIXEL_MUL)
    {
        /* The scaling and packing is a no-op, but we may still need to
         * clear dest, so allow the rest of the function to run so we get
         * the clear functions etc. */
        scale_ctx->is_noop = TRUE;
    }

    /* Enumerate implementations, preferred first */

    if (!(scale_ctx->flags & SMOL_DISABLE_ACCELERATION))
    {
#ifdef SMOL_WITH_AVX2
        if (have_avx2 ())
            implementations [i++] = _smol_get_avx2_implementation ();
#endif
    }

    implementations [i++] = _smol_get_generic_implementation ();
    implementations [i] = NULL;

    /* Install repackers */

    src_ptype = get_host_pixel_type (scale_ctx->src_pixel_type);
    dest_ptype = get_host_pixel_type (scale_ctx->dest_pixel_type);

    src_pmeta = &pixel_type_meta [src_ptype];
    dest_pmeta = &pixel_type_meta [dest_ptype];

    if (src_pmeta->alpha == SMOL_ALPHA_UNASSOCIATED
        && dest_pmeta->alpha == SMOL_ALPHA_UNASSOCIATED)
    {
        /* In order to preserve the color range in transparent pixels when going
         * from unassociated to unassociated, we use 16 bits per channel internally. */
        internal_alpha = SMOL_ALPHA_PREMUL16;
        scale_ctx->storage_type = SMOL_STORAGE_128BPP;
    }

    if ((uint64_t) scale_ctx->hdim.src_size_spx
        > (uint64_t) MAX (scale_ctx->hdim.placement_size_spx, SMOL_SUBPIXEL_MUL) * 8191
        || (uint64_t) scale_ctx->vdim.src_size_spx
        > (uint64_t) MAX (scale_ctx->vdim.placement_size_spx, SMOL_SUBPIXEL_MUL) * 8191)
    {
        /* Even with 128bpp, there's only enough bits to store 11-bit linearized
         * times 13 bits of summed pixels plus 8 bits of scratch space for
         * multiplying with an 8-bit weight -> 32 bits total per channel.
         *
         * For now, just turn off sRGB linearization if the input is bigger
         * than the output by a factor of 2^13 or more. Measured against the
         * virtual (unclipped) placement in subpixels: that's the true box
         * span, which fractional and sub-pixel placement sizes push above
         * the whole-pixel ratio. */
        scale_ctx->gamma_type = SMOL_GAMMA_SRGB_COMPRESSED;
    }

    find_repacks (implementations,
                  src_pmeta->storage, scale_ctx->storage_type, dest_pmeta->storage,
                  src_pmeta->alpha, internal_alpha, dest_pmeta->alpha,
                  SMOL_GAMMA_SRGB_COMPRESSED, scale_ctx->gamma_type, SMOL_GAMMA_SRGB_COMPRESSED,
                  src_pmeta, dest_pmeta,
                  &src_rmeta, &dest_rmeta);

    SMOL_ASSERT (src_rmeta != NULL);
    SMOL_ASSERT (dest_rmeta != NULL);

    scale_ctx->src_unpack_row_func = src_rmeta->repack_row_func;
    scale_ctx->pack_row_func = dest_rmeta->repack_row_func;

    if (scale_ctx->composite_op == SMOL_COMPOSITE_SRC_OVER_DEST)
    {
        const SmolRepackMeta *dest_unpack_rmeta;
        uint8_t mid_order [4];

        /* Need to unpack destination rows and composite on them. The rows
         * must be unpacked into the same internal channel order as the
         * scaled source parts, so the compositor blends matching channels
         * and pack_row_func inverts the unpack exactly. */

        do_reorder (src_pmeta->order, mid_order,
                    reorder_meta [SMOL_REPACK_SIGNATURE_GET_REORDER (src_rmeta->signature)].dest);

        dest_unpack_rmeta = find_repack_to_order (implementations,
                                                  dest_pmeta->storage, scale_ctx->storage_type,
                                                  dest_pmeta->alpha, internal_alpha,
                                                  SMOL_GAMMA_SRGB_COMPRESSED, scale_ctx->gamma_type,
                                                  dest_pmeta, mid_order);

        SMOL_ASSERT (dest_unpack_rmeta != NULL);

        scale_ctx->dest_unpack_row_func = dest_unpack_rmeta->repack_row_func;
    }

    /* The color pixel is used when compositing over a color, and for the
     * SMOL_CLEAR_DEST fill outside the placement (with either op). */

    if (scale_ctx->have_composite_color)
    {
        const SmolRepackMeta *color_rmeta;
        uint8_t mid_order [4];

        /* The fill color must be in the same internal channel order as
         * the scaled source parts, so the compositor blends matching
         * channels and pack_row_func packs it correctly. */

        /* Get the mid_order */

        do_reorder (src_pmeta->order, mid_order,
                    reorder_meta [SMOL_REPACK_SIGNATURE_GET_REORDER (src_rmeta->signature)].dest);

        /* Repack */

        color_rmeta = find_repack_to_order (implementations,
                                            SMOL_STORAGE_32BPP, scale_ctx->storage_type,
                                            SMOL_ALPHA_UNASSOCIATED, internal_alpha,
                                            SMOL_GAMMA_SRGB_COMPRESSED, scale_ctx->gamma_type,
                                            &pixel_type_meta [get_host_pixel_type (SMOL_PIXEL_RGBA8_UNASSOCIATED)],
                                            mid_order);

        SMOL_ASSERT (color_rmeta != NULL);

        color_rmeta->repack_row_func (color_rgba, scale_ctx->color_pixel, 1);
    }
    else
    {
        /* No color provided; use fully transparent black */
        memset (scale_ctx->color_pixel, 0, sizeof (scale_ctx->color_pixel));
    }

    if (scale_ctx->flags & SMOL_CLEAR_DEST)
        populate_clear_batch (scale_ctx);

    /* Install filters and compositors */

    scale_ctx->hfilter_func = NULL;
    scale_ctx->vfilter_func = NULL;
    scale_ctx->composite_over_color_func = NULL;
    scale_ctx->composite_over_dest_func = NULL;
    scale_ctx->clear_dest_func = NULL;

    for (i = 0; implementations [i]; i++)
    {
        SmolHFilterFunc *hfilter_func =
            implementations [i]->hfilter_funcs [scale_ctx->storage_type] [scale_ctx->hdim.filter_type];
        SmolVFilterFunc *vfilter_func =
            implementations [i]->vfilter_funcs [scale_ctx->storage_type] [scale_ctx->vdim.filter_type];
        SmolCompositeOverColorFunc *composite_over_color_func =
            implementations [i]->composite_over_color_funcs
                [scale_ctx->storage_type] [scale_ctx->gamma_type] [internal_alpha];
        SmolCompositeOverDestFunc *composite_over_dest_func =
            implementations [i]->composite_over_dest_funcs
                [scale_ctx->storage_type] [scale_ctx->gamma_type] [internal_alpha];
        SmolClearFunc *clear_dest_func =
            implementations [i]->clear_funcs [dest_pmeta->storage];

        if (!scale_ctx->hfilter_func && hfilter_func)
        {
            scale_ctx->hfilter_func = hfilter_func;
            if (implementations [i]->init_h_func)
                implementations [i]->init_h_func (scale_ctx);
        }

        if (!scale_ctx->vfilter_func && vfilter_func)
        {
            scale_ctx->vfilter_func = vfilter_func;
            if (implementations [i]->init_v_func)
                implementations [i]->init_v_func (scale_ctx);
        }

        if (!scale_ctx->composite_over_color_func && composite_over_color_func)
            scale_ctx->composite_over_color_func = composite_over_color_func;
        if (!scale_ctx->composite_over_dest_func && composite_over_dest_func)
            scale_ctx->composite_over_dest_func = composite_over_dest_func;
        if (!scale_ctx->clear_dest_func && clear_dest_func)
            scale_ctx->clear_dest_func = clear_dest_func;
    }

    SMOL_ASSERT (scale_ctx->hfilter_func != NULL);
    SMOL_ASSERT (scale_ctx->vfilter_func != NULL);
}

static void
init_dim (SmolDim *dim,
          uint32_t src_size_spx,
          uint32_t dest_size_spx,
          int32_t placement_ofs_spx,
          int32_t placement_size_spx,
          SmolFlags flags,
          SmolStorageType *storage_type_out)
{
    int64_t placement_ofs_px, placement_size_px;
    int64_t visible_first_px, visible_end_px;

    dim->src_size_spx = src_size_spx;
    dim->src_size_px = SMOL_SPX_TO_PX (src_size_spx);
    dim->dest_size_spx = dest_size_spx;
    dim->dest_size_px = SMOL_SPX_TO_PX (dest_size_spx);
    dim->placement_ofs_spx = placement_ofs_spx;
    dim->placement_size_spx = placement_size_spx;

    /* Whole-pixel geometry of the virtual placement, in 64-bit arithmetic;
     * offsets close to INT32_MIN and sizes close to INT32_MAX must not wrap. */

    if (placement_ofs_spx < 0)
        placement_ofs_px = ((int64_t) placement_ofs_spx - 255) / SMOL_SUBPIXEL_MUL;
    else
        placement_ofs_px = placement_ofs_spx / SMOL_SUBPIXEL_MUL;
    placement_size_px = SMOL_SPX_TO_PX ((int64_t) placement_size_spx
                                        + SMOL_SUBPIXEL_MOD (placement_ofs_spx));

    pick_filter_params (dim->src_size_px,
                        dim->src_size_spx,
                        dim->placement_ofs_spx,
                        placement_size_px,
                        dim->placement_size_spx,
                        &dim->n_halvings,
                        &dim->placement_size_prehalving_px,
                        &dim->placement_size_prehalving_spx,
                        &dim->filter_type,
                        storage_type_out,
                        &dim->first_opacity,
                        &dim->last_opacity,
                        flags);

    /* Clip the placement against the destination. An empty intersection
     * yields placement_size_px == 0; smol_scale_init() detects that and
     * neutralizes the placement in both dimensions. */

    visible_first_px = MIN (MAX (placement_ofs_px, 0), (int64_t) dim->dest_size_px);
    visible_end_px = MAX (MIN (placement_ofs_px + placement_size_px,
                               (int64_t) dim->dest_size_px), visible_first_px);

    dim->clear_before_px = visible_first_px;
    dim->clear_after_px = dim->dest_size_px - visible_end_px;
    dim->clip_before_px = visible_first_px - placement_ofs_px;
    dim->clip_after_px = (placement_ofs_px + placement_size_px) - visible_end_px;

    /* Subpixel edge opacity belongs to the virtual placement's fringes; a
     * clipped edge is an interior cut and must stay fully opaque. */

    if (dim->clip_before_px > 0)
        dim->first_opacity = 256;
    if (dim->clip_after_px > 0)
        dim->last_opacity = 256;

    dim->placement_ofs_px = visible_first_px;
    dim->placement_size_px = visible_end_px - visible_first_px;
}

/* Number of precalc entries to reserve for a dimension, in units of two
 * uint16s (box entries are one uint32 each, bilinear samples are an
 * offset/factor uint16 pair). Only the visible window is precalculated. */
static uint32_t
precalc_entries_for_dim (const SmolDim *dim)
{
    return (dim->placement_size_px << dim->n_halvings) + 1;
}

/* Validates the user-facing parameters shared by all entry points. Returns
 * 1 if they're usable, 0 otherwise (whereafter the caller fails gracefully).
 *
 * dest_pixels is intentionally not checked here; the batch APIs allow it to
 * be NULL at init time and supplied later via smol_scale_batch_full(). */
static int
check_scale_params (const void *src_pixels,
                    SmolPixelType src_pixel_type,
                    uint32_t src_width, uint32_t src_height,
                    SmolPixelType dest_pixel_type,
                    uint32_t dest_width, uint32_t dest_height)
{
    if (!src_pixels)
        return 0;

    if ((unsigned int) src_pixel_type >= SMOL_PIXEL_MAX
        || (unsigned int) dest_pixel_type >= SMOL_PIXEL_MAX)
        return 0;

    if (src_width < 1 || src_width > SMOL_DIM_MAX
        || src_height < 1 || src_height > SMOL_DIM_MAX
        || dest_width < 1 || dest_width > SMOL_DIM_MAX
        || dest_height < 1 || dest_height > SMOL_DIM_MAX)
        return 0;

    return 1;
}

/* Quantizes a [0.0, 1.0] layer opacity to 1/256 steps */
static uint16_t
composite_opacity_to_u16 (double opacity)
{
    int o;

    if (opacity < 0.0)
        opacity = 0.0;
    else if (opacity > 1.0)
        opacity = 1.0;

    o = (int) (opacity * SMOL_SUBPIXEL_MUL + 0.5);
    if (o > SMOL_SUBPIXEL_MUL)
        o = SMOL_SUBPIXEL_MUL;

    return (uint16_t) o;
}

static int
smol_scale_init (SmolScaleCtx *scale_ctx,
                 const void *src_pixels,
                 SmolPixelType src_pixel_type,
                 uint32_t src_width_spx,
                 uint32_t src_height_spx,
                 uint32_t src_rowstride,
                 const void *color_pixel,
                 SmolPixelType color_pixel_type,
                 void *dest_pixels,
                 SmolPixelType dest_pixel_type,
                 uint32_t dest_width_spx,
                 uint32_t dest_height_spx,
                 uint32_t dest_rowstride,
                 int32_t placement_x_spx,
                 int32_t placement_y_spx,
                 int32_t placement_width_spx,
                 int32_t placement_height_spx,
                 SmolCompositeOp composite_op,
                 double composite_opacity,
                 SmolFlags flags,
                 SmolPostRowFunc post_row_func,
                 void *user_data)
{
    SmolStorageType storage_type [2];

    if (placement_width_spx <= 0 || placement_height_spx <= 0)
    {
        placement_width_spx = 0;
        placement_height_spx = 0;
        placement_x_spx = 0;
        placement_y_spx = 0;
    }

    scale_ctx->src_pixels = src_pixels;
    scale_ctx->src_pixel_type = src_pixel_type;
    scale_ctx->src_rowstride = src_rowstride;

    scale_ctx->dest_pixels = dest_pixels;
    scale_ctx->dest_pixel_type = dest_pixel_type;
    scale_ctx->dest_rowstride = dest_rowstride;

    scale_ctx->composite_op = composite_op;
    scale_ctx->composite_opacity = composite_opacity_to_u16 (composite_opacity);
    scale_ctx->flags = flags;
    scale_ctx->gamma_type = (flags & SMOL_DISABLE_SRGB_LINEARIZATION)
        ? SMOL_GAMMA_SRGB_COMPRESSED : SMOL_GAMMA_SRGB_LINEAR;

    scale_ctx->post_row_func = post_row_func;
    scale_ctx->user_data = user_data;

    init_dim (&scale_ctx->hdim,
              src_width_spx, dest_width_spx,
              placement_x_spx, placement_width_spx,
              flags, &storage_type [0]);
    init_dim (&scale_ctx->vdim,
              src_height_spx, dest_height_spx,
              placement_y_spx, placement_height_spx,
              flags, &storage_type [1]);

    /* A placement with no visible extent in either dimension draws nothing;
     * neutralize it to zero size so the pipeline treats it as a no-op
     * (SMOL_CLEAR_DEST still clears the destination). */
    if (scale_ctx->hdim.placement_size_px == 0 || scale_ctx->vdim.placement_size_px == 0)
    {
        init_dim (&scale_ctx->hdim,
                  src_width_spx, dest_width_spx,
                  0, 0,
                  flags, &storage_type [0]);
        init_dim (&scale_ctx->vdim,
                  src_height_spx, dest_height_spx,
                  0, 0,
                  flags, &storage_type [1]);
    }

    scale_ctx->storage_type = MAX (storage_type [0], storage_type [1]);
    scale_ctx->hdim.precalc = smol_alloc_aligned ((precalc_entries_for_dim (&scale_ctx->hdim)
                                                   + precalc_entries_for_dim (&scale_ctx->vdim)) * 2
                                                  * sizeof (uint16_t),
                                                  &scale_ctx->precalc_storage);
    if (!scale_ctx->precalc_storage)
        return 0;

    scale_ctx->vdim.precalc = ((uint16_t *) scale_ctx->hdim.precalc)
        + precalc_entries_for_dim (&scale_ctx->hdim) * 2;

    get_implementations (scale_ctx, color_pixel, color_pixel_type);
    return 1;
}

static void
smol_scale_finalize (SmolScaleCtx *scale_ctx)
{
    free (scale_ctx->precalc_storage);
}

/* SmolScaleCtx must be aligned */
static SmolScaleCtx *
alloc_scale_ctx (void)
{
    SmolScaleCtx *scale_ctx;
    void *storage;

    scale_ctx = smol_alloc_aligned (sizeof (SmolScaleCtx), &storage);
    if (!storage)
        return NULL;

    memset (scale_ctx, 0, sizeof (SmolScaleCtx));
    scale_ctx->self_storage = storage;
    return scale_ctx;
}

/* ---------- *
 * Public API *
 * ---------- */

SmolScaleCtx *
smol_scale_new_simple (const void *src_pixels,
                       SmolPixelType src_pixel_type,
                       uint32_t src_width,
                       uint32_t src_height,
                       uint32_t src_rowstride,
                       void *dest_pixels,
                       SmolPixelType dest_pixel_type,
                       uint32_t dest_width,
                       uint32_t dest_height,
                       uint32_t dest_rowstride,
                       SmolFlags flags)
{
    SmolScaleCtx *scale_ctx;

    if (!check_scale_params (src_pixels, src_pixel_type, src_width, src_height,
                             dest_pixel_type, dest_width, dest_height))
        return NULL;

    scale_ctx = alloc_scale_ctx ();
    if (!scale_ctx)
        return NULL;

    if (!smol_scale_init (scale_ctx,
                          src_pixels,
                          src_pixel_type,
                          SMOL_PX_TO_SPX (src_width),
                          SMOL_PX_TO_SPX (src_height),
                          src_rowstride,
                          NULL,
                          0,
                          dest_pixels,
                          dest_pixel_type,
                          SMOL_PX_TO_SPX (dest_width),
                          SMOL_PX_TO_SPX (dest_height),
                          dest_rowstride,
                          0,
                          0,
                          SMOL_PX_TO_SPX (dest_width),
                          SMOL_PX_TO_SPX (dest_height),
                          SMOL_COMPOSITE_SRC_OVER_COLOR,
                          1.0,
                          flags,
                          NULL,
                          NULL))
    {
        free (scale_ctx);
        return NULL;
    }

    return scale_ctx;
}

int
smol_scale_simple (const void *src_pixels,
                   SmolPixelType src_pixel_type,
                   uint32_t src_width,
                   uint32_t src_height,
                   uint32_t src_rowstride,
                   void *dest_pixels,
                   SmolPixelType dest_pixel_type,
                   uint32_t dest_width,
                   uint32_t dest_height,
                   uint32_t dest_rowstride,
                   SmolFlags flags)
{
    SMOL_ALIGN SmolScaleCtx scale_ctx = { 0 };
    int first_row, n_rows;
    int result = 0;

    if (!check_scale_params (src_pixels, src_pixel_type, src_width, src_height,
                             dest_pixel_type, dest_width, dest_height))
        return 0;

    if (!smol_scale_init (&scale_ctx,
                          src_pixels,
                          src_pixel_type,
                          SMOL_PX_TO_SPX (src_width),
                          SMOL_PX_TO_SPX (src_height),
                          src_rowstride,
                          NULL,
                          0,
                          dest_pixels,
                          dest_pixel_type,
                          SMOL_PX_TO_SPX (dest_width),
                          SMOL_PX_TO_SPX (dest_height),
                          dest_rowstride,
                          0,
                          0,
                          SMOL_PX_TO_SPX (dest_width),
                          SMOL_PX_TO_SPX (dest_height),
                          SMOL_COMPOSITE_SRC_OVER_COLOR,
                          1.0,
                          flags,
                          NULL, NULL))
    {
        return 0;
    }

    first_row = 0;
    n_rows = scale_ctx.vdim.dest_size_px;

    if (check_row_range (&scale_ctx, &first_row, &n_rows))
    {
        result = do_rows (&scale_ctx,
                          dest_row_ofs_to_pointer (&scale_ctx, 0),
                          first_row,
                          n_rows);
    }
    else
    {
        result = 1;
    }

    smol_scale_finalize (&scale_ctx);
    return result;
}

SmolScaleCtx *
smol_scale_new_full (const void *src_pixels,
                     SmolPixelType src_pixel_type,
                     uint32_t src_width,
                     uint32_t src_height,
                     uint32_t src_rowstride,
                     const void *color_pixel,
                     SmolPixelType color_pixel_type,
                     void *dest_pixels,
                     SmolPixelType dest_pixel_type,
                     uint32_t dest_width,
                     uint32_t dest_height,
                     uint32_t dest_rowstride,
                     int32_t placement_x,
                     int32_t placement_y,
                     uint32_t placement_width,
                     uint32_t placement_height,
                     SmolCompositeOp composite_op,
                     double composite_opacity,
                     SmolFlags flags,
                     SmolPostRowFunc post_row_func,
                     void *user_data)
{
    SmolScaleCtx *scale_ctx;

    if (!check_scale_params (src_pixels, src_pixel_type, src_width, src_height,
                             dest_pixel_type, dest_width, dest_height))
        return NULL;

    scale_ctx = alloc_scale_ctx ();
    if (!scale_ctx)
        return NULL;

    if (!smol_scale_init (scale_ctx,
                          src_pixels,
                          src_pixel_type,
                          SMOL_PX_TO_SPX (src_width),
                          SMOL_PX_TO_SPX (src_height),
                          src_rowstride,
                          color_pixel,
                          color_pixel_type,
                          dest_pixels,
                          dest_pixel_type,
                          SMOL_PX_TO_SPX (dest_width),
                          SMOL_PX_TO_SPX (dest_height),
                          dest_rowstride,
                          placement_x,
                          placement_y,
                          /* Clamp to signed range */
                          MIN (placement_width, (uint32_t) INT32_MAX),
                          MIN (placement_height, (uint32_t) INT32_MAX),
                          composite_op,
                          composite_opacity,
                          flags,
                          post_row_func,
                          user_data))
    {
        free (scale_ctx);
        return NULL;
    }

    return scale_ctx;
}

void
smol_scale_destroy (SmolScaleCtx *scale_ctx)
{
    smol_scale_finalize (scale_ctx);
    free (scale_ctx->self_storage);
}

int
smol_scale_batch (const SmolScaleCtx *scale_ctx,
                  int32_t first_dest_row,
                  int32_t n_dest_rows)
{
    if (!check_row_range (scale_ctx, &first_dest_row, &n_dest_rows))
        return 1;

    return do_rows (scale_ctx,
                    dest_row_ofs_to_pointer (scale_ctx, first_dest_row),
                    first_dest_row,
                    n_dest_rows);
}

int
smol_scale_batch_full (const SmolScaleCtx *scale_ctx,
                       void *dest,
                       int32_t first_dest_row,
                       int32_t n_dest_rows)
{
    if (!check_row_range (scale_ctx, &first_dest_row, &n_dest_rows))
        return 1;

    return do_rows (scale_ctx,
                    dest,
                    first_dest_row,
                    n_dest_rows);
}
