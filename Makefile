DESTDIR    =
PREFIX     = /usr/local
BINDIR     = $(PREFIX)/bin
LIBDIR     = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include
DATADIR    = $(PREFIX)/share
SYSCONFDIR = $(PREFIX)/etc
MANDIR     = $(DATADIR)/man
MAN1DIR    = $(MANDIR)/man1

PACKAGE_NAME := pkgdepdb

API_VERSION  := 1
API_REVISION := 0
API_AGE      := 0

VERSION_MAJOR := 0
VERSION_MINOR := 1
VERSION_PATCH := 11

CXX ?= clang++
CXXFLAGS += -std=c++11
CXXFLAGS += -Wall -Wextra -Werror -Wno-unknown-pragmas -fno-rtti

LTVERSION = -version-info $(API_VERSION):$(API_REVISION):$(API_AGE)

LIBARCHIVE_CFLAGS =
LIBARCHIVE_LIBS   = -larchive
ZLIB_CFLAGS =
ZLIB_LIBS   = -lz

CPPFLAGS += $(LIBARCHIVE_CFLAGS)
LIBS     += $(LIBARCHIVE_LIBS)
CPPFLAGS += $(ZLIB_CFLAGS)
LIBS     += $(ZLIB_LIBS)

OBJECTS  = config.o package.o elf.o db.o db_format.o db_json.o filter.o
MAIN_OBJ = main.o
LIB_OBJ  = capi_common.o capi_config.o capi_elf.o capi_package.o capi_db.o

TEST_SRC = tests/ca_config.c tests/ca_elf.c tests/ca_package.c
TEST_OBJ = $(TEST_SRC:.c=.o)

OBJECTS_SRC = $(OBJECTS:.o=.cpp) $(MAIN_OBJ:.o=.cpp) $(LIB_OBJ:.o=.cpp)

BINARY        = pkgdepdb
STATIC_BINARY = $(BINARY)-static
MANPAGES      = pkgdepdb.1

.PHONY: static \
        man manpages \
        uninstall uninstall-bin uninstall-lib uninstall-man \
        install   install-bin   install-lib   install-man \
        check c-check py-check

default: all

all:      $(BINARY) $(LIBPKGDEPDB_LA)
static:   $(STATIC_BINARY)
man:      $(MANPAGES)
manpages: $(MANPAGES)

main.h: config.h
config.h: config.h.in Makefile Makefile.pre BSDmakefile GNUmakefile
	sed -e 's/@@V_MAJOR@@/$(VERSION_MAJOR)/g' \
	    -e 's/@@V_MINOR@@/$(VERSION_MINOR)/g' \
	    -e 's/@@V_PATCH@@/$(VERSION_PATCH)/g' \
	    -e 's|@@ETC@@|"$(SYSCONFDIR)"|g' \
	    -e 's/@@GIT_INFO@@/"$(GIT_INFO)"/g' \
	    -e 's/@@ENABLE_THREADS@@/$(ENABLE_THREADS)/g' \
	    -e 's/@@ENABLE_ALPM@@/$(ENABLE_ALPM)/g' \
	    -e 's/@@ENABLE_REGEX@@/$(ENABLE_REGEX)/g' \
	    config.h.in > config.h

setup.py: setup.py.in
	sed -e 's/@@API_VERSION@@/$(API_VERSION)/g' \
	    setup.py.in > setup.py

.cpp.o:
	$(LTCXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BINARY): $(MAIN_OBJ) $(OBJECTS)
	$(LTLD) $(LDFLAGS) -o $@ $(LTMAIN_OBJ) $(LTOBJECTS) $(LIBS)

$(STATIC_BINARY): $(MAIN_OBJ) $(OBJECTS)
	$(LTLD) $(LDFLAGS) -o $@ $(LTMAIN_OBJ) $(LTOBJECTS) -all-static $(LIBS)
	@echo "NOTE:"
	@echo "NOTE:"
	@echo "NOTE: The static version will NOT work with threads enabled!!!"
	@echo "NOTE:"
	@echo "NOTE:"

$(LIBPKGDEPDB_LA): $(LIB_OBJ) $(OBJECTS)
	$(LTLD) $(LTVERSION) $(LDFLAGS) -o $@ $(LTLIB_OBJ) $(LTOBJECTS) -rpath $(LIBDIR) $(LIBS)

pkgdepdb.1: pkgdepdb.1.in
	sed -e "s,%%SYSCONFDIR%%,$(SYSCONFDIR),g" \
	    pkgdepdb.1.in \
	    > pkgdepdb.1

clean:
	-rm -f *.o *.lo *.la $(BINARY) $(BINARY)-static $(LIBPKGDEPDB_LA)
	-rm -f .cflags
	-rm -rf .libs
	-rm -f setup.py

install: install-bin $(INSTALL_LIB) install-man
uninstall: uninstall-bin $(UNINSTALL_LIB) uninstall-man
install-prefix:
	install -d -m755 $(DESTDIR)$(PREFIX)
install-bin: install-prefix
	install -d -m755            $(DESTDIR)$(BINDIR)
	install    -m755 $(BINARY)  $(DESTDIR)$(BINDIR)/$(BINARY)
uninstall-bin:
	rm -f $(DESTDIR)$(BINDIR)/$(BINARY)
install-lib: install-prefix
	install -d -m755            $(DESTDIR)$(INCLUDEDIR)
	install    -m644 pkgdepdb.h $(DESTDIR)$(INCLUDEDIR)/pkgdepdb.h
	install -d -m755 $(DESTDIR)$(LIBDIR)
	$(LIBTOOL) --mode=install \
	  install -m755 $(LIBPKGDEPDB_LA) $(DESTDIR)$(LIBDIR)/$(LIBPKGDEPDB_LA)
	@echo
	@echo 'To install the python bindings run: make setup.py'
	@echo 'Then run setup.py as usual. Both python2 and python3 should work.'
uninstall-lib:
	rm -f $(DESTDIR)$(INCLUDEDIR)/pkgdepdb.h
	$(LIBTOOL) --mode=uninstall rm -f $(DESTDIR)$(LIBDIR)/$(LIBPKGDEPDB_LA)
install-man: $(MANPAGES) install-prefix
	install -d -m755            $(DESTDIR)$(MAN1DIR)
	install    -m644 pkgdepdb.1 $(DESTDIR)$(MAN1DIR)/pkgdepdb.1
uninstall-man:
	rm -f $(DESTDIR)$(MAN1DIR)/pkgdepdb.1

depend:
	makedepend -include .cflags -Y $(OBJECTS_SRC) -w300 2> /dev/null
	-rm -f Makefile.bak

.libs/libpkgdepdb.a: $(LIBPKGDEPDB_LA)
check:
	@echo "Running testsuite for the C API"
	$(MAKE) c-check
	@echo "Running testsuite for the python interface using $(PYTHON)"
	$(MAKE) py-check

c-check: .libs/libpkgdepdb.a
	$(CC) -c -o tests/ca_config.o tests/ca_config.c
	$(CXX) -o tests/ca_config tests/ca_config.o .libs/libpkgdepdb.a -lcheck $(LDFLAGS) $(LIBS)
	tests/ca_config
	$(CC) -c -o tests/ca_elf.o tests/ca_elf.c
	$(CXX) -o tests/ca_elf tests/ca_elf.o .libs/libpkgdepdb.a -lcheck $(LDFLAGS) $(LIBS)
	tests/ca_elf
	$(CC) -c -o tests/ca_package.o tests/ca_package.c
	$(CXX) -o tests/ca_package tests/ca_package.o .libs/libpkgdepdb.a -lcheck $(LDFLAGS) $(LIBS)
	tests/ca_package
	$(CC) -c -o tests/ca_db.o tests/ca_db.c
	$(CXX) -o tests/ca_db tests/ca_db.o .libs/libpkgdepdb.a -lcheck $(LDFLAGS) $(LIBS)
	tests/ca_db

py-check: .libs/libpkgdepdb.a
	LD_LIBRARY_PATH=.libs PYTHONPATH=. $(PYTHON) tests/pa_config.py
	LD_LIBRARY_PATH=.libs PYTHONPATH=. $(PYTHON) tests/pa_elf.py
	LD_LIBRARY_PATH=.libs PYTHONPATH=. $(PYTHON) tests/pa_package.py
	LD_LIBRARY_PATH=.libs PYTHONPATH=. $(PYTHON) tests/pa_db.py

# DO NOT DELETE

config.o: .cflags main.h util.h config.h
package.o: .cflags main.h util.h config.h elf.h package.h
elf.o: .cflags elf.h main.h util.h config.h endian.h
db.o: .cflags main.h util.h config.h elf.h package.h db.h filter.h
db_format.o: .cflags main.h util.h config.h elf.h package.h db.h db_format.h
db_json.o: .cflags main.h util.h config.h elf.h package.h db.h filter.h
filter.o: .cflags main.h util.h config.h elf.h package.h db.h filter.h
main.o: .cflags main.h util.h config.h elf.h package.h db.h filter.h
capi_config.o: .cflags main.h util.h config.h elf.h package.h db.h pkgdepdb.h
capi_elf.o: .cflags main.h util.h config.h elf.h package.h db.h pkgdepdb.h
capi_package.o: .cflags main.h util.h config.h elf.h package.h db.h pkgdepdb.h capi_algorithm.h
capi_db.o: .cflags main.h util.h config.h elf.h package.h db.h pkgdepdb.h capi_algorithm.h
