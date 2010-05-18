CC      = gcc -std=c99 -Wall -pedantic -g
VERSION = $(shell git describe --always)
CFLAGS  = -pipe -O2 -D_GNU_SOURCE -DVERSION=\"${VERSION}\"
LDFLAGS = -lcurl
SRC     = burp.c cookies.c conf.c curl.c llist.c util.c
OBJ     = ${SRC:.c=.o}

all: burp doc

doc: burp.1

burp: ${OBJ}
	${CC} ${CFLAGS} ${LDFLAGS} ${OBJ} -o $@

.c.o:
	${CC} ${CFLAGS} -c $<

burp.1: README.pod
	pod2man --section=1 --center=" " --release=" " --name="BURP" --date="burp-VERSION" README.pod > burp.1

dist: clean
	@echo creating dist tarball
	@mkdir -p burp-${VERSION}
	@cp -R ${SRC} *.h README.pod Makefile burp-${VERSION}
	@tar -cf burp-${VERSION}.tar burp-${VERSION}
	@gzip burp-${VERSION}.tar
	@rm -rf burp-${VERSION}

clean:
	@rm -f *.o burp burp.1

install: all
	@echo installing executable to ${DESTDIR}/usr/bin
	@mkdir -p ${DESTDIR}/usr/bin
	@cp -f burp ${DESTDIR}/usr/bin
	@chmod 755 ${DESTDIR}/usr/bin/burp
	@echo installing man page to ${DESTDIR}${MANPREFIX}
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < burp.1 > ${DESTDIR}${MANPREFIX}/man1/burp.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/burp.1

uninstall:
	@echo removing executable file from ${DESTDIR}/usr/bin
	@rm -f ${DESTDIR}/usr/bin/burp
	@echo removing man page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/burp.1

.PHONY: all doc clean install uninstall
