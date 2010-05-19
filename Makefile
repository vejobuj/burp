# burp - seamless AUR uploading

include config.mk

SRC = burp.c cookies.c conf.c curl.c llist.c util.c
OBJ = ${SRC:.c=.o}

all: options burp doc

options:
	@echo burp build options:
	@echo "PREFIX    = ${PREFIX}"
	@echo "MANPREFIX = ${MANPREFIX}"
	@echo "CC        = ${CC}"
	@echo "CFLAGS    = ${CFLAGS}"
	@echo "LDFLAGS   = ${LDFLAGS}"

.c.o:
	@printf "   %-8s %s\n" CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

burp: ${OBJ}
	@printf "   %-8s %s\n" LD $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

doc: burp.1
burp.1: README.pod
	@printf "   %-8s %s\n" DOC burp.1
	@pod2man --section=1 --center=" " --release=" " --name="BURP" --date="burp-VERSION" README.pod > burp.1

dist: clean
	@mkdir -p burp-${VERSION}
	@cp -R ${SRC} *.h README.pod Makefile burp-${VERSION}
	@tar -cf burp-${VERSION}.tar burp-${VERSION}
	@printf "   %-8s %s\n" GZIP burp-${VERSION}.tar
	@gzip burp-${VERSION}.tar
	@rm -rf burp-${VERSION}

clean:
	@printf "   %-8s %s\n" CLEAN "*.o burp burp.1"
	@rm -f *.o burp burp.1

install: burp doc
	@echo installing executable to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f burp ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/burp
	@echo installing man page to ${DESTDIR}${MANPREFIX}
	@mkdir -p ${DESTDIR}/${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < burp.1 > ${DESTDIR}${MANPREFIX}/man1/burp.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/burp.1
	@echo installing bash completion function to ${DESTDIR}/etc/bash_completion.d/
	@mkdir -p ${DESTDIR}/etc/bash_completion.d
	@cp -f burp.bash_completion ${DESTDIR}/etc/bash_completion.d/burp

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/burp
	@echo removing man page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/burp.1
	@echo removing bash completion
	@rm -f ${DESTDIR}/etc/bash_completion.d/burp

.PHONY: all doc clean install options uninstall
