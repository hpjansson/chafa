#!/bin/sh

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

tempdir="${top_builddir}/tests/.temp"

xvfb=$(which Xvfb)
[ "x${xvfb}" = "x" ] && echo "Missing Xvfb" && exit 1

gterm_name=xfce4-terminal
gterm=$(which ${gterm_name})
[ "x${gterm}" = "x" ] && echo "Missing ${gterm_name}" && exit 1

montage=$(which montage)
[ "x${montage}" = "x" ] && echo "Missing montage" && exit 1

# Terminal parameters
font='xos4 Terminus 8'
geometry=60x40
screen_size=1280x1024

# Chafa parameter lists
color_modes="tc 240 16 8 2 none"
color_spaces="rgb din99d"
color_extractors="average median"
dither_types="none ordered diffusion"
symbol_selectors="+0 all wide"
work_factors="1 5 9"

# Input filename. Must be in data/good/.
file="taxic.jpg"

function gen_screenshot
{
    FNAME="$1"

    echo -- Running -e "bash -c \"${tool} -s ${geometry} -w ${5} -c ${2} --symbols ${3} --color-space ${4} \
${top_srcdir}/tests/data/good/${FNAME} ; sleep 0.5 ; import -window chafa out/out-${2}-${3}-${5}-${4}.png -trim\""

    HOME="${tempdir}" XDG_CONFIG_HOME="${HOME}/.config" DISPLAY=:99 ${gterm} \
        -T chafa \
        --disable-server \
        --geometry ${geometry}+0+0 \
        --font "$font" \
        --hide-menubar \
        --hide-toolbar \
        --hide-borders \
        --hide-scrollbar \
        -e "bash -c \"${tool} -s ${geometry} -w ${5} -c ${2} --symbols ${3} --color-space ${4} \
${top_srcdir}/tests/data/good/${FNAME}; sleep 0.5 ; import -window chafa ${tempdir}/out/${2}-${3}-${5}-${4}.png -trim >/dev/null 2>&1\"" >/dev/null 2>&1
}

trap "" EXIT SIGQUIT SIGSTOP SIGTERM

# Make sure we don't blow away any old .temp dir
rm -Rf "${tempdir}/.config"
rm -Rf "${tempdir}/out"
rmdir "${tempdir}"

mkdir -p "${tempdir}/out"

mkdir -p ${tempdir}/.config/xfce4/terminal
cat >${tempdir}/.config/xfce4/terminal/terminalrc <<EOF
[Configuration]
MiscAlwaysShowTabs=FALSE
MiscBell=FALSE
MiscBellUrgent=FALSE
MiscBordersDefault=TRUE
MiscCursorBlinks=TRUE
MiscCursorShape=TERMINAL_CURSOR_SHAPE_BLOCK
MiscDefaultGeometry=${geometry}
MiscInheritGeometry=FALSE
MiscMenubarDefault=TRUE
MiscMouseAutohide=FALSE
MiscMouseWheelZoom=TRUE
MiscToolbarDefault=FALSE
MiscConfirmClose=TRUE
MiscCycleTabs=TRUE
MiscTabCloseButtons=TRUE
MiscTabCloseMiddleClick=TRUE
MiscTabPosition=GTK_POS_TOP
MiscHighlightUrls=TRUE
MiscMiddleClickOpensUri=FALSE
MiscCopyOnSelect=FALSE
MiscRewrapOnResize=TRUE
MiscUseShiftArrowsToScroll=FALSE
MiscSlimTabs=FALSE
MiscNewTabAdjacent=FALSE
ScrollingOnOutput=FALSE
FontName=${font}
ScrollingBar=TERMINAL_SCROLLBAR_LEFT
ColorCursorUseDefault=FALSE
ColorCursorForeground=#000000000000
ColorCursor=#000000000000

EOF

${xvfb} :99 -ac -screen 0 ${screen_size}x24 >/dev/null 2>&1 &
XVFB_PID=$!

sleep 2

for cmode in ${color_modes}; do
    for symbols in ${symbol_selectors}; do
        for cspace in ${color_spaces}; do
            for work in ${work_factors}; do
                gen_screenshot "$file" "$cmode" "$symbols" "$cspace" "$work"
            done
        done
    done
done

kill ${XVFB_PID}

rm -f "${top_builddir}/tests/chafa-tool-gallery.png"
${montage} -stroke white -background grey20 -geometry +4+4 -tile 6 -label '%f' \
    "${tempdir}/out/*.png" \
    "${top_builddir}/tests/chafa-tool-gallery.png"
