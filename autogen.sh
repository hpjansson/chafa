#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir
PROJECT=chafa
TEST_TYPE=-f
FILE=chafa/Makefile.am
DIE=0

MISSING_TOOLS=

MY_ECHO=$(which echo)
[ x$MY_ECHO = x ] && MY_ECHO=echo

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
        MISSING_TOOLS="${MISSING_TOOLS}autoconf "
        DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
        MISSING_TOOLS="${MISSING_TOOLS}automake "
        DIE=1
}

(libtoolize --version) < /dev/null > /dev/null 2>&1 || {
        MISSING_TOOLS="${MISSING_TOOLS}libtool "
        DIE=1
}

(pkg-config --version) < /dev/null > /dev/null 2>&1 || {
        MISSING_TOOLS="${MISSING_TOOLS}pkg-config "
        DIE=1
}

if test "$DIE" -eq 1; then
        ${MY_ECHO}
        ${MY_ECHO} -e "Missing mandatory tools:\e[1;31m $MISSING_TOOLS"
        ${MY_ECHO} -e "\e[0m"
        ${MY_ECHO} "These are required for building Chafa from its git repository."
        ${MY_ECHO} "You should be able to install them using your operating system's"
        ${MY_ECHO} "package manager (apt-get, yum, zypper or similar). Alternately"
        ${MY_ECHO} "they can be obtained directly from GNU: https://ftp.gnu.org/gnu/"
        ${MY_ECHO}
        ${MY_ECHO} "If you can't provide these tools, you may still be able to"
        ${MY_ECHO} "build Chafa from a tarball release: https://hpjansson.org/chafa/releases/"
        ${MY_ECHO}
fi

GTKDOCIZE=$(which gtkdocize 2>/dev/null)

if test -z $GTKDOCIZE; then
        ${MY_ECHO} -e "Missing optional tool:\e[1;33m gtk-doc"
        ${MY_ECHO} -e "\e[0m"
        ${MY_ECHO} "Without this, no developer documentation will be generated."
        ${MY_ECHO}
        rm -f gtk-doc.make
        cat > gtk-doc.make <<EOF
EXTRA_DIST =
CLEANFILES =
EOF
else
        gtkdocize || exit $?
fi

if test "$DIE" -eq 1; then
        exit 1
fi

test $TEST_TYPE $FILE || {
        ${MY_ECHO}
        ${MY_ECHO} "You must run this script in the top-level $PROJECT directory."
        ${MY_ECHO}
        exit 1
}

if test x$NOCONFIGURE = x && test -z "$*"; then
        ${MY_ECHO}
        ${MY_ECHO} "I am going to run ./configure with no arguments - if you wish "
        ${MY_ECHO} "to pass any to it, please specify them on the $0 command line."
        ${MY_ECHO}
fi

am_opt="--include-deps --add-missing"

${MY_ECHO} "Running libtoolize..."
libtoolize --force --copy

${MY_ECHO} "Running aclocal..."
aclocal $ACLOCAL_FLAGS

# optionally feature autoheader
(autoheader --version)  < /dev/null > /dev/null 2>&1 && autoheader

${MY_ECHO} "Running automake..."
automake -a $am_opt

${MY_ECHO} "Running autoconf..."
autoconf

cd $ORIGDIR

conf_flags="--enable-maintainer-mode"

if test x$NOCONFIGURE = x; then
  ${MY_ECHO} Running $srcdir/configure $conf_flags "$@" ...
  $srcdir/configure $conf_flags "$@" \
  && ${MY_ECHO} Now type \`make\' to compile $PROJECT || exit 1
else
  ${MY_ECHO} Skipping configure process.
fi
