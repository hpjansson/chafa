SUBDIRS = smolscale
DISTCLEANFILES =
BUILT_SOURCES =

## --- Library ---

lib_LTLIBRARIES = libchafa.la
noinst_LTLIBRARIES =

libchafa_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -DCHAFA_COMPILATION
libchafa_la_LDFLAGS = $(LIBCHAFA_LDFLAGS) -version-info 2:0:2
libchafa_la_LIBADD = $(GLIB_LIBS) smolscale/libsmolscale.la -lm

libchafa_la_SOURCES = \
	chafa-canvas.c \
	chafa-canvas-config.c \
	chafa-colors.c \
	chafa-features.c \
	chafa-private.h \
	chafa-symbol-map.c \
	chafa-symbols.c \
	chafa-symbols-ascii.h \
	chafa-util.c

chafaincludedir=$(includedir)/chafa
chafainclude_HEADERS = \
	chafa.h \
	chafa-canvas.h \
	chafa-canvas-config.h \
	chafa-features.h \
	chafa-symbol-map.h \
	chafa-util.h \
	chafa-version-macros.h

if HAVE_MMX_INTRINSICS
noinst_LTLIBRARIES += libchafa-mmx.la
libchafa_la_LIBADD += libchafa-mmx.la
libchafa_mmx_la_SOURCES = chafa-mmx.c
libchafa_mmx_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -mmmx -DCHAFA_COMPILATION
libchafa_mmx_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_SSE41_INTRINSICS
noinst_LTLIBRARIES += libchafa-sse41.la
libchafa_la_LIBADD += libchafa-sse41.la
libchafa_sse41_la_SOURCES = chafa-sse41.c
libchafa_sse41_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -msse4.1 -DCHAFA_COMPILATION
libchafa_sse41_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

if HAVE_POPCNT_INTRINSICS
noinst_LTLIBRARIES += libchafa-popcnt.la
libchafa_la_LIBADD += libchafa-popcnt.la
libchafa_popcnt_la_SOURCES = chafa-popcnt.c
libchafa_popcnt_la_CFLAGS = $(LIBCHAFA_CFLAGS) $(GLIB_CFLAGS) -mpopcnt -DCHAFA_COMPILATION
libchafa_popcnt_la_LDFLAGS = $(LIBCHAFA_LDFLAGS)
endif

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

## -- General ---

AM_CPPFLAGS = \
	-I$(top_srcdir)
