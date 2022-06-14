#!/bin/sh

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

for cmode in $cmodes; do
    for size in $sizes; do
        for n_threads in $thread_counts; do
            run_cmd "$tool -f symbol -c $cmode -s $size --threads $n_threads --animate no" || exit $?
        done
    done
done
