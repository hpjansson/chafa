#!/bin/sh

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

color_spaces="rgb din99d"
color_extractors="average median"
dither_types="none ordered diffusion"
symbol_selectors="none all wide 0..1ffff"

for format in $formats; do
    for color_space in $color_spaces; do
        for color_extractor in $color_extractors; do
            for dither_type in $dither_types; do
                for main_symbols in $symbol_selectors; do
                    for fill_symbols in $symbol_selectors; do
                        run_cmd "$tool -f $format -c 240 -s 53 --threads 12 --animate no --color-space $color_space \
--color-extractor $color_extractor --dither $dither_type --symbols $main_symbols --fill $fill_symbols"
                    done
                done
            done
        done
    done
done
