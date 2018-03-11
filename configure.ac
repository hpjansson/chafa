dnl ----------------------------
dnl Automake/autoconf input file
dnl ----------------------------

dnl --- Package configuration ---

AC_PREREQ(2.59)
AC_INIT([chafa], [0.0.0], [hpj@copyleft.no])
AM_INIT_AUTOMAKE([1.9 foreign])

AC_CONFIG_SRCDIR([chafa.pc.in])

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

PKG_CHECK_MODULES(MAGICKWAND, MagickWand >= 6)
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.10)

dnl --- Specific checks ---

dnl
dnl Check for -Bsymbolic-functions linker flag used to avoid
dnl intra-library PLT jumps, if available.
dnl

AC_ARG_ENABLE(Bsymbolic,
              [AS_HELP_STRING([--disable-Bsymbolic],
                              [avoid linking with -Bsymbolic])],,
              [SAVED_LDFLAGS="${LDFLAGS}" SAVED_LIBS="${LIBS}"
               AC_MSG_CHECKING([for -Bsymbolic linker flag])
               LDFLAGS=-Wl,-Bsymbolic
               LIBS=
               AC_TRY_LINK([], [return 0],
                           AC_MSG_RESULT(yes)
                           enable_Bsymbolic=yes,
                           AC_MSG_RESULT(no)
                           enable_Bsymbolic=no)
               LDFLAGS="${SAVED_LDFLAGS}" LIBS="${SAVED_LIBS}"])

if test "x${enable_Bsymbolic}" = "xyes"; then
  CHAFA_BDYNAMIC_FLAGS=-Wl,-Bsymbolic
fi

dnl
dnl Check for gcc x86 instruction set check builtins
dnl

AC_CACHE_CHECK([for gcc __builtin_cpu_init function],
        [ax_cv_gcc_check_x86_cpu_init],
        [AC_RUN_IFELSE(
                [AC_LANG_PROGRAM([#include <stdlib.h>],
                        [__builtin_cpu_init ();])],
                 [ax_cv_gcc_check_x86_cpu_init=yes],
                 [ax_cv_gcc_check_x86_cpu_init=no])])

AC_CACHE_CHECK([for gcc __builtin_cpu_supports function],
        [ax_cv_gcc_check_x86_cpu_supports],
        [AC_RUN_IFELSE(
                [AC_LANG_PROGRAM([#include <stdlib.h>],
                        [__builtin_cpu_supports ("mmx");])],
                 [ax_cv_gcc_check_x86_cpu_supports=yes],
                 [ax_cv_gcc_check_x86_cpu_supports=no])])

dnl AM_CONDITIONAL([HAVE_GCC_X86_FEATURE_BUILTINS], [test "$ac_cv_gcc_check_x86_cpu_init" = "yes" && test "$ac_cv_gcc_check_x86_cpu_supports" = "yes"])
AS_IF([test "$ax_cv_gcc_check_x86_cpu_init" = "yes" && test "$ax_cv_gcc_check_x86_cpu_supports" = "yes"], [
        AC_DEFINE([HAVE_GCC_X86_FEATURE_BUILTINS], [1], [Define if gcc x86 feature builtins work.])
])

dnl Check for working MMX intrinsics
AC_MSG_CHECKING(for working MMX intrinsics)
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
AC_MSG_CHECKING(for working SSE 4.1 intrinsics)
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
CHAFA_VISIBILITY_CFLAGS=""
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
      CHAFA_VISIBILITY_CFLAGS="-fvisibility=hidden"
    ])
    ;;
esac

AC_MSG_CHECKING(for 4-arg MagickResizeImage)
SAVED_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $MAGICKWAND_CFLAGS"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS $MAGICKWAND_LIBS"
AC_TRY_LINK([#include <wand/MagickWand.h>], [MagickResizeImage (0, 0, 0, 0);],
	AC_DEFINE([HAVE_MAGICK_RESIZE_IMAGE_4], [1], [Define if MagickResizeImage takes 4 arguments.])
        AC_MSG_RESULT(yes),
        AC_MSG_RESULT(no))
CFLAGS=$SAVED_CFLAGS
LDFLAGS=$SAVED_LDFLAGS

AC_MSG_CHECKING(for 5-arg MagickResizeImage)
SAVED_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $MAGICKWAND_CFLAGS"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS $MAGICKWAND_LIBS"
AC_TRY_LINK([#include <wand/MagickWand.h>], [MagickResizeImage (0, 0, 0, 0, 1.0);],
	AC_DEFINE([HAVE_MAGICK_RESIZE_IMAGE_5], [1], [Define if MagickResizeImage takes 5 arguments.])
        AC_MSG_RESULT(yes),
        AC_MSG_RESULT(no))
CFLAGS=$SAVED_CFLAGS
LDFLAGS=$SAVED_LDFLAGS

AC_MSG_CHECKING(for MagickAutoOrientImage)
SAVED_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $MAGICKWAND_CFLAGS"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS $MAGICKWAND_LIBS"
AC_TRY_LINK([#include <wand/MagickWand.h>], [MagickAutoOrientImage (0);],
	AC_DEFINE([HAVE_MAGICK_AUTO_ORIENT_IMAGE], [1], [Define if we have MagickAutoOrientImage.])
        AC_MSG_RESULT(yes),
        AC_MSG_RESULT(no))
CFLAGS=$SAVED_CFLAGS
LDFLAGS=$SAVED_LDFLAGS

dnl --- Set compiler flags ---

BASE_CFLAGS="-Wall -Wextra"

LIBCHAFA_CFLAGS="$BASE_CFLAGS $CHAFA_VISIBILITY_CFLAGS"
LIBCHAFA_LDFLAGS="$CHAFA_BDYNAMIC_FLAGS"

CHAFA_CFLAGS="$BASE_CFLAGS $CHAFA_VISIBILITY_CFLAGS"
CHAFA_LDFLAGS="$CHAFA_BDYNAMIC_FLAGS"

AC_SUBST(LIBCHAFA_CFLAGS)
AC_SUBST(LIBCHAFA_LDFLAGS)

AC_SUBST(CHAFA_CFLAGS)
AC_SUBST(CHAFA_LDFLAGS)

dnl --- Output ---

AC_OUTPUT(Makefile
          chafa/Makefile
          chafa.pc)
