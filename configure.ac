dnl ----------------------------
dnl Automake/autoconf input file
dnl ----------------------------

dnl --- Package configuration ---

m4_define([chafa_major_version], [1])
m4_define([chafa_minor_version], [3])
m4_define([chafa_micro_version], [0])

m4_define([chafa_version], [chafa_major_version.chafa_minor_version.chafa_micro_version])

AC_PREREQ(2.59)
AC_INIT([chafa], [chafa_version], [hpj@copyleft.no])
AM_INIT_AUTOMAKE([1.9 foreign])

AC_CONFIG_SRCDIR([chafa.pc.in])

AC_CONFIG_MACRO_DIRS([m4])
AM_CONFIG_HEADER(config.h)

CHAFA_MAJOR_VERSION=chafa_major_version
CHAFA_MINOR_VERSION=chafa_minor_version
CHAFA_MICRO_VERSION=chafa_micro_version
CHAFA_VERSION=chafa_version

AC_SUBST(CHAFA_MAJOR_VERSION)
AC_SUBST(CHAFA_MINOR_VERSION)
AC_SUBST(CHAFA_MICRO_VERSION)
AC_SUBST(CHAFA_VERSION)

AC_DEFINE(CHAFA_MAJOR_VERSION, [chafa_major_version], [Chafa major version])
AC_DEFINE(CHAFA_MINOR_VERSION, [chafa_minor_version], [Chafa minor version])
AC_DEFINE(CHAFA_MICRO_VERSION, [chafa_micro_version], [Chafa micro version])
AC_DEFINE_UNQUOTED(CHAFA_VERSION, "$CHAFA_VERSION", [Package version string])

dnl --- Standard setup ---

AM_SANITY_CHECK
AM_MAINTAINER_MODE

AC_C_CONST
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_CPP
AC_PROG_INSTALL
LT_INIT

dnl --- Required standards ---

dnl POSIX.1-2008 is required to get SA_RESETHAND. We should get this by default
dnl on most systems, but keep the check around just in case.

dnl AC_MSG_CHECKING(for POSIX.1-2008)
dnl AC_EGREP_CPP(posix_200809L_supported,
dnl              [#define _POSIX_C_SOURCE 200809L
dnl               #include <unistd.h>
dnl               #ifdef _POSIX_VERSION
dnl               # if _POSIX_VERSION == 200809L
dnl               posix_200809L_supported
dnl               # endif
dnl               #endif
dnl              ],
dnl              [AC_MSG_RESULT(yes)],
dnl              [AC_MSG_RESULT(no)
dnl               AC_MSG_FAILURE([Implementation must conform to the POSIX.1-2008 standard.])]
dnl )
dnl 
dnl AC_DEFINE([_POSIX_C_SOURCE], [200809L], [Minimum POSIX standard we need])

dnl --- Dependency check ---

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.10)

AC_ARG_WITH(tools,
  [AS_HELP_STRING([--without-tools], [don't build command-line tools [default=on]])],
  ,
  with_tools=yes)
AM_CONDITIONAL([WANT_TOOLS], [test "$with_tools" = "yes"])
AS_IF([test "$with_tools" != no], [
  PKG_CHECK_MODULES(MAGICKWAND, [MagickWand >= 6],,
    [AC_MSG_ERROR([You need ImageMagick-devel (or libmagickwand-dev on debian) to build command-line tools, or pass --without-tools to build without.])])])

# Used by gtk-doc's fixxref.
GLIB_PREFIX="`$PKG_CONFIG --variable=prefix glib-2.0`"
AC_SUBST(GLIB_PREFIX)

dnl --- Documentation ---

# gtkdocize greps for ^GTK_DOC_CHECK and parses it, so you need to have
# it on its own line.
m4_ifdef([GTK_DOC_CHECK], [
GTK_DOC_CHECK([1.20], [--flavour no-tmpl])
],[
AM_CONDITIONAL([ENABLE_GTK_DOC],[false])
])

AC_ARG_ENABLE(man,
              [AS_HELP_STRING([--enable-man],
                              [generate man pages [default=auto]])],,
              enable_man=maybe)

AS_IF([test "$enable_man" != no], [
  AC_PATH_PROG([XSLTPROC], [xsltproc])
  AS_IF([test -z "$XSLTPROC"], [
    AS_IF([test "$enable_man" = yes], [
      AC_MSG_ERROR([xsltproc is required for --enable-man])
    ])
    enable_man=no
  ])
])

AS_IF([ test "$enable_man" != no ], [
  dnl check for DocBook DTD in the local catalog
  JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.1.2//EN],
     [DocBook XML DTD V4.1.2], [have_docbook_dtd=yes], [have_docbook_dtd=no])
  AS_IF([test "$have_docbook_dtd" != yes], [
    AS_IF([test "$enable_man" = yes ], [
      AC_MSG_ERROR([DocBook DTD is required for --enable-man])
    ])
    enable_man=no
  ])
])

AS_IF([test "$enable_man" != no], [
  dnl check for DocBook XSL stylesheets in the local catalog
  JH_CHECK_XML_CATALOG([http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl],
     [DocBook XSL Stylesheets], [have_docbook_style=yes],[have_docbook_style=no])
  AS_IF([ test "$have_docbook_style" != yes ], [
    AS_IF([ test "$enable_man" = yes ], [
      AC_MSG_ERROR([DocBook XSL Stylesheets are required for --enable-man])
    ])
    enable_man=no
  ])
])

AM_CONDITIONAL(ENABLE_MAN, test "$enable_man" != no)

AC_MSG_CHECKING([whether to generate man pages])
AS_IF([ test "$enable_man" != no ], [
  enable_man=yes
  AC_MSG_RESULT([yes])
], [
  AC_MSG_RESULT([no])
])

dnl --- Specific checks ---

AC_CHECK_FUNCS(mmap)

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

dnl Check for working AVX2 intrinsics
AC_MSG_CHECKING(for working AVX2 intrinsics)
SAVED_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -mavx2"
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <immintrin.h>]],
		[[__m256i t = { 0 }; __m256i r = _mm256_abs_epi32 (t);]])],
	[AC_DEFINE([HAVE_AVX2_INTRINSICS], [1], [Define if AVX2 intrinsics work.])
	 ac_cv_avx2_intrinsics=yes],
	[ac_cv_avx2_intrinsics=no])
CFLAGS="${SAVED_CFLAGS}"
AC_MSG_RESULT(${ac_cv_avx2_intrinsics})
AM_CONDITIONAL([HAVE_AVX2_INTRINSICS], [test "$ac_cv_avx2_intrinsics" = "yes"])

dnl Check for working 64bit popcnt intrinsics
AC_MSG_CHECKING(for working 64bit popcnt intrinsics)
SAVED_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -mpopcnt"
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <stdint.h>
                  #include <nmmintrin.h>]],
		[[uint64_t t = 0; t = _mm_popcnt_u64 (t);]])],
	[AC_DEFINE([HAVE_POPCNT64_INTRINSICS], [1], [Define if 64bit popcnt intrinsics work.])
	 ac_cv_popcnt64_intrinsics=yes],
	[ac_cv_popcnt64_intrinsics=no])
CFLAGS="${SAVED_CFLAGS}"
AC_MSG_RESULT(${ac_cv_popcnt64_intrinsics})

dnl Check for working 32bit popcnt intrinsics
AC_MSG_CHECKING(for working 32bit popcnt intrinsics)
SAVED_CFLAGS="${CFLAGS}"
CFLAGS="${CFLAGS} -mpopcnt"
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <stdint.h>
                  #include <nmmintrin.h>]],
		[[uint32_t t = 0; t = _mm_popcnt_u32 (t);]])],
	[AC_DEFINE([HAVE_POPCNT32_INTRINSICS], [1], [Define if 32bit popcnt intrinsics work.])
	 ac_cv_popcnt32_intrinsics=yes],
	[ac_cv_popcnt32_intrinsics=no])
CFLAGS="${SAVED_CFLAGS}"
AC_MSG_RESULT(${ac_cv_popcnt32_intrinsics})

AM_CONDITIONAL([HAVE_POPCNT_INTRINSICS],
    [test "$ac_cv_popcnt64_intrinsics" = "yes" -o "$ac_cv_popcnt32_intrinsics" = "yes"])

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

dnl --- ImageMagick checks ---

dnl ImageMagick 6 needs #include <wand/MagickWand.h>
dnl ImageMagick 7 needs #include <MagickWand/MagickWand.h>

SAVED_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CFLAGS $MAGICKWAND_CFLAGS"
AC_CHECK_HEADERS([wand/MagickWand.h])
AC_CHECK_HEADERS([MagickWand/MagickWand.h])
CPPFLAGS=$SAVED_CPPFLAGS

AC_MSG_CHECKING(for 4-arg MagickResizeImage)
SAVED_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $MAGICKWAND_CFLAGS"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$MAGICKWAND_LIBS $LDFLAGS"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_WAND_MAGICKWAND_H
# include <wand/MagickWand.h>
#else
# include <MagickWand/MagickWand.h>
#endif
]], [[MagickResizeImage (0, 0, 0, 0);]])],
	AC_DEFINE([HAVE_MAGICK_RESIZE_IMAGE_4], [1], [Define if MagickResizeImage takes 4 arguments.])
        AC_MSG_RESULT(yes),
        AC_MSG_RESULT(no))
CFLAGS=$SAVED_CFLAGS
LDFLAGS=$SAVED_LDFLAGS

AC_MSG_CHECKING(for 5-arg MagickResizeImage)
SAVED_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $MAGICKWAND_CFLAGS"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$MAGICKWAND_LIBS $LDFLAGS"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_WAND_MAGICKWAND_H
# include <wand/MagickWand.h>
#else
# include <MagickWand/MagickWand.h>
#endif
]], [[MagickResizeImage (0, 0, 0, 0, 1.0);]])],
	AC_DEFINE([HAVE_MAGICK_RESIZE_IMAGE_5], [1], [Define if MagickResizeImage takes 5 arguments.])
        AC_MSG_RESULT(yes),
        AC_MSG_RESULT(no))
CFLAGS=$SAVED_CFLAGS
LDFLAGS=$SAVED_LDFLAGS

AC_MSG_CHECKING(for MagickAutoOrientImage)
SAVED_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS $MAGICKWAND_CFLAGS"
SAVED_LDFLAGS=$LDFLAGS
LDFLAGS="$MAGICKWAND_LIBS $LDFLAGS"
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_WAND_MAGICKWAND_H
# include <wand/MagickWand.h>
#else
# include <MagickWand/MagickWand.h>
#endif
]], [[MagickAutoOrientImage (0);]])],
	AC_DEFINE([HAVE_MAGICK_AUTO_ORIENT_IMAGE], [1], [Define if we have MagickAutoOrientImage.])
        AC_MSG_RESULT(yes),
        AC_MSG_RESULT(no))
CFLAGS=$SAVED_CFLAGS
LDFLAGS=$SAVED_LDFLAGS

dnl --- Set compiler flags ---

BASE_CFLAGS="-Wall -Wextra -Wmissing-prototypes"

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
          chafa/smolscale/Makefile
          libnsgif/Makefile
          chafa.pc
          docs/Makefile
          docs/version.xml
          tools/Makefile
          tools/chafa/Makefile
          tools/fontgen/Makefile)

echo >&AS_MESSAGE_FD
echo >&AS_MESSAGE_FD "Build command-line tool ..... $with_tools"
echo >&AS_MESSAGE_FD "Build man page .............. $enable_man"
echo >&AS_MESSAGE_FD "Rebuild API documentation ... ${enable_gtk_doc:-no} (--enable-gtk-doc)"
echo >&AS_MESSAGE_FD "Support MMX ................. $ac_cv_mmx_intrinsics"
echo >&AS_MESSAGE_FD "Support SSE 4.1 ............. $ac_cv_sse41_intrinsics"
echo >&AS_MESSAGE_FD "Support AVX2 ................ $ac_cv_avx2_intrinsics"
echo >&AS_MESSAGE_FD "Support popcount32 .......... $ac_cv_popcnt32_intrinsics"
echo >&AS_MESSAGE_FD "Support popcount64 .......... $ac_cv_popcnt64_intrinsics"
echo >&AS_MESSAGE_FD "Install prefix .............. $prefix"
echo >&AS_MESSAGE_FD
echo >&AS_MESSAGE_FD "You can now type \"gmake\" or \"make\" to build the project."
