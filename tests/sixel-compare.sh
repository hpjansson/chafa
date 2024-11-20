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
CHAFA_SIX_FILE="${BASE}-chafa.six"
CHAFA_SIXPNG_FILE="${BASE}-chafa.png"

# --- Prepare input --- #

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

CHAFA_RES=$($TIME_CMD -f "%U %e %M" -- \
    $CHAFA_CMD -f sixel --view-size 9999x9999 --exact-size on "$PREPARED_FILE" \
    2>&1 >"$CHAFA_SIX_FILE")
magick convert "$CHAFA_SIX_FILE" "$CHAFA_SIXPNG_FILE"

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

echo "specimen ncolors MSE SSIM PSNR utime elapsed peakrss"

ORIGINAL_COLORS=$(count_file_colors "$ORIGINAL_FILE")
ORIGINAL_MSE=$(eval_file_metric "$ORIGINAL_FILE" mse)
ORIGINAL_SSIM=$(eval_file_metric "$ORIGINAL_FILE" ssim)
ORIGINAL_PSNR=$(eval_file_metric "$ORIGINAL_FILE" psnr)
echo "original $ORIGINAL_COLORS 0 1 0 0.0 0.0 0"

PREPARED_COLORS=$(count_file_colors "$PREPARED_FILE")
PREPARED_MSE=$(eval_file_metric "$PREPARED_FILE" mse)
PREPARED_SSIM=$(eval_file_metric "$PREPARED_FILE" ssim)
PREPARED_PSNR=$(eval_file_metric "$PREPARED_FILE" psnr)
echo "prepared $PREPARED_COLORS 0 1 0 0.0 0.0 0"

MAGICK_COLORS=$(count_file_colors "$MAGICK_SIX_FILE")
MAGICK_MSE=$(eval_file_metric "$MAGICK_SIX_FILE" mse)
MAGICK_SSIM=$(eval_file_metric "$MAGICK_SIX_FILE" ssim)
MAGICK_PSNR=$(eval_file_metric "$MAGICK_SIX_FILE" psnr)
echo "magick $MAGICK_COLORS $MAGICK_MSE $MAGICK_SSIM $MAGICK_PSNR $MAGICK_RES"

IMG2SIXEL_COLORS=$(count_file_colors "$IMG2SIXEL_SIX_FILE")
IMG2SIXEL_MSE=$(eval_file_metric "$IMG2SIXEL_SIX_FILE" mse)
IMG2SIXEL_SSIM=$(eval_file_metric "$IMG2SIXEL_SIX_FILE" ssim)
IMG2SIXEL_PSNR=$(eval_file_metric "$IMG2SIXEL_SIX_FILE" psnr)
echo "img2sixel $IMG2SIXEL_COLORS $IMG2SIXEL_MSE $IMG2SIXEL_SSIM $IMG2SIXEL_PSNR $IMG2SIXEL_RES"

IMG2SIXEL_HIGH_COLORS=$(count_file_colors "$IMG2SIXEL_HIGH_SIX_FILE")
IMG2SIXEL_HIGH_MSE=$(eval_file_metric "$IMG2SIXEL_HIGH_SIX_FILE" mse)
IMG2SIXEL_HIGH_SSIM=$(eval_file_metric "$IMG2SIXEL_HIGH_SIX_FILE" ssim)
IMG2SIXEL_HIGH_PSNR=$(eval_file_metric "$IMG2SIXEL_HIGH_SIX_FILE" psnr)
echo "img2sixel-high $IMG2SIXEL_HIGH_COLORS $IMG2SIXEL_HIGH_MSE $IMG2SIXEL_HIGH_SSIM $IMG2SIXEL_HIGH_PSNR $IMG2SIXEL_HIGH_RES"

CHAFA_COLORS=$(count_file_colors "$CHAFA_SIX_FILE")
CHAFA_MSE=$(eval_file_metric "$CHAFA_SIX_FILE" mse)
CHAFA_SSIM=$(eval_file_metric "$CHAFA_SIX_FILE" ssim)
CHAFA_PSNR=$(eval_file_metric "$CHAFA_SIX_FILE" psnr)
echo "chafa $CHAFA_COLORS $CHAFA_MSE $CHAFA_SSIM $CHAFA_PSNR $CHAFA_RES"
