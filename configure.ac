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

dnl --- Standard setup ---

AM_SANITY_CHECK
AM_MAINTAINER_MODE

AC_C_CONST
AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AM_PROG_LIBTOOL

dnl --- Dependency check ---

PKG_CHECK_MODULES(ALL, \
  MagickWand >= 6 \
  glib-2.0 >= 2.10)

dnl --- Specific checks ---

dnl
dnl Check for -Bsymbolic-functions linker flag used to avoid
dnl intra-library PLT jumps, if available.
dnl

AC_ARG_ENABLE(Bsymbolic,
              [AS_HELP_STRING([--disable-Bsymbolic],
                              [avoid linking with -Bsymbolic])],,
              [SAVED_LDFLAGS="${LDFLAGS}" SAVED_LIBS="${LIBS}"
               AC_MSG_CHECKING([for -Bsymbolic-functions linker flag])
               LDFLAGS=-Wl,-Bsymbolic-functions
               LIBS=
               AC_TRY_LINK([], [return 0],
                           AC_MSG_RESULT(yes)
                           enable_Bsymbolic=yes,
                           AC_MSG_RESULT(no)
                           enable_Bsymbolic=no)
               LDFLAGS="${SAVED_LDFLAGS}" LIBS="${SAVED_LIBS}"])

if test "x${enable_Bsymbolic}" = "xyes"; then
  CHAFA_LINK_FLAGS=-Wl,-Bsymbolic-functions
fi

dnl Check for working MMX intrinsics
AC_MSG_CHECKING(working MMX intrinsics)
SAVED_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -mmmx"
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM(
                [[#include <mmintrin.h>]],
                [[_mm_empty ();]])],
        [AC_DEFINE([HAVE_MMX_INTRINSICS], [1], [Define if MMX intrinsics work.])
         ac_cv_mmx_intrinsics=yes],
        [ac_cv_mmx_intrinsics=no])
CFLAGS="${SAVED_CFLAGS}"
AC_MSG_RESULT(${ac_cv_mmx_intrinsics})
AM_CONDITIONAL([HAVE_MMX_INTRINSICS], [test "$ac_cv_mmx_intrinsics" = "yes"])

dnl Check for working SSE intrinsics
AC_MSG_CHECKING(working SSE 4.1 intrinsics)
SAVED_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -msse4.1"
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <smmintrin.h>]],
		[[__m128i t = { 0 }; int r = _mm_test_all_ones (t);]])],
	[AC_DEFINE([HAVE_SSE41_INTRINSICS], [1], [Define if SSE 4.1 intrinsics work.])
	 ac_cv_sse41_intrinsics=yes],
	[ac_cv_sse41_intrinsics=no])
CFLAGS="${SAVED_CFLAGS}"
AC_MSG_RESULT(${ac_cv_sse41_intrinsics})
AM_CONDITIONAL([HAVE_SSE41_INTRINSICS], [test "$ac_cv_sse41_intrinsics" = "yes"])

dnl
dnl Check for -fvisibility=hidden to determine if we can do GNU-style
dnl visibility attributes for symbol export control
dnl
CHAFA_HIDDEN_VISIBILITY_CFLAGS=""
case "$host" in
  *-*-mingw*)
    dnl On mingw32 we do -fvisibility=hidden and __declspec(dllexport)
    AC_DEFINE([_CHAFA_EXTERN], [__attribute__((visibility("default"))) __declspec(dllexport) extern],
              [Defines how to decorate public symbols while building])
    CFLAGS="${CFLAGS} -fvisibility=hidden"
    ;;
  *)
    dnl On other compilers, check if we can do -fvisibility=hidden
    SAVED_CFLAGS="${CFLAGS}"
    CFLAGS="-fvisibility=hidden"
    AC_MSG_CHECKING([for -fvisibility=hidden compiler flag])
    AC_TRY_COMPILE([], [return 0],
                   AC_MSG_RESULT(yes)
                   enable_fvisibility_hidden=yes,
                   AC_MSG_RESULT(no)
                   enable_fvisibility_hidden=no)
    CFLAGS="${SAVED_CFLAGS}"

    AS_IF([test "${enable_fvisibility_hidden}" = "yes"], [
      AC_DEFINE([_CHAFA_EXTERN], [__attribute__((visibility("default"))) extern],
                [defines how to decorate public symbols while building])
      CHAFA_HIDDEN_VISIBILITY_CFLAGS="-fvisibility=hidden"
    ])
    ;;
esac
AC_SUBST(CHAFA_HIDDEN_VISIBILITY_CFLAGS)

dnl --- Set compiler flags ---

BASE_CFLAGS="$BASE_CFLAGS -Wall"
CHAFA_LIBS="$BASE_LIBS `pkg-config --libs MagickWand glib-2.0` -lm"
CHAFA_CFLAGS="$BASE_CFLAGS `pkg-config --cflags MagickWand glib-2.0`"

AC_SUBST(CHAFA_LIBS)
AC_SUBST(CHAFA_CFLAGS)

dnl --- Output ---

AC_OUTPUT(Makefile                \
          chafa/Makefile)
