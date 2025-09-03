SUBDIRS = chafa docs examples libnsgif lodepng tools tests

SGR0=$$(tput sgr0 2>/dev/null)
SETAF9=$$(tput setaf 9 2>/dev/null)
SETAF11=$$(tput setaf 11 2>/dev/null)

EXTRA_DIST = \
	HACKING \
	README.md \
	SECURITY.md \
	autogen.sh \
	chafa.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = chafa.pc

all-local:
	@echo
	@echo ---
	@echo --- Success! You can now run tools/chafa/chafa, or install everything
	@echo --- using "make install" or "sudo make install".
	@echo ---
	@echo --- ${SETAF9}NOTE:${SGR0} You may have to run ${SETAF11}msudo ldconfig${SGR0} after installing.
	@echo ---
	@echo
