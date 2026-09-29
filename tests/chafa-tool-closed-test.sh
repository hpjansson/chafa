#!/bin/sh

# Run with closed stdin/stdout/stderr and look for hangs.

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

tool_args="-f symbol -c full -s 400x200 --stretch"

run_cmd_noredir () {
    cmd="$1 ${top_srcdir}/tests/data/good/card-full-noalpha.png"
    echo "$cmd" >&2
    sh -c "$cmd" || exit $?
}

run_cmd_noredir "$timeout_cmd 20 $tool $tool_args <&- >/dev/null" || exit $?
run_cmd_noredir "$timeout_cmd 20 $tool $tool_args >&- " || exit $?
run_cmd_noredir "$timeout_cmd 20 $tool $tool_args <&- >&- " || exit $?
run_cmd_noredir "$timeout_cmd 20 $tool $tool_args <&- >&- 2>&-" || exit $?
