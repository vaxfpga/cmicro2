#!/bin/sh

# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

cmd="${1:-./cmicro2}"

mkdir -p "./runs/test1"
mkdir -p "./runs/test2"

check() {
    sed -Ei -e 's/(\+ \.\/cmicro2)(-\w+)?/\1/' "runs/$1/output.txt"

    if diff -q "runs/$1/output.txt" "tests/$1/output.txt" >/dev/null; then
        echo "test ${1#test}: \e[32mPASSED\e[0m"
    else
        echo "test ${1#test}: \e[31mFAILED\e[0m"
    fi
}

# command line parsing
(
    set -x
    "${cmd}" -l /dev/null -o runs/x/a.bin -- tests/x/input.mic
    "${cmd}" -o /dev/null tests/x/input.mic
    "${cmd}" -o /runs/x/a.bin /dev/null
    "${cmd}" -o /dev/null -- /dev/null
    "${cmd}" -h
    "${cmd}" --listing /dev/null --debug 0xf --output runs/x/a.bin --help
    "${cmd}" -d
    "${cmd}" -d x
    "${cmd}" -d -x
    "${cmd}" -l
    "${cmd}" -l -x
    "${cmd}" -o
    "${cmd}" -o -x
    "${cmd}" -x
    "${cmd}"
    "${cmd}" -l runs/x/a.lst -- tests/x/input.mic
) >runs/test1/output.txt 2>&1
check "test1"

# parsing
(
    set -x
    "${cmd}" -l runs/test2/a.lst -o /dev/null test_parser.mic
    "${cmd}" -d 0xf -l runs/test2/b.lst -o /dev/null test_parser.mic
) >runs/test2/output0.txt 2>&1
rm runs/test2/output.txt
cat runs/test2/a.lst runs/test2/b.lst runs/test2/output0.txt > runs/test2/output.txt
check "test2"
