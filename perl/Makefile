CC ?= clang
PERLCFLAGS != perl -MExtUtils::Embed -e ccopts
PERLLDFLAGS != perl -MExtUtils::Embed -e ccdlflags

LIBS += -lpkgdepdb

CFLAGS += -fPIC $(PERLCFLAGS)
LDFLAGS += $(PERLLDFLAGS)

all: PKGDepDB/cmod.pm

pkgdepdb.i: pkgdepdb.i.in ../pkgdepdb.h
	cp pkgdepdb.i.in $@

pkgdepdb_wrap.c: pkgdepdb.i
	swig -perl5 -module 'PKGDepDB::cmod' pkgdepdb.i
	mv cmod.pm PKGDepDB/
pkgdepdb_wrap.o: pkgdepdb_wrap.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -shared -c pkgdepdb_wrap.c -o pkgdepdb_wrap.o
PKGDepDB/cmod.pm: pkgdepdb_wrap.o ../libpkgdepdb.la
	$(CC) $(LDFLAGS) -shared -o cmod.so pkgdepdb_wrap.o $(LIBS)

clean:
	-rm -f pkgdepdb.i
	-rm -f *.so *.c *.o *.la *.lo cmod.pm PKGDepDB/cmod.pm PKGDepDB/cmod.pm
