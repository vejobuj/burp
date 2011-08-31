# burp - seamless AUR uploading

VERSION   = $(shell git describe)

CPPFLAGS := -DVERSION=\"${VERSION}\" ${CPPFLAGS}
CFLAGS   := -std=c99 -g -pedantic -Wall -Wextra -Werror ${CFLAGS}
LDFLAGS  := -lcurl ${CFLAGS}

PREFIX    = /local/usr

SRC = burp.c conf.c curl.c util.c
OBJ = ${SRC:.c=.o}

all: burp doc

burp: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

doc: burp.1
burp.1: README.pod
	pod2man --section=1 --center="Burp Manual" --name="BURP" --release="burp ${VERSION}" README.pod > burp.1

dist: clean
	mkdir -p burp-${VERSION}
	cp -R ${SRC} *.h README.pod bash_completion burp-${VERSION}
	sed "s/^VERSION *.*/VERSION = ${VERSION}/" < Makefile > burp-${VERSION}/Makefile
	tar -czf burp-${VERSION}.tar.gz burp-${VERSION}
	${RM} -r burp-${VERSION}

clean:
	${RM} *.o burp burp.1

strip: burp
	strip --strip-all $<

install: burp doc
	install -Dm755 burp ${DESTDIR}${PREFIX}/bin/burp
	install -Dm644 bash_completion ${DESTDIR}/etc/bash_completion.d/burp
	install -Dm644 burp.1 ${DESTDIR}${PREFIX}/share/man/man1/burp.1

uninstall:
	${RM} ${DESTDIR}${PREFIX}/bin/burp
	${RM} ${DESTDIR}${MANPREFIX}/man1/burp.1
	${RM} ${DESTDIR}/etc/bash_completion.d/burp

.PHONY: all doc clean install uninstall

