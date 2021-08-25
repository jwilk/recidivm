#!/bin/sh

# Copyright © 2021 Jakub Wilk <jwilk@jwilk.net>
# SPDX-License-Identifier: MIT

is_positive_int()
{
    case $1 in
        '')
            return 1;;
        0*)
            return 1;;
        *[!0-9]*)
            return 1;;
        *)
            return 0;;
    esac
}

set -e -u
echo 1..2
base="${0%/*}/.."
prog="${RECIDIVM_TEST_TARGET:-"$base/recidivm"}"
log=$(mktemp -t recidivm.test.XXXXXX)
ctx="recidivm -- false"
if n=$("$prog" -- false 2>"$log")
then
    echo "not ok 1 $ctx (\$? = $?)"
elif [ -n "$n" ]
then
    echo "not ok 1 $ctx"
else
    echo "ok 1 $ctx"
fi
sed -e 's/^/# /' < "$log"
ctx="recidivm -v -- true"
if n=$("$prog" -v -- true 2>"$log")
then
    if is_positive_int "$n"
    then
        echo "ok 2 $ctx"
    else
        echo "not ok 2 $ctx"
    fi
else
    echo "not ok 2 $ctx (\$? = $?)"
fi
sed -e 's/^/# /' < "$log"

# vim:ts=4 sts=4 sw=4 et ft=sh
