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

OBJECTS = main.o

BINARY = findpkgelf

default: all

all: $(BINARY)

.cpp.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BINARY): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

clean:
	-rm -f *.o $(BINARY)

depend:
	makedepend -include .cflags -Y $(OBJECTS_SRC) -w300 2> /dev/null
	-rm -f Makefile.bak
# DO NOT DELETE

main.o: main.h
