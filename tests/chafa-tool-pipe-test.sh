#!/bin/sh

[ "x${srcdir}" = "x" ] && srcdir="."
. "${srcdir}/chafa-tool-test-common.sh"

run_cmd "$tool -f symbol -c full -s 63 --threads 12 --animate no < "
