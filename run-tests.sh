#!/bin/sh

# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2023 vaxfpga <vaxfpga@users.noreply.github.com>

cmd="${1:-./cmicro2}"

check() {
    sed -Ei -e 's/(\+ \.\/cmicro2)(-\w+)?/\1/' "runs/$1/output.txt"

    if diff -q "runs/$1/output.txt" "tests/$1/output.txt" >/dev/null; then
        echo "test ${1#test}: \e[32mPASSED\e[0m"
    else
        echo "test ${1#test}: \e[31mFAILED\e[0m"
    fi
}

mkdir -p "./runs/test1"
# command line parsing
(
    set -x
    "${cmd}" -l/dev/null -oruns/x/a.bin -d0xf -- tests/x/input.mic
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
    "${cmd}" -o-x
    "${cmd}" -x
    "${cmd}"
    "${cmd}" -l runs/x/a.lst -- tests/x/input.mic
) >runs/test1/output.txt 2>&1
check "test1"

mkdir -p "./runs/test2"
# parsing
(
    set -x
    "${cmd}" -l runs/test2/a.lst -o /dev/null test_parser.mic
    "${cmd}" -d 0xf -l runs/test2/b.lst -o /dev/null test_parser.mic
) >runs/test2/output.txt 2>&1
cat runs/test2/*.lst >> runs/test2/output.txt
check "test2"

mkdir -p "./runs/test3"
# macros
(
    set -x
    "${cmd}" -d 0xf -l runs/test3/a.lst -o /dev/null test_macro.mic
) >runs/test3/output.txt 2>&1
cat runs/test3/*.lst >> runs/test3/output.txt
check "test3"
