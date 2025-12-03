SUBDIRS = internal
DISTCLEANFILES =
BUILT_SOURCES =

## --- Library ---

libchafa_sources = \
	chafa-canvas-config.c \
	chafa-canvas.c \
	chafa-features.c \
	chafa-frame.c \
	chafa-image.c \
	chafa-parser.c \
	chafa-placement.c \
	chafa-stream-reader.c \
	chafa-stream-writer.c \
	chafa-symbol-map.c \
	chafa-term-db.c \
	chafa-term-info.c \
	chafa-term.c \
	chafa-util.c

chafaincludedir=$(includedir)/chafa

chafainclude_HEADERS = \
	chafa.h \
	chafa-canvas-config.h \
	chafa-canvas.h \
	chafa-common.h \
	chafa-features.h \
	chafa-frame.h \
	chafa-image.h \
	chafa-parser.h \
	chafa-placement.h \
	chafa-stream-reader.h \
	chafa-stream-writer.h \
	chafa-symbol-map.h \
	chafa-term-db.h \
	chafa-term-info.h \
	chafa-term-seq-def.h \
	chafa-term.h \
	chafa-util.h \
	chafa-version-macros.h

lib_LTLIBRARIES = libchafa.la
check_LTLIBRARIES = libchafa-test.la
noinst_LTLIBRARIES =
noinst_HEADERS =

libchafa_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(CHAFA_VISIBILITY_FLAGS) $(GLIB_CFLAGS) -DCHAFA_COMPILATION
libchafa_la_LDFLAGS = $(LIBCHAFA_LDFLAGS) -no-undefined -version-info 11:0:11
libchafa_la_LIBADD = $(GLIB_LIBS) internal/libchafa-internal.la -lm

libchafa_test_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -DCHAFA_COMPILATION
libchafa_test_la_LDFLAGS = $(LIBCHAFA_LDFLAGS) -no-undefined
libchafa_test_la_LIBADD = $(GLIB_LIBS) internal/libchafa-internal-test.la -lm

libchafa_la_SOURCES = $(libchafa_sources)
libchafa_test_la_SOURCES = $(libchafa_sources)

# Generate header prototypes with docstrings (-CC to pass through comments)
# for terminal sequence accessors. These will be processed by gtk-doc to
# produce documentation.

noinst_HEADERS += chafa-term-seq-doc.h chafa-term-seq-doc-in.h
DISTCLEANFILES += chafa-term-seq-doc.h
BUILT_SOURCES += chafa-term-seq-doc.h

chafa-term-seq-doc.h: $(srcdir)/chafa-term-seq-doc-in.h
	$(CPP) $(CPPFLAGS) -CC $< -o $@

# Generate chafaconfig.h
#
# The timestamp of the stamp file is used to indicate if chafaconfig.h is
# up to date with respect to config.status.  In the usual case, changes
# to config.status will not result in changes to chafaconfig.h, so we
# avoid touching its timestamp (so we don't rebuild the whole tree).

DISTCLEANFILES += chafaconfig-stamp chafaconfig.h
BUILT_SOURCES += chafaconfig-stamp
configexecincludedir = $(libdir)/chafa/include
nodist_configexecinclude_HEADERS = chafaconfig.h
chafaconfig-stamp: ../config.status
	$(AM_V_GEN) cd $(top_builddir) && \
	  $(SHELL) ./config.status chafa/chafaconfig.h
	@touch chafaconfig-stamp

## --- General ---

AM_CPPFLAGS = \
	-I$(top_srcdir)
