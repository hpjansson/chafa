SUBDIRS = internal
DISTCLEANFILES =
BUILT_SOURCES =

## --- Library ---

lib_LTLIBRARIES = libchafa.la
noinst_LTLIBRARIES =
noinst_HEADERS =

libchafa_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -DCHAFA_COMPILATION
libchafa_la_LDFLAGS = $(LIBCHAFA_LDFLAGS) -version-info 8:2:8
libchafa_la_LIBADD = $(GLIB_LIBS) internal/libchafa-internal.la -lm

libchafa_la_SOURCES = \
	chafa-canvas.c \
	chafa-canvas-config.c \
	chafa-features.c \
	chafa-symbol-map.c \
	chafa-term-db.c \
	chafa-term-info.c \
	chafa-util.c

chafaincludedir=$(includedir)/chafa
chafainclude_HEADERS = \
	chafa.h \
	chafa-canvas.h \
	chafa-canvas-config.h \
	chafa-common.h \
	chafa-features.h \
	chafa-symbol-map.h \
	chafa-term-db.h \
	chafa-term-info.h \
	chafa-term-seq-def.h \
	chafa-util.h \
	chafa-version-macros.h

# Generate header prototypes with docstrings (-CC to pass through comments)
# for terminal sequence accessors. These will be processed by gtk-doc to
# produce documentation.

noinst_HEADERS += chafa-term-seq-doc.h chafa-term-seq-doc-in.h
DISTCLEANFILES += chafa-term-seq-doc.h
BUILT_SOURCES += chafa-term-seq-doc.h

chafa-term-seq-doc.h: $(srcdir)/chafa-term-seq-doc-in.h
	$(CPP) -CC $< -o $@

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
