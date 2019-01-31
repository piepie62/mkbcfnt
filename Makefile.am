# Makefile.am -- Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = mkbcfnt

mkbcfnt_SOURCES = source/main.cpp

mkbcfnt_LDADD = $(FreeType_LIBS)
AM_CPPFLAGS   = -I$(srcdir)/include -D_GNU_SOURCE $(FreeType_CFLAGS)
EXTRA_DIST = autogen.sh
