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

# some TAP producer helpers

lazy=0

inc() {
  lazy=$(expr $lazy + 1)
}

pass() {
  inc
  echo "ok $lazy - $1"
}

fail() {
  inc
  echo "not ok $lazy - $1"
}

skip() {
  inc
  echo "ok $lazy # SKIP $1"
}

finish() {
  echo "1..$lazy"
  exit $1
}

# vi: ts=8 sw=2 sts=2 et tw=80
