#! /bin/bash

echo "generate the config.h"
VAR=$1;
file="$(sed -n 's/^AC_CONFIG_HEADERS(\([^)]*\))/\1/p' $1 | tr -d '[]')"
if [[ "$file" =~ ',' ]]; then
	echo Error: Invalid file name: $file
	exit 1
fi

if [[ "$file" = '' ]]; then
	file="config.h"
fi
echo "${VAR%/*}/$file"
cat > "${VAR%/*}/$file" << EOF
#pragma once

/* Define to the version of this package. */
#define PACKAGE_VERSION "$(sed -n 's/^AC_INIT([^,]\+,[ \t]*\([^,]*\).*)/\1/p' $1 | tr -d '[]')"

inline int malloc_trim(int size) {return size;}

/* qsort_r is not available in Android, so redefine it */
inline void qsort_r(void *__base, size_t __nmemb, size_t __size,
                    int * function, void *__arg) {}

EOF
