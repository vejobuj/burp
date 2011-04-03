# burp - seamless AUR uploading

include config.mk

SRC = burp.c conf.c curl.c util.c
OBJ = ${SRC:.c=.o}

all: burp doc

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

burp: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

doc: burp.1
burp.1: README.pod
	pod2man --section=1 --center="Burp Manual" --name="BURP" --release="burp ${VERSION}" README.pod > burp.1

dist: clean
	mkdir -p burp-${VERSION}
	cp -R ${SRC} *.h README.pod Makefile bash_completion burp-${VERSION}
	sed "s/^VERSION.*/VERSION = ${VERSION}/g" < config.mk > burp-${VERSION}/config.mk
	tar -cf burp-${VERSION}.tar burp-${VERSION}
	gzip burp-${VERSION}.tar
	${RM} -r burp-${VERSION}

clean:
	${RM} *.o burp burp.1

install: burp doc
	install -Dm755 burp ${DESTDIR}${PREFIX}/bin/burp
	install -Dm644 bash_completion ${DESTDIR}/etc/bash_completion.d/burp
	install -Dm644 burp.1 ${DESTDIR}${MANPREFIX}/man1/burp.1

uninstall:
	${RM} ${DESTDIR}${PREFIX}/bin/burp
	${RM} ${DESTDIR}${MANPREFIX}/man1/burp.1
	${RM} ${DESTDIR}/etc/bash_completion.d/burp

.PHONY: all doc clean install uninstall

