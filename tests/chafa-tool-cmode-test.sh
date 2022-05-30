#!/bin/sh

[ "x${top_srcdir}" = "x" ] && top_srcdir=".."
[ "x${top_builddir}" = "x" ] && top_builddir="$top_srcdir"
. "${top_srcdir}/tests/chafa-tool-test-common.sh"

for cmode in $cmodes; do
    for size in $sizes; do
        for n_threads in $thread_counts; do
            run_cmd "$tool -f symbol -c $cmode -s $size --threads $n_threads --animate no"
        done
    done
done
