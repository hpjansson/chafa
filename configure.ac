dnl ----------------------------
dnl Automake/autoconf input file
dnl ----------------------------

dnl --- Package configuration ---

m4_define([chafa_major_version], [1])
m4_define([chafa_minor_version], [14])
m4_define([chafa_micro_version], [5])

m4_define([chafa_version], [chafa_major_version.chafa_minor_version.chafa_micro_version])

AC_PREREQ([2.69])
AC_INIT([chafa],[chafa_version],[hpj@hpjansson.org])
AM_INIT_AUTOMAKE([1.9 foreign dist-xz no-dist-gzip -Wall])

AC_CONFIG_SRCDIR([chafa.pc.in])

AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_HEADERS(config.h)

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

BASE_CFLAGS="-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_26 \
-Wall -Wextra -Wmissing-prototypes -Wwrite-strings -Wunused-macros -Wundef \
-Wpointer-arith -Werror=format-security"

# May want to look into -Wconversion sometime. For now, it's just too much noise.

AC_USE_SYSTEM_EXTENSIONS
AM_SANITY_CHECK
AM_MAINTAINER_MODE

AC_C_CONST
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AM_PROG_AR

# We keep this obsolete macro around to allow configuration on older systems
# that require -std=c99, cf. CentOS 7. See github#113.
AC_PROG_CC_STDC

LT_INIT([win32-dll])

dnl --- Check for extra compiler warnings ---

AX_CHECK_COMPILE_FLAG([-Wstack-usage=131072],[BASE_CFLAGS="$BASE_CFLAGS -Wstack-usage=131072"],,[-Werror])
AX_CHECK_COMPILE_FLAG([-Wfor-loop-analysis],[BASE_CFLAGS="$BASE_CFLAGS -Wfor-loop-analysis"],,[-Werror])
AX_CHECK_COMPILE_FLAG([-Wlogical-op],[BASE_CFLAGS="$BASE_CFLAGS -Wlogical-op"],,[-Werror])
AX_CHECK_COMPILE_FLAG([-Wlogical-op-parentheses],[BASE_CFLAGS="$BASE_CFLAGS -Wlogical-op-parentheses"],,[-Werror])

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

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.26)

AC_ARG_WITH(tools,
  [AS_HELP_STRING([--without-tools], [don't build command-line tools [default=on]])],
  ,
  with_tools=yes)
AS_IF([test "$with_tools" != no], [
  dnl FreeType (required)
  PKG_CHECK_MODULES(FREETYPE, [freetype2 >= 2.0.0],,
    [AC_MSG_ERROR([You need freetype2-devel (or libfreetype6-dev on Debian) to build command-line tools, or pass --without-tools to build without.])])

  dnl libavif (optional)
  AC_ARG_WITH(avif,
    [AS_HELP_STRING([--without-avif], [don't build AVIF loader [default=on]])],
    ,
    with_avif=yes)
  AS_IF([test "$with_avif" != no], [
    PKG_CHECK_MODULES(AVIF, [libavif],,
      missing_rpms="$missing_rpms libavif-devel"
      missing_debs="$missing_debs libavif-dev"
      with_avif=no)])
  AS_IF([test "$with_avif" != no], [AC_DEFINE([HAVE_AVIF], [1], [Define if we have AVIF support.])])

  dnl libjpeg (optional)
  AC_ARG_WITH(jpeg,
    [AS_HELP_STRING([--without-jpeg], [don't build JPEG loader [default=on]])],
    ,
    with_jpeg=yes)
  AS_IF([test "$with_jpeg" != no], [PKG_CHECK_MODULES(JPEG, [libjpeg],,
    missing_rpms="$missing_rpms libjpeg-devel"
    missing_debs="$missing_debs libjpeg-dev"
    with_jpeg=no)])
  AS_IF([test "$with_jpeg" != no], [AC_DEFINE([HAVE_JPEG], [1], [Define if we have JPEG support.])])

  dnl librsvg (optional)
  AC_ARG_WITH(svg,
    [AS_HELP_STRING([--without-svg], [don't build SVG loader [default=on]])],
    ,
    with_svg=yes)
  AS_IF([test "$with_svg" != no], [PKG_CHECK_MODULES(SVG, [librsvg-2.0],,
    missing_rpms="$missing_rpms librsvg-devel"
    missing_debs="$missing_debs librsvg2-dev"
    with_svg=no)])
  AS_IF([test "$with_svg" != no], [AC_DEFINE([HAVE_SVG], [1], [Define if we have SVG support.])])

  dnl libtiff (optional)
  AC_ARG_WITH(tiff,
    [AS_HELP_STRING([--without-tiff], [don't build TIFF loader [default=on]])],
    ,
    with_tiff=yes)
  AS_IF([test "$with_tiff" != no], [PKG_CHECK_MODULES(TIFF, [libtiff-4],,
    missing_rpms="$missing_rpms libtiff-devel"
    missing_debs="$missing_debs libtiff-dev"
    with_tiff=no)])
  AS_IF([test "$with_tiff" != no], [AC_DEFINE([HAVE_TIFF], [1], [Define if we have TIFF support.])])

  dnl libwebp (optional)
  AC_ARG_WITH(webp,
    [AS_HELP_STRING([--without-webp], [don't build WebP loader [default=on]])],
    ,
    with_webp=yes)
  AS_IF([test "$with_webp" != no], [
    PKG_CHECK_MODULES(WEBP, [libwebp libwebpdemux],,
      missing_rpms="$missing_rpms libwebp-devel"
      missing_debs="$missing_debs libwebp-dev"
      with_webp=no)])
  AS_IF([test "$with_webp" != no], [AC_DEFINE([HAVE_WEBP], [1], [Define if we have WebP support.])])

  dnl libjxl (optional)
  AC_ARG_WITH(jxl,
    [AS_HELP_STRING([--without-jxl], [don't build JXL loader [default=on]])],
    ,
    with_jxl=yes)
  AS_IF([test "$with_jxl" != no], [
    PKG_CHECK_MODULES(JXL, [libjxl libjxl_threads],,
      missing_rpms="$missing_rpms libjxl-devel"
      missing_debs="$missing_debs libjxl-dev"
      with_jxl=no)])
  AS_IF([test "$with_jxl" != no], [AC_DEFINE([HAVE_JXL], [1], [Define if we have JXL support.])])
])

AM_CONDITIONAL([WANT_TOOLS], [test "$with_tools" != no])
AM_CONDITIONAL([HAVE_JPEG], [test "$with_tools" != no -a "$with_jpeg" != no])
AM_CONDITIONAL([HAVE_SVG], [test "$with_tools" != no -a "$with_svg" != no])
AM_CONDITIONAL([HAVE_TIFF], [test "$with_tools" != no -a "$with_tiff" != no])
AM_CONDITIONAL([HAVE_WEBP], [test "$with_tools" != no -a "$with_webp" != no])
AM_CONDITIONAL([HAVE_AVIF], [test "$with_tools" != no -a "$with_avif" != no])
AM_CONDITIONAL([HAVE_JXL], [test "$with_tools" != no -a "$with_jxl" != no])

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

AC_CHECK_FUNCS(ctermid getrandom mmap sigaction)
AC_CHECK_HEADERS(sys/ioctl.h termios.h windows.h)

dnl
dnl Define IS_WIN32_BUILD if we're building for Microsoft Windows. In order to
dnl get UTF-8 support in command-line arguments and environment vars, we need
dnl to build a resource file with windres and link it in.
dnl

is_windows_build=no
case "${host_os}" in
     cygwin*|mingw*)
         is_windows_build=yes
         ;;
esac

AM_CONDITIONAL([IS_WIN32_BUILD],
               [test "x$is_windows_build" = "xyes"])

AC_CHECK_TOOL([WINDRES], [windres], [:])

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
               AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[return 0]])],[AC_MSG_RESULT(yes)
                              enable_Bsymbolic=yes],[AC_MSG_RESULT(no)
                              enable_Bsymbolic=no])
               LDFLAGS="${SAVED_LDFLAGS}" LIBS="${SAVED_LIBS}"])

if test "x${enable_Bsymbolic}" = "xyes"; then
  CHAFA_BDYNAMIC_FLAGS=-Wl,-Bsymbolic
fi

dnl
dnl Check for runtime gcc x86 instruction set detection. Used in 'chafa-features.c'.
dnl

AC_CACHE_CHECK([for gcc __builtin_cpu_init function],
        [ax_cv_gcc_check_x86_cpu_init],
        [AC_LINK_IFELSE(
                [AC_LANG_PROGRAM([#include <stdlib.h>],
                        [__builtin_cpu_init ();])],
                 [ax_cv_gcc_check_x86_cpu_init=yes],
                 [ax_cv_gcc_check_x86_cpu_init=no])])

AC_CACHE_CHECK([for gcc __builtin_cpu_supports function],
        [ax_cv_gcc_check_x86_cpu_supports],
        [AC_LINK_IFELSE(
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
AC_LINK_IFELSE(
        [AC_LANG_PROGRAM(
                [[#include <mmintrin.h>]],
                [[__m64 t [2] = { 0 }; t [0] = _mm_setzero_si64 ();]])],
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
AC_LINK_IFELSE(
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
AC_LINK_IFELSE(
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
    CHAFA_VISIBILITY_CFLAGS="-fvisibility=hidden"
    ;;
  *)
    dnl On other compilers, check if we can do -fvisibility=hidden
    SAVED_CFLAGS="${CFLAGS}"
    CFLAGS="-fvisibility=hidden"
    AC_MSG_CHECKING([for -fvisibility=hidden compiler flag])
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[return 0]])],[AC_MSG_RESULT(yes)
                      enable_fvisibility_hidden=yes],[AC_MSG_RESULT(no)
                      enable_fvisibility_hidden=no])
    CFLAGS="${SAVED_CFLAGS}"

    AS_IF([test "${enable_fvisibility_hidden}" = "yes"], [
      AC_DEFINE([_CHAFA_EXTERN], [__attribute__((visibility("default"))) extern],
                [Defines how to decorate public symbols while building])
      CHAFA_VISIBILITY_CFLAGS="-fvisibility=hidden"
    ])
    ;;
esac

dnl
dnl We're not picky about floating point behavior, and this makes e.g.
dnl lrintf() a lot faster.
dnl
AX_CHECK_COMPILE_FLAG([-ffast-math],
    [BASE_CFLAGS="$BASE_CFLAGS -ffast-math"],
    ,
    [-Werror])

dnl --- Set compiler flags ---

dnl Disable some LodePNG features. In particular, the CRC feature, which would
dnl sometimes cause valid images to not load because of issues in the
dnl encoding software.

LODEPNG_FEATURES="\
-DLODEPNG_NO_COMPILE_ENCODER \
-DLODEPNG_NO_COMPILE_DISK \
-DLODEPNG_NO_COMPILE_CPP \
-DLODEPNG_NO_COMPILE_CRC"

LODEPNG_CFLAGS="$BASE_CFLAGS $LODEPNG_FEATURES"
LODEPNG_LDFLAGS="$CHAFA_BDYNAMIC_FLAGS"

LIBNSGIF_CFLAGS="$BASE_CFLAGS"
LIBNSGIF_LDFLAGS="$CHAFA_BDYNAMIC_FLAGS"

LIBCHAFA_CFLAGS="$BASE_CFLAGS $CHAFA_VISIBILITY_CFLAGS"
LIBCHAFA_LDFLAGS="$CHAFA_BDYNAMIC_FLAGS"

CHAFA_CFLAGS="$BASE_CFLAGS $CHAFA_VISIBILITY_CFLAGS $LODEPNG_FEATURES"
CHAFA_LDFLAGS="$CHAFA_BDYNAMIC_FLAGS"

AC_SUBST(LODEPNG_CFLAGS)
AC_SUBST(LODEPNG_LDFLAGS)

AC_SUBST(LIBNSGIF_CFLAGS)
AC_SUBST(LIBNSGIF_LDFLAGS)

AC_SUBST(LIBCHAFA_CFLAGS)
AC_SUBST(LIBCHAFA_LDFLAGS)

AC_SUBST(CHAFA_CFLAGS)
AC_SUBST(CHAFA_LDFLAGS)

AC_ARG_ENABLE(rpath,
              [AS_HELP_STRING([--enable-rpath],
                              [use rpath [default=no]])])

AM_CONDITIONAL(ENABLE_RPATH, test "$enable_rpath" == yes)

dnl --- Output ---

AC_CONFIG_FILES([Makefile
                 chafa/Makefile
                 chafa/chafaconfig.h
                 chafa/internal/Makefile
                 chafa/internal/smolscale/Makefile
                 libnsgif/Makefile
                 lodepng/Makefile
                 chafa.pc
                 docs/Makefile
                 docs/version.xml
                 tests/Makefile
                 tests/data/Makefile
                 tests/data/bad/Makefile
                 tests/data/good/Makefile
                 tools/Makefile
                 tools/chafa/Makefile
                 tools/completions/Makefile
                 tools/fontgen/Makefile])
AC_OUTPUT

dnl --- Print a neatly colorized summary ---

colorize_vars="
  enable_man
  ac_cv_mmx_intrinsics
  ac_cv_sse41_intrinsics
  ac_cv_avx2_intrinsics
  ac_cv_popcnt32_intrinsics
  ac_cv_popcnt64_intrinsics
  with_tools
  with_jpeg
  with_svg
  with_tiff
  with_webp
  with_jxl
  with_avif
"

dnl Only use colors if the terminal supports the aixterm-style bright ones (16 total).

cols=$(tput colors 2>/dev/null)

if test ${cols:--1} -ge 16; then
  normal=$(tput sgr0)
  red=$(tput setaf 9)
  green=$(tput setaf 10)
  yellow=$(tput setaf 11)
  blue=$(tput setaf 12)

  pyes=${green}yes${normal}
  pno=${red}no${normal}
  pyno=${yellow}no${normal}
else
  normal=
  red=
  green=
  yellow=
  blue=

  pyes=yes
  pno=no
  pyno=no
fi

dnl Gross. At least make sure eval arguments are sanitized.
for i in $colorize_vars; do
  eval state=\$$i
  if test x$state != xno; then state=yes; fi
  eval p$i=\$p$state
done

dnl gtk-doc needs special handling; since docs come pregenerated in the tarball, "no"
dnl here is less critical, so use a different color. Also, enable_gtk_doc can be empty.
if test x$enable_gtk_doc = xyes; then
  penable_gtk_doc=$pyes
else
  penable_gtk_doc=$pyno
fi

echo >&AS_MESSAGE_FD $normal
echo >&AS_MESSAGE_FD "Build man page .............. $penable_man"
echo >&AS_MESSAGE_FD "Rebuild API documentation ... $penable_gtk_doc (--enable-gtk-doc)"
echo >&AS_MESSAGE_FD "Support MMX ................. $pac_cv_mmx_intrinsics"
echo >&AS_MESSAGE_FD "Support SSE 4.1 ............. $pac_cv_sse41_intrinsics"
echo >&AS_MESSAGE_FD "Support AVX2 ................ $pac_cv_avx2_intrinsics"
echo >&AS_MESSAGE_FD "Support popcount32 .......... $pac_cv_popcnt32_intrinsics"
echo >&AS_MESSAGE_FD "Support popcount64 .......... $pac_cv_popcnt64_intrinsics"
echo >&AS_MESSAGE_FD
echo >&AS_MESSAGE_FD "Build command-line tool ..... $pwith_tools"

if test "x$with_tools" != xno; then
echo >&AS_MESSAGE_FD "With AVIF loader ............ $pwith_avif"
echo >&AS_MESSAGE_FD "With GIF loader ............. $pyes (internal)"
echo >&AS_MESSAGE_FD "With JPEG loader ............ $pwith_jpeg"
echo >&AS_MESSAGE_FD "With PNG loader ............. $pyes (internal)"
echo >&AS_MESSAGE_FD "With QOI loader ............. $pyes (internal)"
echo >&AS_MESSAGE_FD "With SVG loader ............. $pwith_svg"
echo >&AS_MESSAGE_FD "With TIFF loader ............ $pwith_tiff"
echo >&AS_MESSAGE_FD "With WebP loader ............ $pwith_webp"
echo >&AS_MESSAGE_FD "With JXL loader ............. $pwith_jxl"
echo >&AS_MESSAGE_FD "With XWD loader ............. $pyes (internal)"
fi

echo >&AS_MESSAGE_FD
echo >&AS_MESSAGE_FD "Install prefix .............. $blue$prefix$normal"
echo >&AS_MESSAGE_FD
echo >&AS_MESSAGE_FD "You can now type ${blue}gmake${normal} or ${blue}make${normal} to build the project."

dnl --- Warn about missing dependencies ---

dnl Remove leading spaces.
missing_rpms=$(echo $missing_rpms | sed 's/^ *//')
missing_debs=$(echo $missing_debs | sed 's/^ *//')

if test "x$missing_rpms" != x; then
echo >&AS_MESSAGE_FD
echo >&AS_MESSAGE_FD "Some optional libraries were not found. You may want to install these and"
echo >&AS_MESSAGE_FD "run configure again (package names may be different on your system)."
echo >&AS_MESSAGE_FD
echo >&AS_MESSAGE_FD "On Fedora, openSUSE or similar:"
echo >&AS_MESSAGE_FD "${blue}${missing_rpms}${normal}"
echo >&AS_MESSAGE_FD 
echo >&AS_MESSAGE_FD "On Debian, Ubuntu or similar:"
echo >&AS_MESSAGE_FD "${blue}${missing_debs}${normal}"
echo >&AS_MESSAGE_FD
fi
