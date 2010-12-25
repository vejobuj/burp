# burp version
VERSION = $(shell git describe)

# paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man

# includes and libs
CURLINC = /usr/include/curl

INCS = -I. -I/usr/include -I${CURLINC}
LIBS = -L/usr/lib -lc -lcurl

# flags
CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\"
CFLAGS += -std=c89 -pedantic -Wall -Wextra ${INCS} ${CPPFLAGS}
LDFLAGS += ${LIBS}

# compiler and linker
CC = gcc -pipe -g
