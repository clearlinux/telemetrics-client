#!/bin/sh

# This program is part of the Clear Linux Project
#
# Copyright 2015 Intel Corporation
#
# This program is free software; you can redistribute it and/or modify it under
# the terms and conditions of the GNU Lesser General Public License, as
# published by the Free Software Foundation; either version 2.1 of the License,
# or (at your option) any later version.
#
# This program is distributed in the hope it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
# A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
# details.

. $srcdir/tests/taplib.sh

## BEGIN TESTS ##

corept="/proc/sys/kernel/core_pattern"

if [ ! -f "$corept" ]; then
  skip "$corept does not exist"
  finish 0
else
  pass "found $corept"
fi

content=$(cat $corept)

if [ "$content" != "core" ]; then
  skip "$corept must contain string \"core\" to continue"
  finish 0
else
  pass "$corept contains \"core\""
fi

## END TESTS ##

finish 0

# vi: ts=8 sw=2 sts=2 et tw=80
