dnl TELEM_CHECK_SYSTEMD
dnl
dnl Check for existence and usability of systemd libraries
dnl
dnl First arg: human-readable lib name
dnl Second arg: uppercase variant
dnl
AC_DEFUN([TELEM_CHECK_SYSTEMD],
[have_lib=no
deprecatedlib_found=no
lib_found=no
AC_MSG_CHECKING([for systemd $1 support])
PKG_CHECK_EXISTS([libsystemd-$1],[deprecatedlib_found=yes],
                PKG_CHECK_EXISTS([libsystemd],[lib_found=yes],[]))

AS_IF([test "x$deprecatedlib_found" = "xyes"], [PKG_CHECK_MODULES([SYSTEMD_$2], [libsystemd-$1])],
      [test "x$lib_found" = "xyes"], [PKG_CHECK_MODULES([SYSTEMD_$2], [libsystemd])],
      [AC_MSG_RESULT([not found, skipping])])

test "x$deprecatedlib_found" = "xyes" -o "x$lib_found" = "xyes" && have_lib=yes
if test "x$have_lib" = "xyes"; then
  AC_CHECK_HEADERS([systemd/sd-$1.h])
fi
AM_CONDITIONAL([HAVE_SYSTEMD_$2], [test "x$have_lib" = "xyes"])
])
