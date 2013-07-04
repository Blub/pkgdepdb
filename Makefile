DESTDIR =
PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin
DATADIR = $(PREFIX)/share
MANDIR  = $(DATADIR)/man
MAN1DIR = $(MANDIR)/man1

PACKAGE_NAME := pkgdepdb

VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_PATCH := 0

CXX ?= clang++
CXXFLAGS += -std=c++11
CXXFLAGS += -Wall -Wextra -Werror -fno-rtti
CPPFLAGS += -DPKGDEPDB_V_MAJ=$(VERSION_MAJOR)
CPPFLAGS += -DPKGDEPDB_V_MIN=$(VERSION_MINOR)
CPPFLAGS += -DPKGDEPDB_V_PAT=$(VERSION_PATCH)

LIBARCHIVE_CFLAGS =
LIBARCHIVE_LIBS   = -larchive
ZLIB_CFLAGS =
ZLIB_LIBS   = -lz

CPPFLAGS += $(LIBARCHIVE_CFLAGS)
LIBS     += $(LIBARCHIVE_LIBS)
CPPFLAGS += $(ZLIB_CFLAGS)
LIBS     += $(ZLIB_LIBS)

OBJECTS = main.o package.o elf.o db.o db_format.o

BINARY = pkgdepdb

.PHONY: uninstall uninstall-bin uninstall-man

default: all

all: $(BINARY)

.cpp.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BINARY): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

clean:
	-rm -f *.o $(BINARY)

install: install-bin install-man
uninstall: uninstall-bin uninstall-man
install-prefix:
	install -d -m755 $(DESTDIR)$(PREFIX)
install-bin: install-prefix
	install -d -m755            $(DESTDIR)$(BINDIR)
	install    -m755 $(BINARY)  $(DESTDIR)$(BINDIR)/$(BINARY)
uninstall-bin:
	rm -f $(DESTDIR)$(BINDIR)/$(BINARY)
install-man: install-prefix
	install -d -m755            $(DESTDIR)$(MAN1DIR)
	install    -m644 pkgdepdb.1 $(DESTDIR)$(MAN1DIR)/pkgdepdb.1
uninstall-man:
	rm -f $(DESTDIR)$(MAN1DIR)/pkgdepdb.1

depend:
	makedepend -include .cflags -Y $(OBJECTS_SRC) -w300 2> /dev/null
	-rm -f Makefile.bak
# DO NOT DELETE

main.o: main.h
package.o: main.h
elf.o: main.h
db.o: main.h
db_format.o: main.h
