DESTDIR    =
PREFIX     = /usr/local
BINDIR     = $(PREFIX)/bin
DATADIR    = $(PREFIX)/share
SYSCONFDIR = $(PREFIX)/etc
MANDIR     = $(DATADIR)/man
MAN1DIR    = $(MANDIR)/man1

PACKAGE_NAME := pkgdepdb

VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_PATCH := 4.1

CXX ?= clang++
CXXFLAGS += -std=c++11
CXXFLAGS += -Wall -Wextra -Werror -fno-rtti
CPPFLAGS += -DPKGDEPDB_V_MAJ=$(VERSION_MAJOR)
CPPFLAGS += -DPKGDEPDB_V_MIN=$(VERSION_MINOR)
CPPFLAGS += -DPKGDEPDB_V_PAT=$(VERSION_PATCH)

CPPFLAGS += -DPKGDEPDB_ETC="\"$(SYSCONFDIR)\""

LIBARCHIVE_CFLAGS =
LIBARCHIVE_LIBS   = -larchive
ZLIB_CFLAGS =
ZLIB_LIBS   = -lz

CPPFLAGS += $(LIBARCHIVE_CFLAGS)
LIBS     += $(LIBARCHIVE_LIBS)
CPPFLAGS += $(ZLIB_CFLAGS)
LIBS     += $(ZLIB_LIBS)

OBJECTS = main.o config.o package.o elf.o db.o db_format.o db_json.o filter.o

BINARY        = pkgdepdb
STATIC_BINARY = $(BINARY)-static
MANPAGES      = pkgdepdb.1

.PHONY: man manpages uninstall uninstall-bin uninstall-man static

default: all

all:      $(BINARY)
static:   $(STATIC_BINARY)
man:      $(MANPAGES)
manpages: $(MANPAGES)

.cpp.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BINARY): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

$(STATIC_BINARY): $(OBJECTS)
	libtool --mode=link $(CXX) $(LDFLAGS) -o $@ $(OBJECTS) -all-static $(LIBS)
	@echo "NOTE:"
	@echo "NOTE:"
	@echo "NOTE: The static version will NOT work with threads enabled!!!"
	@echo "NOTE:"
	@echo "NOTE:"

pkgdepdb.1: pkgdepdb.1.in
	sed -e "s,%%SYSCONFDIR%%,$(SYSCONFDIR),g" \
	    pkgdepdb.1.in \
	    > pkgdepdb.1

clean:
	-rm -f *.o $(BINARY) $(BINARY)-static
	-rm -f .cflags

install: install-bin install-man
uninstall: uninstall-bin uninstall-man
install-prefix:
	install -d -m755 $(DESTDIR)$(PREFIX)
install-bin: install-prefix
	install -d -m755            $(DESTDIR)$(BINDIR)
	install    -m755 $(BINARY)  $(DESTDIR)$(BINDIR)/$(BINARY)
uninstall-bin:
	rm -f $(DESTDIR)$(BINDIR)/$(BINARY)
install-man: $(MANPAGES) install-prefix
	install -d -m755            $(DESTDIR)$(MAN1DIR)
	install    -m644 pkgdepdb.1 $(DESTDIR)$(MAN1DIR)/pkgdepdb.1
uninstall-man:
	rm -f $(DESTDIR)$(MAN1DIR)/pkgdepdb.1

depend:
	makedepend -include .cflags -Y $(OBJECTS_SRC) -w300 2> /dev/null
	-rm -f Makefile.bak
# DO NOT DELETE

main.o: main.h
config.o: main.h
package.o: main.h
elf.o: main.h endian.h
db.o: main.h
db_format.o: main.h
db_json.o: main.h
filter.o: main.h
