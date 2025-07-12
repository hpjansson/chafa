/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* Copyright © 2019-2025 Hans Petter Jansson. See COPYING for details. */

#include <assert.h> /* assert */
#include <stdlib.h> /* malloc, free, alloca */
#include <string.h> /* memset */
#include <limits.h>
#include "smolscale-private.h"

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
    { { 1, 2, 3, 4 }, { 3, 2, 4, 1 } }
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
 * As a side effect, the values in the lower range (first 35 indexes) are
 * off by < 2%. */

const uint16_t _smol_from_srgb_lut [256] =
{
       0,    1,    2,    3,    4,    5,    6,    7,    8,    9,   10,   11, 
      12,   13,   14,   15,   16,   17,   18,   19,   20,   21,   22,   23, 
      24,   25,   26,   27,   28,   29,   30,   31,   32,   33,   34,   35, 
      37,   39,   41,   43,   45,   47,   49,   51,   53,   55,   57,   59, 
      62,   64,   67,   69,   72,   74,   77,   79,   82,   85,   88,   91, 
      94,   97,  100,  103,  106,  109,  113,  116,  119,  123,  126,  130, 
     134,  137,  141,  145,  149,  153,  157,  161,  165,  169,  174,  178, 
     182,  187,  191,  196,  201,  205,  210,  215,  220,  225,  230,  235, 
     240,  246,  251,  256,  262,  267,  273,  279,  284,  290,  296,  302, 
     308,  314,  320,  326,  333,  339,  345,  352,  359,  365,  372,  379, 
     385,  392,  399,  406,  414,  421,  428,  435,  443,  450,  458,  466, 
     473,  481,  489,  497,  505,  513,  521,  530,  538,  546,  555,  563, 
     572,  581,  589,  598,  607,  616,  625,  634,  644,  653,  662,  672, 
     682,  691,  701,  711,  721,  731,  741,  751,  761,  771,  782,  792, 
     803,  813,  824,  835,  845,  856,  867,  879,  890,  901,  912,  924, 
     935,  947,  959,  970,  982,  994, 1006, 1018, 1030, 1043, 1055, 1067, 
    1080, 1093, 1105, 1118, 1131, 1144, 1157, 1170, 1183, 1197, 1210, 1223, 
    1237, 1251, 1264, 1278, 1292, 1306, 1320, 1334, 1349, 1363, 1377, 1392, 
    1407, 1421, 1436, 1451, 1466, 1481, 1496, 1512, 1527, 1542, 1558, 1573, 
    1589, 1605, 1621, 1637, 1653, 1669, 1685, 1702, 1718, 1735, 1751, 1768, 
    1785, 1802, 1819, 1836, 1853, 1870, 1887, 1905, 1922, 1940, 1958, 1976, 
    1994, 2012, 2030, 2047
};

const uint8_t _smol_to_srgb_lut [SRGB_LINEAR_MAX] =
{
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13, 
     14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27, 
     28,  29,  30,  31,  32,  33,  34,  35,  36,  36,  37,  37,  38,  38, 
     39,  39,  40,  40,  41,  41,  42,  42,  43,  43,  44,  44,  45,  45, 
     46,  46,  47,  47,  47,  48,  48,  49,  49,  49,  50,  50,  51,  51, 
     51,  52,  52,  53,  53,  53,  54,  54,  55,  55,  55,  56,  56,  56, 
     57,  57,  57,  58,  58,  58,  59,  59,  59,  60,  60,  60,  61,  61, 
     61,  62,  62,  62,  63,  63,  63,  64,  64,  64,  65,  65,  65,  65, 
     66,  66,  66,  67,  67,  67,  68,  68,  68,  68,  69,  69,  69,  70, 
     70,  70,  70,  71,  71,  71,  71,  72,  72,  72,  73,  73,  73,  73, 
     74,  74,  74,  74,  75,  75,  75,  75,  76,  76,  76,  76,  77,  77, 
     77,  77,  78,  78,  78,  78,  79,  79,  79,  79,  80,  80,  80,  80, 
     81,  81,  81,  81,  81,  82,  82,  82,  82,  83,  83,  83,  83,  84, 
     84,  84,  84,  84,  85,  85,  85,  85,  86,  86,  86,  86,  86,  87, 
     87,  87,  87,  88,  88,  88,  88,  88,  89,  89,  89,  89,  89,  90, 
     90,  90,  90,  90,  91,  91,  91,  91,  91,  92,  92,  92,  92,  92, 
     93,  93,  93,  93,  93,  94,  94,  94,  94,  94,  95,  95,  95,  95, 
     95,  96,  96,  96,  96,  96,  97,  97,  97,  97,  97,  98,  98,  98, 
     98,  98,  98,  99,  99,  99,  99,  99, 100, 100, 100, 100, 100, 100, 
    101, 101, 101, 101, 101, 102, 102, 102, 102, 102, 102, 103, 103, 103, 
    103, 103, 103, 104, 104, 104, 104, 104, 105, 105, 105, 105, 105, 105, 
    106, 106, 106, 106, 106, 106, 107, 107, 107, 107, 107, 107, 108, 108, 
    108, 108, 108, 108, 109, 109, 109, 109, 109, 109, 110, 110, 110, 110, 
    110, 110, 110, 111, 111, 111, 111, 111, 111, 112, 112, 112, 112, 112, 
    112, 113, 113, 113, 113, 113, 113, 113, 114, 114, 114, 114, 114, 114, 
    115, 115, 115, 115, 115, 115, 115, 116, 116, 116, 116, 116, 116, 117, 
    117, 117, 117, 117, 117, 117, 118, 118, 118, 118, 118, 118, 118, 119, 
    119, 119, 119, 119, 119, 120, 120, 120, 120, 120, 120, 120, 121, 121, 
    121, 121, 121, 121, 121, 122, 122, 122, 122, 122, 122, 122, 123, 123, 
    123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 124, 125, 
    125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 127, 
    127, 127, 127, 127, 127, 127, 128, 128, 128, 128, 128, 128, 128, 128, 
    129, 129, 129, 129, 129, 129, 129, 129, 130, 130, 130, 130, 130, 130, 
    130, 131, 131, 131, 131, 131, 131, 131, 131, 132, 132, 132, 132, 132, 
    132, 132, 132, 133, 133, 133, 133, 133, 133, 133, 134, 134, 134, 134, 
    134, 134, 134, 134, 135, 135, 135, 135, 135, 135, 135, 135, 136, 136, 
    136, 136, 136, 136, 136, 136, 137, 137, 137, 137, 137, 137, 137, 137, 
    137, 138, 138, 138, 138, 138, 138, 138, 138, 139, 139, 139, 139, 139, 
    139, 139, 139, 140, 140, 140, 140, 140, 140, 140, 140, 141, 141, 141, 
    141, 141, 141, 141, 141, 141, 142, 142, 142, 142, 142, 142, 142, 142, 
    143, 143, 143, 143, 143, 143, 143, 143, 143, 144, 144, 144, 144, 144, 
    144, 144, 144, 144, 145, 145, 145, 145, 145, 145, 145, 145, 146, 146, 
    146, 146, 146, 146, 146, 146, 146, 147, 147, 147, 147, 147, 147, 147, 
    147, 147, 148, 148, 148, 148, 148, 148, 148, 148, 148, 149, 149, 149, 
    149, 149, 149, 149, 149, 149, 150, 150, 150, 150, 150, 150, 150, 150, 
    150, 151, 151, 151, 151, 151, 151, 151, 151, 151, 152, 152, 152, 152, 
    152, 152, 152, 152, 152, 152, 153, 153, 153, 153, 153, 153, 153, 153, 
    153, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 155, 155, 155, 
    155, 155, 155, 155, 155, 155, 156, 156, 156, 156, 156, 156, 156, 156, 
    156, 156, 157, 157, 157, 157, 157, 157, 157, 157, 157, 158, 158, 158, 
    158, 158, 158, 158, 158, 158, 158, 159, 159, 159, 159, 159, 159, 159, 
    159, 159, 159, 160, 160, 160, 160, 160, 160, 160, 160, 160, 160, 161, 
    161, 161, 161, 161, 161, 161, 161, 161, 161, 162, 162, 162, 162, 162, 
    162, 162, 162, 162, 162, 163, 163, 163, 163, 163, 163, 163, 163, 163, 
    163, 164, 164, 164, 164, 164, 164, 164, 164, 164, 164, 165, 165, 165, 
    165, 165, 165, 165, 165, 165, 165, 165, 166, 166, 166, 166, 166, 166, 
    166, 166, 166, 166, 167, 167, 167, 167, 167, 167, 167, 167, 167, 167, 
    167, 168, 168, 168, 168, 168, 168, 168, 168, 168, 168, 169, 169, 169, 
    169, 169, 169, 169, 169, 169, 169, 169, 170, 170, 170, 170, 170, 170, 
    170, 170, 170, 170, 170, 171, 171, 171, 171, 171, 171, 171, 171, 171, 
    171, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 172, 173, 173, 
    173, 173, 173, 173, 173, 173, 173, 173, 173, 174, 174, 174, 174, 174, 
    174, 174, 174, 174, 174, 174, 175, 175, 175, 175, 175, 175, 175, 175, 
    175, 175, 175, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 176, 
    176, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 177, 178, 178, 
    178, 178, 178, 178, 178, 178, 178, 178, 178, 179, 179, 179, 179, 179, 
    179, 179, 179, 179, 179, 179, 179, 180, 180, 180, 180, 180, 180, 180, 
    180, 180, 180, 180, 181, 181, 181, 181, 181, 181, 181, 181, 181, 181, 
    181, 181, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 182, 
    183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 183, 184, 184, 184, 
    184, 184, 184, 184, 184, 184, 184, 184, 184, 185, 185, 185, 185, 185, 
    185, 185, 185, 185, 185, 185, 185, 186, 186, 186, 186, 186, 186, 186, 
    186, 186, 186, 186, 186, 187, 187, 187, 187, 187, 187, 187, 187, 187, 
    187, 187, 187, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 188, 
    188, 188, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 189, 
    190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 190, 191, 191, 
    191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 191, 192, 192, 192, 
    192, 192, 192, 192, 192, 192, 192, 192, 192, 193, 193, 193, 193, 193, 
    193, 193, 193, 193, 193, 193, 193, 193, 194, 194, 194, 194, 194, 194, 
    194, 194, 194, 194, 194, 194, 194, 195, 195, 195, 195, 195, 195, 195, 
    195, 195, 195, 195, 195, 195, 196, 196, 196, 196, 196, 196, 196, 196, 
    196, 196, 196, 196, 197, 197, 197, 197, 197, 197, 197, 197, 197, 197, 
    197, 197, 197, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 198, 
    198, 198, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 199, 
    199, 199, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 
    200, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 201, 
    202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 202, 
    203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 204, 
    204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 204, 205, 
    205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 206, 206, 
    206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 206, 207, 207, 
    207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 207, 208, 208, 
    208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 208, 209, 209, 
    209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 209, 210, 210, 
    210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 210, 211, 211, 
    211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 211, 212, 212, 
    212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 212, 213, 
    213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 213, 214, 
    214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 214, 215, 
    215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 
    216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 216, 
    216, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 217, 
    217, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 218, 
    218, 218, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 219, 
    219, 219, 219, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 220, 
    220, 220, 220, 220, 221, 221, 221, 221, 221, 221, 221, 221, 221, 221, 
    221, 221, 221, 221, 221, 222, 222, 222, 222, 222, 222, 222, 222, 222, 
    222, 222, 222, 222, 222, 222, 223, 223, 223, 223, 223, 223, 223, 223, 
    223, 223, 223, 223, 223, 223, 223, 223, 224, 224, 224, 224, 224, 224, 
    224, 224, 224, 224, 224, 224, 224, 224, 224, 225, 225, 225, 225, 225, 
    225, 225, 225, 225, 225, 225, 225, 225, 225, 225, 226, 226, 226, 226, 
    226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 226, 227, 227, 
    227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 227, 
    228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 228, 
    228, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 229, 
    229, 229, 229, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 230, 
    230, 230, 230, 230, 230, 231, 231, 231, 231, 231, 231, 231, 231, 231, 
    231, 231, 231, 231, 231, 231, 231, 232, 232, 232, 232, 232, 232, 232, 
    232, 232, 232, 232, 232, 232, 232, 232, 232, 233, 233, 233, 233, 233, 
    233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 233, 234, 234, 234, 
    234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 234, 
    235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 235, 
    235, 235, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 236, 
    236, 236, 236, 236, 237, 237, 237, 237, 237, 237, 237, 237, 237, 237, 
    237, 237, 237, 237, 237, 237, 237, 238, 238, 238, 238, 238, 238, 238, 
    238, 238, 238, 238, 238, 238, 238, 238, 238, 238, 239, 239, 239, 239, 
    239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 239, 240, 240, 
    240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 240, 
    240, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 241, 
    241, 241, 241, 241, 242, 242, 242, 242, 242, 242, 242, 242, 242, 242, 
    242, 242, 242, 242, 242, 242, 242, 243, 243, 243, 243, 243, 243, 243, 
    243, 243, 243, 243, 243, 243, 243, 243, 243, 243, 244, 244, 244, 244, 
    244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 245, 
    245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 245, 
    245, 245, 245, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 246, 
    246, 246, 246, 246, 246, 246, 247, 247, 247, 247, 247, 247, 247, 247, 
    247, 247, 247, 247, 247, 247, 247, 247, 247, 248, 248, 248, 248, 248, 
    248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 248, 249, 
    249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 249, 
    249, 249, 249, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 
    250, 250, 250, 250, 250, 250, 251, 251, 251, 251, 251, 251, 251, 251, 
    251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 252, 252, 252, 252, 
    252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 
    253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 
    253, 253, 253, 253, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 
    254, 254, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 
    255, 255, 255, 255
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

/* Lossy premultiplication: 8-bit * alpha -> 8-bit. Not perfectly reversible. */
const uint32_t _smol_inv_div_p8_lut [256] =
{
    0x00000000, 0x00181fff, 0x000e2fff, 0x0009f555, 0x0007a7ff, 0x00063333, 0x00052555, 0x00047999,
    0x0003ebff, 0x0003838e, 0x00032333, 0x0002e2e8, 0x0002a2aa, 0x0002713b, 0x00024249, 0x00021ccc,
    0x0001f924, 0x0001dd17, 0x0001c1c7, 0x0001ab4b, 0x000195e5, 0x0001830c, 0x000170c3, 0x00016164,
    0x0001537a, 0x0001450d, 0x0001390b, 0x00012de9, 0x00012249, 0x00011846, 0x00010eaa, 0x0001069e,
    0x0000fd70, 0x0000f6aa, 0x0000eedb, 0x0000e8f5, 0x0000e1c7, 0x0000db8e, 0x0000d638, 0x0000d069,
    0x0000cb7c, 0x0000c690, 0x0000c186, 0x0000bd2d, 0x0000b8f9, 0x0000b4f7, 0x0000b0ff, 0x0000ad65,
    0x0000a9ac, 0x0000a687, 0x0000a286, 0x00009f33, 0x00009c98, 0x000099b9, 0x000096f1, 0x00009414,
    0x00009147, 0x00008efa, 0x00008c59, 0x00008a0a, 0x000087b4, 0x0000856c, 0x00008341, 0x0000818c,
    0x00007f55, 0x00007d60, 0x00007b7f, 0x000079b2, 0x000077b9, 0x00007608, 0x0000743c, 0x000072b5,
    0x0000711a, 0x00006fac, 0x00006e1a, 0x00006cad, 0x00006b17, 0x000069e1, 0x00006864, 0x00006736,
    0x000065db, 0x000064b1, 0x00006357, 0x00006250, 0x000060c5, 0x00006060, 0x00005ec0, 0x00005da5,
    0x00005c9b, 0x00005b8b, 0x00005a93, 0x000059ab, 0x00005884, 0x00005799, 0x000056ae, 0x000055d5,
    0x000054e2, 0x0000540b, 0x00005343, 0x00005255, 0x0000517c, 0x000050a7, 0x00004fff, 0x00004f2c,
    0x00004e5e, 0x00004d9f, 0x00004cec, 0x00004c34, 0x00004b78, 0x00004adc, 0x00004a23, 0x00004981,
    0x000048ce, 0x00004836, 0x0000478c, 0x000046eb, 0x00004656, 0x000045b6, 0x00004524, 0x0000449c,
    0x000043ff, 0x00004370, 0x000042e2, 0x00004257, 0x000041ce, 0x00004147, 0x000040c3, 0x00004081,
    0x00003fff, 0x00003f57, 0x00003ed3, 0x00003e54, 0x00003dd9, 0x00003d60, 0x00003ced, 0x00003c78,
    0x00003c07, 0x00003b9a, 0x00003b26, 0x00003abf, 0x00003a4f, 0x000039e1, 0x0000397e, 0x00003917,
    0x000038af, 0x00003848, 0x000037ee, 0x00003787, 0x00003726, 0x000036c9, 0x0000366b, 0x0000360d,
    0x000035b0, 0x00003567, 0x00003503, 0x000034aa, 0x00003453, 0x000033ff, 0x000033a8, 0x0000335c,
    0x00003305, 0x000032b3, 0x00003266, 0x00003213, 0x000031c7, 0x00003178, 0x0000312b, 0x000030df,
    0x00003094, 0x00003049, 0x00003018, 0x00002fc0, 0x00002f76, 0x00002f2d, 0x00002ee8, 0x00002ea6,
    0x00002e5f, 0x00002e1c, 0x00002dd9, 0x00002d99, 0x00002d59, 0x00002d17, 0x00002cdf, 0x00002c9b,
    0x00002c5d, 0x00002c1c, 0x00002be1, 0x00002ba6, 0x00002b6a, 0x00002b2e, 0x00002af3, 0x00002ac7,
    0x00002a85, 0x00002a4a, 0x00002a11, 0x000029dc, 0x000029a6, 0x0000296e, 0x00002936, 0x00002904,
    0x000028cd, 0x0000289a, 0x00002866, 0x00002833, 0x0000280a, 0x000027d0, 0x0000279e, 0x0000276f,
    0x0000273c, 0x0000270d, 0x000026de, 0x000026ad, 0x0000267e, 0x00002652, 0x00002622, 0x000025f5,
    0x000025c9, 0x0000259b, 0x0000256f, 0x00002545, 0x00002518, 0x000024ef, 0x000024c3, 0x0000249c,
    0x0000246f, 0x00002446, 0x0000241c, 0x000023f4, 0x000023ca, 0x000023a2, 0x0000237b, 0x00002354,
    0x0000232e, 0x00002306, 0x000022e0, 0x000022b9, 0x00002294, 0x0000226f, 0x0000224b, 0x00002226,
    0x00002202, 0x000021dc, 0x000021b8, 0x00002195, 0x00002172, 0x0000214f, 0x0000212c, 0x0000210a,
    0x000020e7, 0x000020c5, 0x000020a4, 0x00002083, 0x00002061, 0x00002041, 0x00002020, 0x00002020
};

/* Lossy premultiplication: 11-bit * alpha -> 11-bit. Not perfectly reversible. */
const uint32_t _smol_inv_div_p8l_lut [256] =
{
    0x00000000, 0x0007ffff, 0x0003ffff, 0x0002aaaa, 0x0001ffff, 0x00019999, 0x00015555, 0x00012492,
    0x0000ffff, 0x0000e38e, 0x0000cccc, 0x0000ba2e, 0x0000aaaa, 0x00009d89, 0x00009249, 0x00008888,
    0x00007fff, 0x00007878, 0x000071c7, 0x00006bca, 0x00006666, 0x00006186, 0x00005d17, 0x0000590b,
    0x00005555, 0x000051eb, 0x00004ec4, 0x00004bda, 0x00004924, 0x0000469e, 0x00004444, 0x00004210,
    0x00003fff, 0x00003e0f, 0x00003c3c, 0x00003a83, 0x000038e3, 0x0000372a, 0x000035b7, 0x00003458,
    0x0000330a, 0x000031cc, 0x0000309e, 0x00002f7d, 0x00002e69, 0x00002d62, 0x00002c66, 0x00002b75,
    0x00002a8e, 0x000029b0, 0x000028db, 0x0000280f, 0x0000274a, 0x0000268c, 0x000025d6, 0x00002526,
    0x0000247d, 0x000023d9, 0x0000233c, 0x000022a3, 0x0000220f, 0x00002181, 0x000020f7, 0x00002071,
    0x00001ff0, 0x00001f72, 0x00001ef8, 0x00001e82, 0x00001e0f, 0x00001da0, 0x00001d34, 0x00001ccb,
    0x00001c65, 0x00001bf5, 0x00001b95, 0x00001b37, 0x00001adb, 0x00001a82, 0x00001a2c, 0x000019d7,
    0x00001985, 0x00001934, 0x000018e6, 0x00001899, 0x0000184f, 0x00001806, 0x000017be, 0x00001779,
    0x00001734, 0x000016f2, 0x000016b1, 0x00001671, 0x00001633, 0x000015f6, 0x000015ba, 0x00001580,
    0x00001547, 0x0000150f, 0x000014d8, 0x000014a2, 0x0000146d, 0x0000143a, 0x00001407, 0x000013d5,
    0x000013a5, 0x00001375, 0x00001346, 0x00001318, 0x000012eb, 0x000012be, 0x0000128e, 0x00001263,
    0x00001239, 0x00001210, 0x000011e7, 0x000011c0, 0x00001199, 0x00001172, 0x0000114d, 0x00001127,
    0x00001103, 0x000010df, 0x000010bc, 0x00001099, 0x00001077, 0x00001055, 0x00001034, 0x00001014,
    0x00000ff4, 0x00000fd4, 0x00000fb5, 0x00000f96, 0x00000f78, 0x00000f5a, 0x00000f3d, 0x00000f20,
    0x00000f04, 0x00000ee8, 0x00000ecc, 0x00000eb1, 0x00000e96, 0x00000e7c, 0x00000e62, 0x00000e48,
    0x00000e2f, 0x00000e16, 0x00000dfa, 0x00000de2, 0x00000dca, 0x00000db2, 0x00000d9b, 0x00000d84,
    0x00000d6d, 0x00000d57, 0x00000d41, 0x00000d2b, 0x00000d16, 0x00000d00, 0x00000ceb, 0x00000cd7,
    0x00000cc2, 0x00000cae, 0x00000c9a, 0x00000c86, 0x00000c73, 0x00000c5f, 0x00000c4c, 0x00000c3a,
    0x00000c27, 0x00000c15, 0x00000c03, 0x00000bf1, 0x00000bdf, 0x00000bcd, 0x00000bbc, 0x00000bab,
    0x00000b9a, 0x00000b89, 0x00000b79, 0x00000b68, 0x00000b58, 0x00000b48, 0x00000b38, 0x00000b27,
    0x00000b17, 0x00000b08, 0x00000af9, 0x00000aea, 0x00000adb, 0x00000acc, 0x00000abe, 0x00000ab0,
    0x00000aa1, 0x00000a93, 0x00000a85, 0x00000a78, 0x00000a6a, 0x00000a5c, 0x00000a4f, 0x00000a42,
    0x00000a35, 0x00000a28, 0x00000a1b, 0x00000a0e, 0x00000a02, 0x000009f5, 0x000009e9, 0x000009dd,
    0x000009d1, 0x000009c5, 0x000009b9, 0x000009ad, 0x000009a1, 0x00000996, 0x0000098a, 0x0000097f,
    0x00000974, 0x00000969, 0x0000095e, 0x00000951, 0x00000947, 0x0000093c, 0x00000931, 0x00000927,
    0x0000091c, 0x00000912, 0x00000908, 0x000008fe, 0x000008f3, 0x000008e9, 0x000008e0, 0x000008d6,
    0x000008cc, 0x000008c2, 0x000008b9, 0x000008af, 0x000008a6, 0x0000089d, 0x00000893, 0x0000088a,
    0x00000881, 0x00000878, 0x0000086f, 0x00000866, 0x0000085e, 0x00000855, 0x0000084c, 0x00000844,
    0x0000083b, 0x00000833, 0x0000082a, 0x00000822, 0x0000081a, 0x00000812, 0x0000080a, 0x00000801
};

/* Lossless premultiplication: 8-bit * alpha -> 16-bit. Reversible with this table. */
const uint32_t _smol_inv_div_p16_lut [256] =
{
    0x00000000, 0x00005556, 0x00004000, 0x00003334, 0x00002aab, 0x00002493, 0x00002000, 0x00001c72,
    0x0000199a, 0x00001746, 0x00001556, 0x000013b2, 0x0000124a, 0x00001112, 0x00001000, 0x00000f10,
    0x00000e39, 0x00000d7a, 0x00000ccd, 0x00000c31, 0x00000ba3, 0x00000b22, 0x00000aab, 0x00000a3e,
    0x000009d9, 0x0000097c, 0x00000925, 0x000008d4, 0x00000889, 0x00000843, 0x00000800, 0x000007c2,
    0x00000788, 0x00000751, 0x0000071d, 0x000006ec, 0x000006bd, 0x00000691, 0x00000667, 0x0000063f,
    0x00000619, 0x000005f5, 0x000005d2, 0x000005b1, 0x00000591, 0x00000573, 0x00000556, 0x0000053a,
    0x0000051f, 0x00000506, 0x000004ed, 0x000004d5, 0x000004be, 0x000004a8, 0x00000493, 0x0000047e,
    0x0000046a, 0x00000457, 0x00000445, 0x00000433, 0x00000422, 0x00000411, 0x00000400, 0x000003f1,
    0x000003e1, 0x000003d3, 0x000003c4, 0x000003b6, 0x000003a9, 0x0000039c, 0x0000038f, 0x00000382,
    0x00000376, 0x0000036a, 0x0000035f, 0x00000354, 0x00000349, 0x0000033e, 0x00000334, 0x0000032a,
    0x00000320, 0x00000316, 0x0000030d, 0x00000304, 0x000002fb, 0x000002f2, 0x000002e9, 0x000002e1,
    0x000002d9, 0x000002d1, 0x000002c9, 0x000002c1, 0x000002ba, 0x000002b2, 0x000002ab, 0x000002a4,
    0x0000029d, 0x00000296, 0x00000290, 0x00000289, 0x00000283, 0x0000027d, 0x00000277, 0x00000271,
    0x0000026b, 0x00000265, 0x0000025f, 0x0000025a, 0x00000254, 0x0000024f, 0x0000024a, 0x00000244,
    0x0000023f, 0x0000023a, 0x00000235, 0x00000231, 0x0000022c, 0x00000227, 0x00000223, 0x0000021e,
    0x0000021a, 0x00000215, 0x00000211, 0x0000020d, 0x00000209, 0x00000205, 0x00000200, 0x000001fd,
    0x000001f9, 0x000001f5, 0x000001f1, 0x000001ed, 0x000001ea, 0x000001e6, 0x000001e2, 0x000001df,
    0x000001db, 0x000001d8, 0x000001d5, 0x000001d1, 0x000001ce, 0x000001cb, 0x000001c8, 0x000001c4,
    0x000001c1, 0x000001be, 0x000001bb, 0x000001b8, 0x000001b5, 0x000001b3, 0x000001b0, 0x000001ad,
    0x000001aa, 0x000001a7, 0x000001a5, 0x000001a2, 0x0000019f, 0x0000019d, 0x0000019a, 0x00000198,
    0x00000195, 0x00000193, 0x00000190, 0x0000018e, 0x0000018b, 0x00000189, 0x00000187, 0x00000184,
    0x00000182, 0x00000180, 0x0000017e, 0x0000017b, 0x00000179, 0x00000177, 0x00000175, 0x00000173,
    0x00000171, 0x0000016f, 0x0000016d, 0x0000016b, 0x00000169, 0x00000167, 0x00000165, 0x00000163,
    0x00000161, 0x0000015f, 0x0000015d, 0x0000015b, 0x00000159, 0x00000158, 0x00000156, 0x00000154,
    0x00000152, 0x00000151, 0x0000014f, 0x0000014d, 0x0000014b, 0x0000014a, 0x00000148, 0x00000147,
    0x00000145, 0x00000143, 0x00000142, 0x00000140, 0x0000013f, 0x0000013d, 0x0000013c, 0x0000013a,
    0x00000139, 0x00000137, 0x00000136, 0x00000134, 0x00000133, 0x00000131, 0x00000130, 0x0000012f,
    0x0000012d, 0x0000012c, 0x0000012a, 0x00000129, 0x00000128, 0x00000126, 0x00000125, 0x00000124,
    0x00000122, 0x00000121, 0x00000120, 0x0000011f, 0x0000011d, 0x0000011c, 0x0000011b, 0x0000011a,
    0x00000119, 0x00000117, 0x00000116, 0x00000115, 0x00000114, 0x00000113, 0x00000112, 0x00000110,
    0x0000010f, 0x0000010e, 0x0000010d, 0x0000010c, 0x0000010b, 0x0000010a, 0x00000109, 0x00000108,
    0x00000107, 0x00000106, 0x00000105, 0x00000104, 0x00000103, 0x00000102, 0x00000100, 0x00000100
};

/* Lossless premultiplication: 11-bit * alpha -> 19-bit. Reversible with this table. */
const uint32_t _smol_inv_div_p16l_lut [256] =
{
    0x00000000, 0x0002aaab, 0x00020000, 0x0001999a, 0x00015556, 0x00012493, 0x00010000, 0x0000e38f,
    0x0000cccd, 0x0000ba2f, 0x0000aaab, 0x00009d8a, 0x0000924a, 0x00008889, 0x00008000, 0x00007879,
    0x000071c8, 0x00006bcb, 0x00006667, 0x00006187, 0x00005d18, 0x0000590c, 0x00005556, 0x000051ec,
    0x00004ec5, 0x00004bdb, 0x00004925, 0x0000469f, 0x00004445, 0x00004211, 0x00004000, 0x00003e10,
    0x00003c3d, 0x00003a84, 0x000038e4, 0x0000375a, 0x000035e6, 0x00003484, 0x00003334, 0x000031f4,
    0x000030c4, 0x00002fa1, 0x00002e8c, 0x00002d83, 0x00002c86, 0x00002b94, 0x00002aab, 0x000029cc,
    0x000028f6, 0x00002829, 0x00002763, 0x000026a5, 0x000025ee, 0x0000253d, 0x00002493, 0x000023ef,
    0x00002350, 0x000022b7, 0x00002223, 0x00002193, 0x00002109, 0x00002083, 0x00002000, 0x00001f82,
    0x00001f08, 0x00001e92, 0x00001e1f, 0x00001daf, 0x00001d42, 0x00001cd9, 0x00001c72, 0x00001c0f,
    0x00001bad, 0x00001b4f, 0x00001af3, 0x00001a99, 0x00001a42, 0x000019ed, 0x0000199a, 0x00001949,
    0x000018fa, 0x000018ad, 0x00001862, 0x00001819, 0x000017d1, 0x0000178b, 0x00001746, 0x00001703,
    0x000016c2, 0x00001682, 0x00001643, 0x00001606, 0x000015ca, 0x0000158f, 0x00001556, 0x0000151e,
    0x000014e6, 0x000014b0, 0x0000147b, 0x00001447, 0x00001415, 0x000013e3, 0x000013b2, 0x00001382,
    0x00001353, 0x00001324, 0x000012f7, 0x000012ca, 0x0000129f, 0x00001274, 0x0000124a, 0x00001220,
    0x000011f8, 0x000011d0, 0x000011a8, 0x00001182, 0x0000115c, 0x00001136, 0x00001112, 0x000010ed,
    0x000010ca, 0x000010a7, 0x00001085, 0x00001063, 0x00001042, 0x00001021, 0x00001000, 0x00000fe1,
    0x00000fc1, 0x00000fa3, 0x00000f84, 0x00000f67, 0x00000f49, 0x00000f2c, 0x00000f10, 0x00000ef3,
    0x00000ed8, 0x00000ebc, 0x00000ea1, 0x00000e87, 0x00000e6d, 0x00000e53, 0x00000e39, 0x00000e20,
    0x00000e08, 0x00000def, 0x00000dd7, 0x00000dbf, 0x00000da8, 0x00000d91, 0x00000d7a, 0x00000d63,
    0x00000d4d, 0x00000d37, 0x00000d21, 0x00000d0c, 0x00000cf7, 0x00000ce2, 0x00000ccd, 0x00000cb9,
    0x00000ca5, 0x00000c91, 0x00000c7d, 0x00000c6a, 0x00000c57, 0x00000c44, 0x00000c31, 0x00000c1f,
    0x00000c0d, 0x00000bfb, 0x00000be9, 0x00000bd7, 0x00000bc6, 0x00000bb4, 0x00000ba3, 0x00000b93,
    0x00000b82, 0x00000b71, 0x00000b61, 0x00000b51, 0x00000b41, 0x00000b31, 0x00000b22, 0x00000b12,
    0x00000b03, 0x00000af4, 0x00000ae5, 0x00000ad7, 0x00000ac8, 0x00000ab9, 0x00000aab, 0x00000a9d,
    0x00000a8f, 0x00000a81, 0x00000a73, 0x00000a66, 0x00000a58, 0x00000a4b, 0x00000a3e, 0x00000a31,
    0x00000a24, 0x00000a17, 0x00000a0b, 0x000009fe, 0x000009f2, 0x000009e5, 0x000009d9, 0x000009cd,
    0x000009c1, 0x000009b5, 0x000009aa, 0x0000099e, 0x00000992, 0x00000987, 0x0000097c, 0x00000971,
    0x00000965, 0x0000095b, 0x00000950, 0x00000945, 0x0000093a, 0x00000930, 0x00000925, 0x0000091b,
    0x00000910, 0x00000906, 0x000008fc, 0x000008f2, 0x000008e8, 0x000008de, 0x000008d4, 0x000008cb,
    0x000008c1, 0x000008b8, 0x000008ae, 0x000008a5, 0x0000089b, 0x00000892, 0x00000889, 0x00000880,
    0x00000877, 0x0000086e, 0x00000865, 0x0000085c, 0x00000854, 0x0000084b, 0x00000843, 0x0000083a,
    0x00000832, 0x00000829, 0x00000821, 0x00000819, 0x00000811, 0x00000809, 0x00000800, 0x000007f9
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

    if (*n_dest_rows < 0 || *first_dest_row + *n_dest_rows > (int32_t) scale_ctx->vdim.dest_size_px)
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
    if (dest_row_index < scale_ctx->vdim.clear_before_px
        || dest_row_index >= scale_ctx->vdim.dest_size_px - scale_ctx->vdim.clear_after_px)
    {
        /* Row doesn't intersect placement */

        if (scale_ctx->composite_op == SMOL_COMPOSITE_SRC_CLEAR_DEST)
        {
            /* Clear entire row */
            scale_ctx->clear_dest_func (scale_ctx->color_pixels_clear_batch,
                                        row_out,
                                        scale_ctx->hdim.dest_size_px);
        }
    }
    else
    {
        if (scale_ctx->composite_op == SMOL_COMPOSITE_SRC_CLEAR_DEST)
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
        else
        {
            int scaled_row_index;

            scaled_row_index = scale_ctx->vfilter_func (scale_ctx,
                                                        local_ctx,
                                                        dest_row_index - scale_ctx->vdim.clear_before_px);

            if ((scale_ctx->composite_op == SMOL_COMPOSITE_SRC
                 || scale_ctx->composite_op == SMOL_COMPOSITE_SRC_CLEAR_DEST)
                && scale_ctx->have_composite_color)
            {
                scale_ctx->composite_over_color_func (local_ctx->parts_row [scaled_row_index],
                                                      scale_ctx->color_pixel,
                                                      scale_ctx->hdim.placement_size_px);
            }

            scale_ctx->pack_row_func (local_ctx->parts_row [scaled_row_index],
                                      dest_hofs_to_pointer (scale_ctx, row_out, scale_ctx->hdim.placement_ofs_px),
                                      scale_ctx->hdim.placement_size_px);

        }

        if (scale_ctx->composite_op == SMOL_COMPOSITE_SRC_CLEAR_DEST)
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

static void
do_rows (const SmolScaleCtx *scale_ctx,
         void *dest,
         uint32_t row_dest_index,
         uint32_t n_rows)
{
    SmolLocalCtx local_ctx = { 0 };
    uint32_t n_parts_per_pixel = 1;
    uint32_t n_stored_rows = 4;
    uint32_t i;

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

        local_ctx.parts_row [i] [scale_ctx->hdim.src_size_px * n_parts_per_pixel] = 0;
        if (n_parts_per_pixel == 2)
            local_ctx.parts_row [i] [scale_ctx->hdim.src_size_px * n_parts_per_pixel + 1] = 0;
    }

    for (i = row_dest_index; i < row_dest_index + n_rows; i++)
    {
        scale_dest_row (scale_ctx, &local_ctx, i, dest);
        dest = (char *) dest + scale_ctx->dest_rowstride;
    }

    for (i = 0; i < n_stored_rows; i++)
    {
        smol_free (local_ctx.row_storage [i]);
    }

    /* Used to align row data if needed. May be allocated in scale_horizontal(). */
    if (local_ctx.src_aligned)
        smol_free (local_ctx.src_aligned_storage);
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

    *first_opacity = SMOL_SUBPIXEL_MOD (-dest_ofs_spx - 1) + 1;
    *last_opacity = SMOL_SUBPIXEL_MOD (dest_ofs_spx + dest_dim_spx - 1) + 1;

    /* Special handling when the output is a single pixel */

    if (dest_dim == 1)
    {
        *first_opacity = dest_dim_spx;
        *last_opacity = 256;
    }

    /* The box algorithms are only sufficiently precise when
     * src_dim > dest_dim * 5. box_64bpp typically starts outperforming
     * bilinear+halving at src_dim > dest_dim * 8. */

    if (src_dim > dest_dim * 255)
    {
        *dest_storage = SMOL_STORAGE_128BPP;
        *dest_filter = SMOL_FILTER_BOX;
    }
    else if (src_dim > dest_dim * 8)
    {
        *dest_filter = SMOL_FILTER_BOX;
    }
    else if (src_dim <= 1)
    {
        *dest_filter = SMOL_FILTER_ONE;
        *last_opacity = ((dest_ofs_spx + dest_dim_spx - 1) % SMOL_SUBPIXEL_MUL) + 1;
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
        uint32_t d = dest_dim_spx;

        for (;;)
        {
            d *= 2;
            if (d >= src_dim_spx)
                break;
            n_halvings++;
        }

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

                    if (*((const uint32_t *) dest_order) == *((const uint32_t *) dest_pmeta->order))
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

static void
populate_clear_batch (SmolScaleCtx *scale_ctx)
{
    uint8_t dest_color [16];
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
    uint64_t color_pixel_internal [2];
    uint64_t color_pixel_as_src [2];
    int i = 0;

    if (color_pixel)
        scale_ctx->have_composite_color = TRUE;

    /* Check for noop (direct copy) */

    if (scale_ctx->hdim.src_size_spx == scale_ctx->hdim.dest_size_spx
        && scale_ctx->vdim.src_size_spx == scale_ctx->vdim.dest_size_spx
        && scale_ctx->src_pixel_type == scale_ctx->dest_pixel_type
        && scale_ctx->composite_op != SMOL_COMPOSITE_SRC_OVER_DEST)
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

    if (scale_ctx->hdim.src_size_px > scale_ctx->hdim.dest_size_px * 8191
        || scale_ctx->vdim.src_size_px > scale_ctx->vdim.dest_size_px * 8191)
    {
        /* Even with 128bpp, there's only enough bits to store 11-bit linearized
         * times 13 bits of summed pixels plus 8 bits of scratch space for
         * multiplying with an 8-bit weight -> 32 bits total per channel.
         *
         * For now, just turn off sRGB linearization if the input is bigger
         * than the output by a factor of 2^13 or more. */
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

        /* Need to unpack destination rows and composite on them */

        find_repacks (implementations,
                      dest_pmeta->storage, scale_ctx->storage_type, dest_pmeta->storage,
                      dest_pmeta->alpha, internal_alpha, dest_pmeta->alpha,
                      SMOL_GAMMA_SRGB_COMPRESSED, scale_ctx->gamma_type, SMOL_GAMMA_SRGB_COMPRESSED,
                      dest_pmeta, dest_pmeta,
                      &dest_unpack_rmeta, NULL);

        SMOL_ASSERT (dest_unpack_rmeta != NULL);

        scale_ctx->dest_unpack_row_func = dest_unpack_rmeta->repack_row_func;
    }
    else
    {
        /* Compositing on solid color */

        if (color_pixel)
        {
            SmolPixelType color_ptype;
            const SmolPixelTypeMeta *color_pmeta;
            const SmolRepackMeta *color_in_rmeta, *color_out_rmeta;

            color_ptype = get_host_pixel_type (color_pixel_type);
            color_pmeta = &pixel_type_meta [color_ptype];

            find_repacks (implementations,
                          color_pmeta->storage, scale_ctx->storage_type, src_pmeta->storage,
                          color_pmeta->alpha, internal_alpha, src_pmeta->alpha,
                          SMOL_GAMMA_SRGB_COMPRESSED, scale_ctx->gamma_type, SMOL_GAMMA_SRGB_COMPRESSED,
                          color_pmeta, src_pmeta,
                          &color_in_rmeta, &color_out_rmeta);

            SMOL_ASSERT (color_in_rmeta != NULL);
            SMOL_ASSERT (color_out_rmeta != NULL);

            /* We need the fill color to have the same internal byte order as
             * src. Currently, the simplest way to achieve this is to unpack the color,
             * repack it to the same format as src, then unpacking it to internal again. */

            color_in_rmeta->repack_row_func (color_pixel, color_pixel_internal, 1);
            color_out_rmeta->repack_row_func (color_pixel_internal, color_pixel_as_src, 1);
            src_rmeta->repack_row_func (color_pixel_as_src, scale_ctx->color_pixel, 1);
        }
        else
        {
            /* No color provided; use fully transparent black */
            memset (scale_ctx->color_pixel, 0, sizeof (scale_ctx->color_pixel));
        }

        populate_clear_batch (scale_ctx);
    }

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
            implementations [i]->composite_over_dest_funcs [scale_ctx->storage_type];
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
    dim->src_size_spx = src_size_spx;
    dim->src_size_px = SMOL_SPX_TO_PX (src_size_spx);
    dim->dest_size_spx = dest_size_spx;
    dim->dest_size_px = SMOL_SPX_TO_PX (dest_size_spx);
    dim->placement_ofs_spx = placement_ofs_spx;
    if (placement_ofs_spx < 0)
        dim->placement_ofs_px = (placement_ofs_spx - 255) / SMOL_SUBPIXEL_MUL;
    else
        dim->placement_ofs_px = placement_ofs_spx / SMOL_SUBPIXEL_MUL;
    dim->placement_size_spx = placement_size_spx;
    dim->placement_size_px = SMOL_SPX_TO_PX (placement_size_spx + SMOL_SUBPIXEL_MOD (placement_ofs_spx));

    pick_filter_params (dim->src_size_px,
                        dim->src_size_spx,
                        dim->placement_ofs_spx,
                        dim->placement_size_px,
                        dim->placement_size_spx,
                        &dim->n_halvings,
                        &dim->placement_size_prehalving_px,
                        &dim->placement_size_prehalving_spx,
                        &dim->filter_type,
                        storage_type_out,
                        &dim->first_opacity,
                        &dim->last_opacity,
                        flags);

    /* Calculate clip and clear intervals */

    if (dim->placement_ofs_px > 0)
    {
        dim->clear_before_px = dim->placement_ofs_px;
        dim->clip_before_px = 0;
    }
    else if (dim->placement_ofs_px < 0)
    {
        dim->clear_before_px = 0;
        dim->clip_before_px = -dim->placement_ofs_px;
        dim->first_opacity = 256;
    }

    if (dim->placement_ofs_px + dim->placement_size_px < dim->dest_size_px)
    {
        dim->clear_after_px = dim->dest_size_px - dim->placement_ofs_px - dim->placement_size_px;
        dim->clip_after_px = 0;
    }
    else if (dim->placement_ofs_px + dim->placement_size_px > dim->dest_size_px)
    {
        dim->clear_after_px = 0;
        dim->clip_after_px = dim->placement_ofs_px + dim->placement_size_px - dim->dest_size_px;
        dim->last_opacity = 256;
    }

    /* Clamp placement */

    if (dim->placement_ofs_px < 0)
    {
        dim->placement_size_px += dim->placement_ofs_px;
        dim->placement_ofs_px = 0;
    }

    if (dim->placement_ofs_px + dim->placement_size_px > dim->dest_size_px)
    {
        dim->placement_size_px = dim->dest_size_px - dim->placement_ofs_px;
    }
}

static void
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

    scale_ctx->storage_type = MAX (storage_type [0], storage_type [1]);

    scale_ctx->hdim.precalc = smol_alloc_aligned (((scale_ctx->hdim.placement_size_prehalving_px + 1) * 2
                                                + (scale_ctx->vdim.placement_size_prehalving_px + 1) * 2)
                                               * sizeof (uint16_t),
                                               &scale_ctx->precalc_storage);
    scale_ctx->vdim.precalc = ((uint16_t *) scale_ctx->hdim.precalc) + (scale_ctx->hdim.placement_size_prehalving_px + 1) * 2;

    get_implementations (scale_ctx, color_pixel, color_pixel_type);
}

static void
smol_scale_finalize (SmolScaleCtx *scale_ctx)
{
    free (scale_ctx->precalc_storage);
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

    scale_ctx = calloc (sizeof (SmolScaleCtx), 1);
    smol_scale_init (scale_ctx,
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
                     SMOL_COMPOSITE_SRC,
                     flags,
                     NULL,
                     NULL);
    return scale_ctx;
}

void
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
    SmolScaleCtx scale_ctx = { 0 };
    int first_row, n_rows;

    smol_scale_init (&scale_ctx,
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
                     SMOL_COMPOSITE_SRC,
                     flags,
                     NULL, NULL);

    first_row = 0;
    n_rows = scale_ctx.vdim.dest_size_px;

    if (check_row_range (&scale_ctx, &first_row, &n_rows))
    {
        do_rows (&scale_ctx,
                 dest_row_ofs_to_pointer (&scale_ctx, 0),
                 first_row,
                 n_rows);
    }

    smol_scale_finalize (&scale_ctx);
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
                     SmolFlags flags,
                     SmolPostRowFunc post_row_func,
                     void *user_data)
{
    SmolScaleCtx *scale_ctx;

    scale_ctx = calloc (sizeof (SmolScaleCtx), 1);
    smol_scale_init (scale_ctx,
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
                     placement_width,
                     placement_height,
                     composite_op,
                     flags,
                     post_row_func,
                     user_data);
    return scale_ctx;
}

void
smol_scale_destroy (SmolScaleCtx *scale_ctx)
{
    smol_scale_finalize (scale_ctx);
    free (scale_ctx);
}

void
smol_scale_batch (const SmolScaleCtx *scale_ctx,
                  int32_t first_dest_row,
                  int32_t n_dest_rows)
{
    if (!check_row_range (scale_ctx, &first_dest_row, &n_dest_rows))
        return;

    do_rows (scale_ctx,
             dest_row_ofs_to_pointer (scale_ctx, first_dest_row),
             first_dest_row,
             n_dest_rows);
}

void
smol_scale_batch_full (const SmolScaleCtx *scale_ctx,
                       void *dest,
                       int32_t first_dest_row,
                       int32_t n_dest_rows)
{
    if (!check_row_range (scale_ctx, &first_dest_row, &n_dest_rows))
        return;

    do_rows (scale_ctx,
             dest,
             first_dest_row,
             n_dest_rows);
}
