#!/bin/sh

curdir=$(basename $(pwd))

if [ $curdir != "scripts" ]; then
  echo "Must run this script within the scripts directory"
  exit 1
fi

which uncrustify &> /dev/null

if [ "$?" -ne 0 ]; then
  echo "Install uncrustify to continue"
  exit 1
fi

find .. -path "../*/*.[ch]" -exec uncrustify {} -c tm.cfg --no-backup \;

# vi: ts=8 sw=2 sts=2 et tw=80
