# © 2008 André Prata <andreprata at bugflux dot org>
# See LICENSE file for license details.
include config.mk

SRC = lockx.c
OBJ = ${SRC:.c=.o}

all: options lockx

options:
	@echo lockx build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: config.mk

lockx: ${OBJ}
	@echo CC -o $@
	@${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	@echo cleaning
	@rm -f lockx ${OBJ} lockx-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p lockx-${VERSION}
	@cp -R LICENSE Makefile README config.mk ${SRC} lockx-${VERSION}
	@tar -cf lockx-${VERSION}.tar lockx-${VERSION}
	@gzip lockx-${VERSION}.tar
	@rm -rf lockx-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f lockx ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/lockx
	@chmod u+s ${DESTDIR}${PREFIX}/bin/lockx

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/lockx

.PHONY: all options clean dist install uninstall
