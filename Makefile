DESTDIR :=
PREFIX  := /usr/local
BINDIR  := $(PREFIX)/bin
DATADIR := $(PREFIX)/share
MANDIR  := $(DATADIR)/man

PACKAGE_NAME := readpkgelf

CXX ?= clang++
CXXFLAGS += -std=c++11
CXXFLAGS += -Wall -Wextra -Werror -fno-rtti

LIBARCHIVE_CFLAGS :=
LIBARCHIVE_LIBS   := -larchive
LLVM_CFLAGS := `llvm-config --cppflags`
LLVM_LIBS   := `llvm-config --ldflags --libs object`

CPPFLAGS += $(LIBARCHIVE_CFLAGS)
CPPFLAGS += $(LLVM_CFLAGS)
LIBS     += $(LIBARCHIVE_LIBS)
LIBS     += $(LLVM_LIBS)

OBJECTS = main.o package.o

BINARY = readpkgelf

default: all

all: $(BINARY)

.cpp.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BINARY): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

clean:
	-rm -f *.o $(BINARY)

install: install-bin install-man
install-prefix:
	install -d -m755 $(DESTDIR)$(PREFIX)
install-bin: install-prefix
	install -d -m755              $(DESTDIR)$(BINDIR)
	install    -m755 $(BINARY)    $(DESTDIR)$(BINDIR)/$(BINARY)
install-man: install-prefix
	install -d -m755              $(DESTDIR)$(MANDIR)/man1
	install    -m644 readpkgelf.1 $(DESTDIR)$(MANDIR)/man1/readpkgelf.1

depend:
	makedepend -include .cflags -Y $(OBJECTS_SRC) -w300 2> /dev/null
	-rm -f Makefile.bak
# DO NOT DELETE

main.o: main.h
package.o: main.h
