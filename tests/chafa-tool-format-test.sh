#!/bin/sh

[ "x${top_srcdir}" == "x" ] && top_srcdir=".."
[ "x${top_builddir}" == "x" ] && top_builddir="$top_srcdir"
. "${top_srcdir}/tests/chafa-tool-test-common.sh"

for format in $formats; do
    for size in $sizes; do
        for n_threads in $thread_counts; do
            run_cmd "$tool -f $format -c full -s $size --threads $n_threads --animate no"
        done
    done
done
