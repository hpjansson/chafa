#!/bin/sh

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

# One file ok
run_cmd_single_file "$tool --animate no" "good/pixel.png"
[ $? -eq 0 ] || exit 1

# Multiple files ok
run_cmd_multiple_files "$tool --animate no" "good/pixel.png" "good/pixel.gif"
[ $? -eq 0 ] || exit 1

# Some files failed
run_cmd_multiple_files "$tool --animate no" "good/pixel.png" "missing.foo"
[ $? -eq 1 ] || exit 1

# All files failed
run_cmd_multiple_files "$tool --animate no" "missing1.foo" "missing2.foo"
[ $? -eq 2 ] || exit 1

# --help
run_cmd_multiple_files "$tool --help"
[ $? -eq 0 ] || exit 1

# --version
run_cmd_multiple_files "$tool --version"
[ $? -eq 0 ] || exit 1

# No args
run_cmd_multiple_files "$tool"
[ $? -eq 2 ] || exit 1

exit 0
