CC=gcc -std=c99 -Wall -pedantic -g
VERSION=$(shell git describe --always)
CFLAGS=-pipe -O2 -D_GNU_SOURCE -DVERSION=\"${VERSION}\"
LDFLAGS=-lcurl
OBJ=llist.o

all: burp doc

burp: burp.c ${OBJ}
	${CC} ${CFLAGS} ${LDFLAGS} $< ${OBJ} -o $@

%.o: %.c %.h
	${CC} ${CFLAGS} $< -c

doc:
	pod2man --section=1 --center=" " --release=" " --date="burp-VERSION" burp.pod > burp.1

install: all
	@echo installing executable to ${DESTDIR}/usr/bin
	@mkdir -p ${DESTDIR}/usr/bin
	@cp -f burp ${DESTDIR}/usr/bin
	@chmod 755 ${DESTDIR}/usr/bin/burp
	@echo installing man page to ${DESTDIR}${MANPREFIX}
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < burp.1 > ${DESTDIR}${MANPREFIX}/man1/burp.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/burp.1


clean:
	@rm *.o burp burp.1

.PHONY: all burp doc install clean
