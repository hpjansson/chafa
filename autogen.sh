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

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
        MISSING_TOOLS="${MISSING_TOOLS}autoconf "
        DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
        MISSING_TOOLS="${MISSING_TOOLS}automake "
        DIE=1
}

(libtool --version) < /dev/null > /dev/null 2>&1 || {
        MISSING_TOOLS="${MISSING_TOOLS}libtool "
        DIE=1
}

if test "$DIE" -eq 1; then
        echo
        echo "The following tools were not found: $MISSING_TOOLS"
        echo
        echo "These are required for building Chafa from its git repository."
        echo "You should be able to install them using your operating system's"
        echo "package manager (apt-get, yum, zypper or similar). Alternately"
        echo "they can be obtained directly from GNU: http://ftp.gnu.org/gnu/"
        echo
        exit 1
fi

test $TEST_TYPE $FILE || {
        echo
        echo "You must run this script in the top-level $PROJECT directory"
        echo
        exit 1
}

if test -z "$*"; then
        echo
        echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
        echo
fi

am_opt="--include-deps --add-missing"

echo "Running libtoolize..."
libtoolize --force --copy

echo "Running aclocal..."
aclocal $ACLOCAL_FLAGS

# optionally feature autoheader
(autoheader --version)  < /dev/null > /dev/null 2>&1 && autoheader

echo "Running automake..."
automake -a $am_opt

echo "Running autoconf..."
autoconf

cd $ORIGDIR
$srcdir/configure --enable-maintainer-mode "$@"
