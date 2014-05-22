ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

bashcompletiondir=$(datarootdir)/bash-completion/completions
zshcompletiondir=$(datarootdir)/zsh/site-functions

EXTRA_DIST = \
	extra/bash-completion \
	extra/zsh-completion \
	README.pod

dist_man_MANS = \
	burp.1

bin_PROGRAMS = \
	burp

if USE_GIT_VERSION
GIT_VERSION := $(shell git describe --abbrev=4 --dirty | sed 's/^v//')
REAL_PACKAGE_VERSION = $(GIT_VERSION)
else
REAL_PACKAGE_VERSION = $(PACKAGE_VERSION)
endif

AM_CFLAGS = \
	-pthread \
	$(WARNINGFLAGS)

AM_LDFLAGS = \
	-pthread \
	$(GCLDFLAGS)

AM_CPPFLAGS = \
	-imacros $(top_builddir)/config.h \
	-I${top_srcdir}/src

if USE_GIT_VERSION
AM_CPPFLAGS += \
	-DGIT_VERSION=\"$(GIT_VERSION)\"
endif

burp_SOURCES = \
	src/aur.c src/aur.h \
	src/log.c src/log.h \
	src/burp.c \
	src/util.h

burp_CFLAGS = \
	$(AM_CFLAGS) \
	$(CURL_CFLAGS)

burp_LDADD = \
	$(CURL_LIBS)

burp.1: README.pod
	$(AM_V_GEN)$(POD2MAN) \
		--section=1 \
		--center="burp Manual" \
		--name="burp" \
		--release="burp $(REAL_PACKAGE_VERSION)" $< > $@

CLEANFILES = \
	$(dist_man_MANS)

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(bashcompletiondir)
	$(INSTALL_DATA) $(top_srcdir)/extra/bash-completion $(DESTDIR)$(bashcompletiondir)/burp
	$(MKDIR_P) $(DESTDIR)$(zshcompletiondir)
	$(INSTALL_DATA) $(top_srcdir)/extra/zsh-completion $(DESTDIR)$(zshcompletiondir)/_burp

uninstall-local:
	$(RM) $(DESTDIR)$(bashcompletiondir)/burp
	$(RM) $(DESTDIR)$(zshcompletiondir)/_burp

# custom targets
upload: all distcheck
	gpg --detach-sign burp-$(VERSION).tar.xz
	scp burp-$(VERSION).tar.xz burp-$(VERSION).tar.xz.sig code.falconindy.com:archive/burp/

fmt:
	clang-format -i -style=Google $(burp_SOURCES)
