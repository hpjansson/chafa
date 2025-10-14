#!/bin/sh

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

grid_sizes="1 4 x3 5x"

for format in $formats; do
    for size in $sizes; do
        for grid_size in $grid_sizes; do
            run_cmd_all_safe_files "$tool -f $format -c full -s $size --grid $grid_size --threads 3" || exit $?
        done
    done
done
