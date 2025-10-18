#!/bin/sh

run_cmd () {
    cmd="$1 ${top_srcdir}/tests/data/good/card-32c-noalpha.png >/dev/null"
    echo "$cmd" >&2
    sh -c "$cmd" || exit $?
}

run_cmd_single_file () {
    file="$2"
    cmd="$1 ${top_srcdir}/tests/data/$file >/dev/null"
    echo "$cmd" >&2
    sh -c "$cmd" || exit $?
}

run_cmd_multiple_files () {
    cmd="$1"; shift
    flist=
    for fname in $*; do
        flist="$flist ${top_srcdir}/tests/data/$1"; shift
    done
    cmd="$cmd $flist >/dev/null"
    echo "$cmd" >&2
    sh -c "$cmd"
}

run_cmd_all_safe_files () {
    # Only run on files for which we're guaranteed to have loaders.
    # '$dir/*.{gif,png,xwd}' is a Bash-ism, so we can't use it.
    dir="${top_srcdir}/tests/data/good"
    cmd="$1 $dir/*.gif $dir/*.png $dir/*.xwd >/dev/null"
    echo "$cmd" >&2
    sh -c "$cmd" || exit $?
}

run_cmd_all_bad_files () {
    # Since this only fails on crashes, it's fine to run it on absolutely
    # everything (including unsupported formats, build files etc).
    cmd="$1 ${top_srcdir}/tests/data/bad/* >/dev/null"
    echo "$cmd" >&2
    sh -c "$cmd"
    result=$?

    # For corrupt files, an exit value of 1 or 2 is a fine result,
    # but other values are trouble (e.g. terminated by signal).
    if test $result -gt 2; then exit $result; fi
}

get_supported_loaders () {
    sh -c "$tool --version" \
        | grep '^Loaders: ' \
        | sed 's/[^:]*: *\(.*\)/\1/' \
        | tr [:upper:] [:lower:]
}

[ "x${top_srcdir}" = "x" ] && top_srcdir="${srcdir}/.."
[ "x${top_builddir}" = "x" ] && top_builddir=".."

# Passing "long" as the first argument will run a much longer test
# cycle with more permutations.
long="no"
[ $# -ge 1 ] && [ "x$1" = "xlong" ] && long="yes"

tool="${top_builddir}/tools/chafa/chafa"
timeout_cmd="$(which timeout)"
[ "x${timeout_cmd}" = "x" ] && timeout_cmd="$(which gtimeout)"

if [ "x$long" = "xyes" ]; then
    formats="symbol sixel kitty iterm"
    cmodes="none 2 8 16/8 16 240 256 full"
    sizes="$(seq 1 100)"
    thread_counts="$(seq 1 32) 61"
else
    # The 13x13 size is specifically required to trigger the normalization
    # overflow (see commit 60d7718f9d8fa591d3d69079fe583913c58c19d9).

    formats="symbol sixel kitty iterm"
    cmodes="none 2 16/8 16 256 full"
    sizes="1 3 13x13 133"
    thread_counts="1 2 3 12 32 61"
fi
