#!/bin/sh

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

for format in $formats; do
    for size in $sizes; do
        run_cmd_all_bad_files "$tool -f $format -c 16 --dither fs -s $size --threads 12 -d 0 --speed max" || exit $?
    done
done
