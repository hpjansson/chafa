#!/bin/sh

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

for format in $formats; do
    for size in $sizes; do
        for n_threads in $thread_counts; do
            run_cmd_all_safe_files "$tool -f $format -c full -s $size --threads $n_threads -d 0 --speed max" || exit $?
        done
    done
done
