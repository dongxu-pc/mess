# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = dwm.c
OBJ = ${SRC:.c=.o}

all: options dwm

options:
	@echo dwm build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk

dwm: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f dwm ${OBJ} dwm-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p dwm-${VERSION}
	@cp -R LICENSE Makefile README config.h config.mk \
		dwm.1 ${SRC} dwm-${VERSION}
	@tar -cf dwm-${VERSION}.tar dwm-${VERSION}
	@gzip dwm-${VERSION}.tar
	@rm -rf dwm-${VERSION}

install: all
	@echo installing executable file to ${PREFIX}/bin
	@mkdir -p ${PREFIX}/bin
	@cp -f dwm ${PREFIX}/bin
	@chmod 755 ${PREFIX}/bin/dwm
	@echo installing manual page to ${MANPREFIX}/man1
	@mkdir -p ${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g" < dwm.1 > ${MANPREFIX}/man1/dwm.1
	@chmod 644 ${MANPREFIX}/man1/dwm.1

uninstall:
	@echo removing executable file from ${PREFIX}/bin
	@rm -f ${PREFIX}/bin/dwm
	@echo removing manual page from ${MANPREFIX}/man1
	@rm -f ${MANPREFIX}/man1/dwm.1

.PHONY: all options clean dist install uninstall
