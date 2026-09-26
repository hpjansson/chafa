#!/bin/sh

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

tool_args="-f sixel --threads 12 --animate no"
extensions="$(get_supported_loaders)"

for ext in $extensions; do
    [ "x${ext}" = "ximagemagick" ] && continue
    [ "x${ext}" = "xcoregraphics" ] && continue

    if [ "x${ext}" = "xheif" ]; then
        # libheif loads its codecs as plugins at run time, so a build with
        # the HEIF loader can still lack a decoder for the sample. Let's be
        # lenient.

        cmd="$tool $tool_args ${top_srcdir}/tests/data/good/pixel.heif >/dev/null"
        echo "$cmd" >&2
        sh -c "$cmd"
        result=$?

        if [ $result -eq 2 ]; then
            echo "NOTE: The HEIF loader is built, but libheif could not decode pixel.heif." >&2
            echo "      It probably lacks an AV1 decoder plugin (e.g. libheif-plugin-aomdec" >&2
            echo "      or libheif-plugin-dav1d). Skipping the HEIF loader test." >&2
            continue
        fi

        [ $result -ne 0 ] && exit $result
        continue
    fi

    run_cmd_single_file "$tool $tool_args" "good/pixel.$ext" || exit $?
done
