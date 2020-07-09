AM_CFLAGS = $(CHAFA_CFLAGS) $(GLIB_CFLAGS)
LDADD = $(GLIB_LIBS) $(top_builddir)/chafa/libchafa.la

noinst_PROGRAMS = \
	term-info-test

term_info_test_SOURCES = \
	term-info-test.c

## -- General ---

## Include $(top_builddir)/chafa to get generated chafaconfig.h.

AM_CPPFLAGS = \
	-I$(top_srcdir)/chafa \
	-I$(top_builddir)/chafa
