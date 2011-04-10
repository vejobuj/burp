# burp version
VERSION = $(shell git describe)

# paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man

CC       ?= gcc
CFLAGS   += -std=c99 -g -pedantic -Wall -Wextra -Werror ${CPPFLAGS}
CPPFLAGS += -D_GNU_SOURCE -DVERSION=\"${VERSION}\"
LDFLAGS  += -lcurl

