#!/bin/sh

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

extensions="$(get_supported_loaders)"

for ext in $extensions; do
    [ "x${ext}" = "ximagemagick" ] && continue
    run_cmd_single_file "$tool -f sixel --threads 12 --animate no" "good/pixel.$ext"
done
