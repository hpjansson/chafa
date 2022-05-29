#!/bin/sh

set -e

srcdir="$1"
builddir="$2"
if [ "$3" == "quick" ]; then quick=yes; echo foo; fi

tool="$builddir/tools/chafa/chafa"

if [ "$quick" == "yes" ]; then
    sizes="1 3 17 133"
    thread_counts="1 2 3 4 6 12 13 24 32 61"
else
    sizes="$(seq 1 100)"
    thread_counts="$(seq 1 32) 61"
fi

for cmode in none 2 8 16/8 16 240 256 full; do
    for size in $sizes; do
        for n_threads in $thread_counts; do
            cmd="$tool -c $cmode -s $size --threads $n_threads --animate no"
            echo $cmd
            $cmd "$srcdir/docs/chafa-logo.gif" >/dev/null
        done
    done
done
