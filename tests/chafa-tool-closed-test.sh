#!/bin/sh

# Run with closed stdin/stdout/stderr and look for hangs.

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

run_cmd_noredir () {
    cmd="$1 ${top_srcdir}/tests/data/good/taxic.jpg"
    echo "$cmd" >&2
    sh -c "$cmd" || exit $?
}

run_cmd_noredir "$timeout_cmd 20 $tool -f symbol -c full -s 200 <&- >/dev/null" || exit $?
run_cmd_noredir "$timeout_cmd 20 $tool -f symbol -c full -s 200 >&- " || exit $?
run_cmd_noredir "$timeout_cmd 20 $tool -f symbol -c full -s 200 <&- >&- " || exit $?
run_cmd_noredir "$timeout_cmd 20 $tool -f symbol -c full -s 200 <&- >&- 2>&-" || exit $?
