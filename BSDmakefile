GIT_INFO != GIT_CEILING_DIRECTORIES="`pwd`/.." git describe --always 2>/dev/null || true

.if exists(.localflags)
.  include ".localflags"
.endif

.if exists(.cflags)
.  include ".cflags"
.endif

.include "Makefile.pre"
.if $(CXX) == clang++
CPPFLAGS+=$(CLANG_FLAGS)
.endif

LIBPKGDEPDB_LA:=
LTCXX = $(CXX)
LTLD  = $(CXX)
LTOBJECTS  = $(OBJECTS)
LTMAIN_OBJ = $(MAIN_OBJ)
LTLIB_OBJ  = $(LIB_OBJ)
.if $(WITH_LIBRARY) == yes
HAVE_LIBTOOL != which $(LIBTOOL) >/dev/null && echo yes
.if $(HAVE_LIBTOOL) == yes
LIBPKGDEPDB_LA:=libpkgdepdb.la
LTCXX = $(LIBTOOL) --mode=compile $(CXX)
LTLD  = $(LIBTOOL) --mode=link $(CXX)
LTOBJECTS  = $(OBJECTS:.o=.lo)
LTMAIN_OBJ = $(MAIN_OBJ:.o=.lo)
LTLIB_OBJ  = $(LIB_OBJ:.o=.lo)
INSTALLTARGETS+=$(INSTALLTARGETS_LIB)
.endif
.endif

.if $(ALPM) == yes
ENABLE_ALPM := define
LIBS += -lalpm
.endif

.if $(REGEX) == yes
ENABLE_REGEX := define
.endif

.if $(THREADS) == yes
ENABLE_THREADS := define
.endif

.include "Makefile"

.if !defined(ALLFLAGS) || !defined(OLDCXX) \
    || "$(ALLFLAGS)" != "$(COMPAREFLAGS)" \
    || "$(OLDCXX)" != "$(CXX)"
.PHONY: .cflags
# the first echo needs to use single quotes otherwise the .if
# comparisons fail
.cflags:
	@echo 'ALLFLAGS := $(COMPAREFLAGS)' > .cflags
	@echo "OLDCXX := $(CXX)" >> .cflags
.endif
