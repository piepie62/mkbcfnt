#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mkbcfnt], [0.0.1], [https://github.com/piepie62/mkbcfnt/issues])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([source/main.cpp])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CXX

AC_LANG_PUSH([C++])
AX_CHECK_COMPILE_FLAG([-Wall],      [CPPFLAGS+=" -Wall"])
AX_CHECK_COMPILE_FLAG([-pthread],   [CPPFLAGS+=" -pthread" LDFLAGS+=" -pthread"])
AX_CHECK_COMPILE_FLAG([-flto],      [CPPFLAGS+=" -flto" LDFLAGS+=" -flto"])
AX_CHECK_COMPILE_FLAG([-pipe],      [CPPFLAGS+=" -pipe"])
AX_CHECK_COMPILE_FLAG([-fno-exceptions], [CPPFLAGS+=" -fno-exceptions" LDFLAGS+=" -fno-exceptions"])
AX_CXX_COMPILE_STDCXX_11(noext, mandatory)
AC_LANG_POP()

AC_CHECK_PROGS([DOT], [dot])
AM_CONDITIONAL([HAVE_DOT], [test -n "$DOT"])
AM_COND_IF([HAVE_DOT], AC_SUBST(HAVE_DOT, ["YES"], AC_SUBST(HAVE_DOT, ["NO"])))

AC_DEFUN([PKG_CHECK_MODULES_STATIC],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
_save_PKG_CONFIG=$PKG_CONFIG
PKG_CONFIG="$PKG_CONFIG --static"
PKG_CHECK_MODULES([$1], [$2], [$3], [$4], [$5])
PKG_CONFIG=$_save_PKG_CONFIG[]dnl
])

# Checks for libraries.
PKG_CHECK_MODULES_STATIC(FreeType, [freetype2])
PKG_CHECK_MODULES_STATIC(ImageMagick, [Magick++ >= 6.0.0])

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for library functions.
AC_CHECK_FUNCS([strcasecmp])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
