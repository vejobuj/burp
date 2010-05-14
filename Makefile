CC=gcc -std=c99 -Wall -pedantic -g
VERSION=-DVERSION=\"$(shell git describe --always)\"
CFLAGS=-pipe -O2 -D_GNU_SOURCE
LDFLAGS=-lcurl
OBJ=llist.o

all: burp

burp: burp.c ${OBJ}
	${CC} ${CFLAGS} ${VERSION} ${LDFLAGS} $< ${OBJ} -o $@

%.o: %.c %.h
	${CC} ${CFLAGS} $< -c

install: all
	@echo installing executable to ${DESTDIR}/usr/bin
	mkdir -p ${DESTDIR}/usr/bin
	cp -f burp ${DESTDIR}/usr/bin
	chmod 755 ${DESTDIR}/usr/bin/burp

clean:
	@rm *.o burp

