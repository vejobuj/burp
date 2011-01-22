# burp version
VERSION = $(shell git describe)

# paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man

# includes and libs
LIBDIR = $(PREFIX)/lib
INCDIR = $(PREFIX)/include
CURLINC = $(INCDIR)/curl

INCS = -I. -I$(INCDIR) -I${CURLINC}
LIBS = -L$(LIBDIR) -lc -lcurl

# flags
CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\"
CFLAGS += -std=c89 -pedantic -Wall -Wextra ${INCS} ${CPPFLAGS}
LDFLAGS += ${LIBS}

# compiler and linker
CC = gcc -pipe -g
