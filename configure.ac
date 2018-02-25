dnl ----------------------------
dnl Automake/autoconf input file
dnl ----------------------------

dnl --- Package configuration ---

AC_PREREQ(2.59)
AC_INIT([chafa], [0.0.0], [hpj@copyleft.no])
AM_INIT_AUTOMAKE([1.9 foreign])

AC_CONFIG_MACRO_DIRS([m4])
AM_CONFIG_HEADER(config.h)

AC_SUBST(VERSION)
AC_SUBST(PACKAGE)

AC_DEFINE_UNQUOTED(VERSION, "$VERSION", [Package version])
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Package name])

AM_SANITY_CHECK
AM_MAINTAINER_MODE

AC_C_CONST
AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AM_PROG_LIBTOOL

PKG_CHECK_MODULES(ALL, \
  MagickWand >= 6 \
  glib-2.0 >= 2.10)

dnl --- Set up directories ---

privdatadir=$(eval echo "${datadir}/chafa")

AC_SUBST(privdatadir)

dnl --- Set compiler flags ---

BASE_CFLAGS="$BASE_CFLAGS -Wall"
CHAFA_LIBS="$BASE_LIBS `pkg-config --libs MagickWand glib-2.0` -lm"
CHAFA_CFLAGS="$BASE_CFLAGS `pkg-config --cflags MagickWand glib-2.0`"

AC_SUBST(CHAFA_LIBS)
AC_SUBST(CHAFA_CFLAGS)

dnl --- Output ---

AC_OUTPUT(Makefile                \
          chafa/Makefile)
