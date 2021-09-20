# From GLib.
#
# Checks the location of the XML Catalog
# Usage:
#   JH_PATH_XML_CATALOG([ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# Defines XMLCATALOG and XML_CATALOG_FILE substitutions
AC_DEFUN([JH_PATH_XML_CATALOG],
[
  # check for the presence of the XML catalog
  AC_ARG_WITH([xml-catalog],
              AS_HELP_STRING([--with-xml-catalog=CATALOG],
                             [path to xml catalog to use]),,
              [with_xml_catalog=/etc/xml/catalog])
  jh_found_xmlcatalog=true
  XML_CATALOG_FILE="$with_xml_catalog"
  AC_SUBST([XML_CATALOG_FILE])
  AC_MSG_CHECKING([for XML catalog ($XML_CATALOG_FILE)])
  if test -f "$XML_CATALOG_FILE"; then
    AC_MSG_RESULT([found])
  else
    jh_found_xmlcatalog=false
    AC_MSG_RESULT([not found])
  fi

  # check for the xmlcatalog program
  AC_PATH_PROG(XMLCATALOG, xmlcatalog, no)
  if test "x$XMLCATALOG" = xno; then
    jh_found_xmlcatalog=false
  fi

  if $jh_found_xmlcatalog; then
    ifelse([$1],,[:],[$1])
  else
    ifelse([$2],,[AC_MSG_ERROR([could not find XML catalog])],[$2])
  fi
])

# From GLib.
#
# Checks if a particular URI appears in the XML catalog
# Usage:
#   JH_CHECK_XML_CATALOG(URI, [FRIENDLY-NAME], [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
AC_DEFUN([JH_CHECK_XML_CATALOG],
[
  AC_REQUIRE([JH_PATH_XML_CATALOG],[JH_PATH_XML_CATALOG(,[:])])dnl
  AC_MSG_CHECKING([for ifelse([$2],,[$1],[$2]) in XML catalog])
  if $jh_found_xmlcatalog && \
     AC_RUN_LOG([$XMLCATALOG --noout "$XML_CATALOG_FILE" "$1" >&2]); then
    AC_MSG_RESULT([found])
    ifelse([$3],,,[$3
])dnl
  else
    AC_MSG_RESULT([not found])
    ifelse([$4],,
       [AC_MSG_ERROR([could not find ifelse([$2],,[$1],[$2]) in XML catalog])],
       [$4])
  fi
])

# ===========================================================================
#  https://www.gnu.org/software/autoconf-archive/ax_check_compile_flag.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CHECK_COMPILE_FLAG(FLAG, [ACTION-SUCCESS], [ACTION-FAILURE], [EXTRA-FLAGS], [INPUT])
#
# DESCRIPTION
#
#   Check whether the given FLAG works with the current language's compiler
#   or gives an error.  (Warnings, however, are ignored)
#
#   ACTION-SUCCESS/ACTION-FAILURE are shell commands to execute on
#   success/failure.
#
#   If EXTRA-FLAGS is defined, it is added to the current language's default
#   flags (e.g. CFLAGS) when the check is done.  The check is thus made with
#   the flags: "CFLAGS EXTRA-FLAGS FLAG".  This can for example be used to
#   force the compiler to issue an error when a bad flag is given.
#
#   INPUT gives an alternative input source to AC_COMPILE_IFELSE.
#
#   NOTE: Implementation based on AX_CFLAGS_GCC_OPTION. Please keep this
#   macro in sync with AX_CHECK_{PREPROC,LINK}_FLAG.
#
# LICENSE
#
#   Copyright (c) 2008 Guido U. Draheim <guidod@gmx.de>
#   Copyright (c) 2011 Maarten Bosmans <mkbosmans@gmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

AC_DEFUN([AX_CHECK_COMPILE_FLAG],
[AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_IF
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_[]_AC_LANG_ABBREV[]flags_$4_$1])dnl
AC_CACHE_CHECK([whether _AC_LANG compiler accepts $1], CACHEVAR, [
  ax_check_save_flags=$[]_AC_LANG_PREFIX[]FLAGS
  _AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $4 $1"
  AC_COMPILE_IFELSE([m4_default([$5],[AC_LANG_PROGRAM()])],
    [AS_VAR_SET(CACHEVAR,[yes])],
    [AS_VAR_SET(CACHEVAR,[no])])
  _AC_LANG_PREFIX[]FLAGS=$ax_check_save_flags])
AS_VAR_IF(CACHEVAR,yes,
  [m4_default([$2], :)],
  [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_COMPILE_FLAGS
