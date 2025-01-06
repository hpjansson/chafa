#!/bin/bash

# --- Variables and filenames --- #

# Original file and prepared input file.
ORIGINAL_FILE="$1"
BASE=$(basename "$1")
PREPARED_FILE="${BASE}-prepared.qif"
PREPARED_FILE_PNG="${BASE}-prepared.png"

# Programs under test.
MAGICK_CMD=magick
IMG2SIXEL_CMD=img2sixel
CHAFA_CMD=chafa

# We need GNU time for resource measurements.
TIME_PATH="$(which time)"
TIME_CMD="$TIME_PATH"

# Sixel output files and PNGs converted back for visual inspection.
# These will be left in the current directory, overwriting any existing
# files by the same name.
MAGICK_SIX_FILE="${BASE}-magick.six"
MAGICK_SIXPNG_FILE="${BASE}-magick.png"
IMG2SIXEL_SIX_FILE="${BASE}-img2sixel.six"
IMG2SIXEL_SIXPNG_FILE="${BASE}-img2sixel.png"
IMG2SIXEL_HIGH_SIX_FILE="${BASE}-img2sixel-high.six"
IMG2SIXEL_HIGH_SIXPNG_FILE="${BASE}-img2sixel-high.png"
CHAFA_W1_SIX_FILE="${BASE}-chafa-w1.six"
CHAFA_W1_SIXPNG_FILE="${BASE}-chafa-w1.png"
CHAFA_W2_SIX_FILE="${BASE}-chafa-w2.six"
CHAFA_W2_SIXPNG_FILE="${BASE}-chafa-w2.png"
CHAFA_W3_SIX_FILE="${BASE}-chafa-w3.six"
CHAFA_W3_SIXPNG_FILE="${BASE}-chafa-w3.png"
CHAFA_W4_SIX_FILE="${BASE}-chafa-w4.six"
CHAFA_W4_SIXPNG_FILE="${BASE}-chafa-w4.png"
CHAFA_W5_SIX_FILE="${BASE}-chafa-w5.six"
CHAFA_W5_SIXPNG_FILE="${BASE}-chafa-w5.png"
CHAFA_W6_SIX_FILE="${BASE}-chafa-w6.six"
CHAFA_W6_SIXPNG_FILE="${BASE}-chafa-w6.png"
CHAFA_W7_SIX_FILE="${BASE}-chafa-w7.six"
CHAFA_W7_SIXPNG_FILE="${BASE}-chafa-w7.png"
CHAFA_W8_SIX_FILE="${BASE}-chafa-w8.six"
CHAFA_W8_SIXPNG_FILE="${BASE}-chafa-w8.png"
CHAFA_W9_SIX_FILE="${BASE}-chafa-w9.six"
CHAFA_W9_SIXPNG_FILE="${BASE}-chafa-w9.png"

# --- Prepare input --- #

echo 'INFO: Preparing input...' >&2

# Magick (7.1.1-26 Q16-HDRI) will create corrupt sixel files for very large
# images, so resize if necessary. This will also convert to QIF, which is
# a very simple image format - differences in loaders should affect the
# quality minimally (as compared to e.g. JPEG), and loader overhead should be
# a smaller proportion of overall resource usage (as compared to PNG),
# resulting in a more direct comparison of the image conversion internals.
magick convert -resize '10000000@>' "$ORIGINAL_FILE" "$PREPARED_FILE"

# Write a courtesy PNG copy of the prepared file for easier viewing.
magick convert "$PREPARED_FILE" "$PREPARED_FILE_PNG"

# Chafa pads to the character cell size, so we need to crop that out.
DIMENSIONS=$(magick identify -ping -format '%wx%h' "$PREPARED_FILE")

# --- Run the converters --- #

echo 'INFO: Running converters...' >&2

MAGICK_RES=$($TIME_CMD -f "%U %e %M" -- \
    $MAGICK_CMD convert +dither "$PREPARED_FILE" sixel:- \
    2>&1 >"$MAGICK_SIX_FILE")
magick convert "$MAGICK_SIX_FILE" "$MAGICK_SIXPNG_FILE"

IMG2SIXEL_RES=$($TIME_CMD -f "%U %e %M" -- \
    $IMG2SIXEL_CMD -q full -d none "$PREPARED_FILE" \
    2>&1 >"$IMG2SIXEL_SIX_FILE")
magick convert "$IMG2SIXEL_SIX_FILE" "$IMG2SIXEL_SIXPNG_FILE"

IMG2SIXEL_HIGH_RES=$($TIME_CMD -f "%U %e %M" -- \
    $IMG2SIXEL_CMD -q full -d none -I "$PREPARED_FILE" \
    2>&1 >"$IMG2SIXEL_HIGH_SIX_FILE")
magick convert "$IMG2SIXEL_HIGH_SIX_FILE" "$IMG2SIXEL_HIGH_SIXPNG_FILE"

CHAFA_W1_RES=$($TIME_CMD -f "%U %e %M" -- \
    $CHAFA_CMD -w 1 -f sixel --dither none --view-size 9999x9999 --exact-size on "$PREPARED_FILE" \
    2>&1 >"$CHAFA_W1_SIX_FILE")
magick convert "$CHAFA_W1_SIX_FILE" "$CHAFA_W1_SIXPNG_FILE"

CHAFA_W2_RES=$($TIME_CMD -f "%U %e %M" -- \
    $CHAFA_CMD -w 2 -f sixel --dither none --view-size 9999x9999 --exact-size on "$PREPARED_FILE" \
    2>&1 >"$CHAFA_W2_SIX_FILE")
magick convert "$CHAFA_W2_SIX_FILE" "$CHAFA_W2_SIXPNG_FILE"

CHAFA_W3_RES=$($TIME_CMD -f "%U %e %M" -- \
    $CHAFA_CMD -w 3 -f sixel --dither none --view-size 9999x9999 --exact-size on "$PREPARED_FILE" \
    2>&1 >"$CHAFA_W3_SIX_FILE")
magick convert "$CHAFA_W3_SIX_FILE" "$CHAFA_W3_SIXPNG_FILE"

CHAFA_W4_RES=$($TIME_CMD -f "%U %e %M" -- \
    $CHAFA_CMD -w 4 -f sixel --dither none --view-size 9999x9999 --exact-size on "$PREPARED_FILE" \
    2>&1 >"$CHAFA_W4_SIX_FILE")
magick convert "$CHAFA_W4_SIX_FILE" "$CHAFA_W4_SIXPNG_FILE"

CHAFA_W5_RES=$($TIME_CMD -f "%U %e %M" -- \
    $CHAFA_CMD -w 5 -f sixel --dither none --view-size 9999x9999 --exact-size on "$PREPARED_FILE" \
    2>&1 >"$CHAFA_W5_SIX_FILE")
magick convert "$CHAFA_W5_SIX_FILE" "$CHAFA_W5_SIXPNG_FILE"

CHAFA_W6_RES=$($TIME_CMD -f "%U %e %M" -- \
    $CHAFA_CMD -w 6 -f sixel --dither none --view-size 9999x9999 --exact-size on "$PREPARED_FILE" \
    2>&1 >"$CHAFA_W6_SIX_FILE")
magick convert "$CHAFA_W6_SIX_FILE" "$CHAFA_W6_SIXPNG_FILE"

CHAFA_W7_RES=$($TIME_CMD -f "%U %e %M" -- \
    $CHAFA_CMD -w 7 -f sixel --dither none --view-size 9999x9999 --exact-size on "$PREPARED_FILE" \
    2>&1 >"$CHAFA_W7_SIX_FILE")
magick convert "$CHAFA_W7_SIX_FILE" "$CHAFA_W7_SIXPNG_FILE"

CHAFA_W8_RES=$($TIME_CMD -f "%U %e %M" -- \
    $CHAFA_CMD -w 8 -f sixel --dither none --view-size 9999x9999 --exact-size on "$PREPARED_FILE" \
    2>&1 >"$CHAFA_W8_SIX_FILE")
magick convert "$CHAFA_W8_SIX_FILE" "$CHAFA_W8_SIXPNG_FILE"

CHAFA_W9_RES=$($TIME_CMD -f "%U %e %M" -- \
    $CHAFA_CMD -w 9 -f sixel --dither none --view-size 9999x9999 --exact-size on "$PREPARED_FILE" \
    2>&1 >"$CHAFA_W9_SIX_FILE")
magick convert "$CHAFA_W9_SIX_FILE" "$CHAFA_W9_SIXPNG_FILE"

# --- Calculate and print metrics relative to prepared image --- #

eval_file_metric () {
    # Magick will produce an additional normalized output for some metrics.
    # Remove it with 'cut' to avoid cluttering the table.
    magick compare -metric "$2" "$PREPARED_FILE" \
        \( "$1" -crop "$DIMENSIONS"+0+0 +repage \) null: 2>&1 | \
        cut -d ' ' -f 1
}

count_file_colors () {
    # Count the colors in images to ensure the result is apples to apples.
    magick identify -format '%k' "$1"
}

echo 'INFO: Assessing output images...' >&2
echo >&2

echo "specimen ncolors MSE SSIM PSNR PAE utime elapsed peakrss"

ORIGINAL_COLORS=$(count_file_colors "$ORIGINAL_FILE")
ORIGINAL_MSE=$(eval_file_metric "$ORIGINAL_FILE" mse)
ORIGINAL_SSIM=$(eval_file_metric "$ORIGINAL_FILE" ssim)
ORIGINAL_PSNR=$(eval_file_metric "$ORIGINAL_FILE" psnr)
ORIGINAL_PAE=$(eval_file_metric "$ORIGINAL_FILE" pae)
echo "original $ORIGINAL_COLORS 0 1 0 0.0 0.0 0"

PREPARED_COLORS=$(count_file_colors "$PREPARED_FILE")
PREPARED_MSE=$(eval_file_metric "$PREPARED_FILE" mse)
PREPARED_SSIM=$(eval_file_metric "$PREPARED_FILE" ssim)
PREPARED_PSNR=$(eval_file_metric "$PREPARED_FILE" psnr)
PREPARED_PAE=$(eval_file_metric "$PREPARED_FILE" pae)
echo "prepared $PREPARED_COLORS 0 1 0 0.0 0.0 0"

MAGICK_COLORS=$(count_file_colors "$MAGICK_SIX_FILE")
MAGICK_MSE=$(eval_file_metric "$MAGICK_SIX_FILE" mse)
MAGICK_SSIM=$(eval_file_metric "$MAGICK_SIX_FILE" ssim)
MAGICK_PSNR=$(eval_file_metric "$MAGICK_SIX_FILE" psnr)
MAGICK_PAE=$(eval_file_metric "$MAGICK_SIX_FILE" pae)
echo "magick $MAGICK_COLORS $MAGICK_MSE $MAGICK_SSIM $MAGICK_PSNR $MAGICK_PAE $MAGICK_RES"

IMG2SIXEL_COLORS=$(count_file_colors "$IMG2SIXEL_SIX_FILE")
IMG2SIXEL_MSE=$(eval_file_metric "$IMG2SIXEL_SIX_FILE" mse)
IMG2SIXEL_SSIM=$(eval_file_metric "$IMG2SIXEL_SIX_FILE" ssim)
IMG2SIXEL_PSNR=$(eval_file_metric "$IMG2SIXEL_SIX_FILE" psnr)
IMG2SIXEL_PAE=$(eval_file_metric "$IMG2SIXEL_SIX_FILE" pae)
echo "img2sixel $IMG2SIXEL_COLORS $IMG2SIXEL_MSE $IMG2SIXEL_SSIM $IMG2SIXEL_PSNR $IMG2SIXEL_PAE $IMG2SIXEL_RES"

IMG2SIXEL_HIGH_COLORS=$(count_file_colors "$IMG2SIXEL_HIGH_SIX_FILE")
IMG2SIXEL_HIGH_MSE=$(eval_file_metric "$IMG2SIXEL_HIGH_SIX_FILE" mse)
IMG2SIXEL_HIGH_SSIM=$(eval_file_metric "$IMG2SIXEL_HIGH_SIX_FILE" ssim)
IMG2SIXEL_HIGH_PSNR=$(eval_file_metric "$IMG2SIXEL_HIGH_SIX_FILE" psnr)
IMG2SIXEL_HIGH_PAE=$(eval_file_metric "$IMG2SIXEL_HIGH_SIX_FILE" pae)
echo "img2sixel-high $IMG2SIXEL_HIGH_COLORS $IMG2SIXEL_HIGH_MSE $IMG2SIXEL_HIGH_SSIM $IMG2SIXEL_HIGH_PSNR $IMG2SIXEL_HIGH_PAE $IMG2SIXEL_HIGH_RES"

CHAFA_W1_COLORS=$(count_file_colors "$CHAFA_W1_SIX_FILE")
CHAFA_W1_MSE=$(eval_file_metric "$CHAFA_W1_SIX_FILE" mse)
CHAFA_W1_SSIM=$(eval_file_metric "$CHAFA_W1_SIX_FILE" ssim)
CHAFA_W1_PSNR=$(eval_file_metric "$CHAFA_W1_SIX_FILE" psnr)
CHAFA_W1_PAE=$(eval_file_metric "$CHAFA_W1_SIX_FILE" pae)
echo "chafa-w1 $CHAFA_W1_COLORS $CHAFA_W1_MSE $CHAFA_W1_SSIM $CHAFA_W1_PSNR $CHAFA_W1_PAE $CHAFA_W1_RES"

CHAFA_W2_COLORS=$(count_file_colors "$CHAFA_W2_SIX_FILE")
CHAFA_W2_MSE=$(eval_file_metric "$CHAFA_W2_SIX_FILE" mse)
CHAFA_W2_SSIM=$(eval_file_metric "$CHAFA_W2_SIX_FILE" ssim)
CHAFA_W2_PSNR=$(eval_file_metric "$CHAFA_W2_SIX_FILE" psnr)
CHAFA_W2_PAE=$(eval_file_metric "$CHAFA_W2_SIX_FILE" pae)
echo "chafa-w2 $CHAFA_W2_COLORS $CHAFA_W2_MSE $CHAFA_W2_SSIM $CHAFA_W2_PSNR $CHAFA_W2_PAE $CHAFA_W2_RES"

CHAFA_W3_COLORS=$(count_file_colors "$CHAFA_W3_SIX_FILE")
CHAFA_W3_MSE=$(eval_file_metric "$CHAFA_W3_SIX_FILE" mse)
CHAFA_W3_SSIM=$(eval_file_metric "$CHAFA_W3_SIX_FILE" ssim)
CHAFA_W3_PSNR=$(eval_file_metric "$CHAFA_W3_SIX_FILE" psnr)
CHAFA_W3_PAE=$(eval_file_metric "$CHAFA_W3_SIX_FILE" pae)
echo "chafa-w3 $CHAFA_W3_COLORS $CHAFA_W3_MSE $CHAFA_W3_SSIM $CHAFA_W3_PSNR $CHAFA_W3_PAE $CHAFA_W3_RES"

CHAFA_W4_COLORS=$(count_file_colors "$CHAFA_W4_SIX_FILE")
CHAFA_W4_MSE=$(eval_file_metric "$CHAFA_W4_SIX_FILE" mse)
CHAFA_W4_SSIM=$(eval_file_metric "$CHAFA_W4_SIX_FILE" ssim)
CHAFA_W4_PSNR=$(eval_file_metric "$CHAFA_W4_SIX_FILE" psnr)
CHAFA_W4_PAE=$(eval_file_metric "$CHAFA_W4_SIX_FILE" pae)
echo "chafa-w4 $CHAFA_W4_COLORS $CHAFA_W4_MSE $CHAFA_W4_SSIM $CHAFA_W4_PSNR $CHAFA_W4_PAE $CHAFA_W4_RES"

CHAFA_W5_COLORS=$(count_file_colors "$CHAFA_W5_SIX_FILE")
CHAFA_W5_MSE=$(eval_file_metric "$CHAFA_W5_SIX_FILE" mse)
CHAFA_W5_SSIM=$(eval_file_metric "$CHAFA_W5_SIX_FILE" ssim)
CHAFA_W5_PSNR=$(eval_file_metric "$CHAFA_W5_SIX_FILE" psnr)
CHAFA_W5_PAE=$(eval_file_metric "$CHAFA_W5_SIX_FILE" pae)
echo "chafa-w5 $CHAFA_W5_COLORS $CHAFA_W5_MSE $CHAFA_W5_SSIM $CHAFA_W5_PSNR $CHAFA_W5_PAE $CHAFA_W5_RES"

CHAFA_W6_COLORS=$(count_file_colors "$CHAFA_W6_SIX_FILE")
CHAFA_W6_MSE=$(eval_file_metric "$CHAFA_W6_SIX_FILE" mse)
CHAFA_W6_SSIM=$(eval_file_metric "$CHAFA_W6_SIX_FILE" ssim)
CHAFA_W6_PSNR=$(eval_file_metric "$CHAFA_W6_SIX_FILE" psnr)
CHAFA_W6_PAE=$(eval_file_metric "$CHAFA_W6_SIX_FILE" pae)
echo "chafa-w6 $CHAFA_W6_COLORS $CHAFA_W6_MSE $CHAFA_W6_SSIM $CHAFA_W6_PSNR $CHAFA_W6_PAE $CHAFA_W6_RES"

CHAFA_W7_COLORS=$(count_file_colors "$CHAFA_W7_SIX_FILE")
CHAFA_W7_MSE=$(eval_file_metric "$CHAFA_W7_SIX_FILE" mse)
CHAFA_W7_SSIM=$(eval_file_metric "$CHAFA_W7_SIX_FILE" ssim)
CHAFA_W7_PSNR=$(eval_file_metric "$CHAFA_W7_SIX_FILE" psnr)
CHAFA_W7_PAE=$(eval_file_metric "$CHAFA_W7_SIX_FILE" pae)
echo "chafa-w7 $CHAFA_W7_COLORS $CHAFA_W7_MSE $CHAFA_W7_SSIM $CHAFA_W7_PSNR $CHAFA_W7_PAE $CHAFA_W7_RES"

CHAFA_W8_COLORS=$(count_file_colors "$CHAFA_W8_SIX_FILE")
CHAFA_W8_MSE=$(eval_file_metric "$CHAFA_W8_SIX_FILE" mse)
CHAFA_W8_SSIM=$(eval_file_metric "$CHAFA_W8_SIX_FILE" ssim)
CHAFA_W8_PSNR=$(eval_file_metric "$CHAFA_W8_SIX_FILE" psnr)
CHAFA_W8_PAE=$(eval_file_metric "$CHAFA_W8_SIX_FILE" pae)
echo "chafa-w8 $CHAFA_W8_COLORS $CHAFA_W8_MSE $CHAFA_W8_SSIM $CHAFA_W8_PSNR $CHAFA_W8_PAE $CHAFA_W8_RES"

CHAFA_W9_COLORS=$(count_file_colors "$CHAFA_W9_SIX_FILE")
CHAFA_W9_MSE=$(eval_file_metric "$CHAFA_W9_SIX_FILE" mse)
CHAFA_W9_SSIM=$(eval_file_metric "$CHAFA_W9_SIX_FILE" ssim)
CHAFA_W9_PSNR=$(eval_file_metric "$CHAFA_W9_SIX_FILE" psnr)
CHAFA_W9_PAE=$(eval_file_metric "$CHAFA_W9_SIX_FILE" pae)
echo "chafa-w9 $CHAFA_W9_COLORS $CHAFA_W9_MSE $CHAFA_W9_SSIM $CHAFA_W9_PSNR $CHAFA_W9_PAE $CHAFA_W9_RES"
