GIT_INFO := $(shell GIT_CEILING_DIRECTORIES=`pwd`/.. git describe --always 2>/dev/null || true)

# For the local development environment
# define your most precious flags in .localflags
-include .localflags

-include .cflags

-include Makefile.pre
ifeq ($(CXX),clang++)
	CPPFLAGS+=$(CLANG_FLAGS)
endif

LIBPKGDEPDB_LA:=
INSTALL_LIB:=
UNINSTALL_LIB:=
LTCXX = $(CXX)
LTLD  = $(CXX)
LTOBJECTS  = $(OBJECTS)
LTMAIN_OBJ = $(MAIN_OBJ)
LTLIB_OBJ  = $(LIB_OBJ)
ifeq ($(WITH_LIBRARY),yes)
HAVE_LIBTOOL := $(shell which $(LIBTOOL) >/dev/null && echo yes)
ifeq ($(HAVE_LIBTOOL),yes)
LIBPKGDEPDB_LA:=libpkgdepdb.la
INSTALL_LIB:=install-lib
UNINSTALL_LIB:=uninstall-lib
LTCXX = $(LIBTOOL) --mode=compile $(CXX)
LTLD  = $(LIBTOOL) --mode=link $(CXX)
LTOBJECTS  = $(OBJECTS:.o=.lo)
LTMAIN_OBJ = $(MAIN_OBJ:.o=.lo)
LTLIB_OBJ  = $(LIB_OBJ:.o=.lo)
INSTALLTARGETS+=$(INSTALLTARGETS_LIB)
endif
endif

ifeq ($(ALPM),yes)
ENABLE_ALPM := define
LIBS += -lalpm
endif

ifeq ($(REGEX),yes)
ENABLE_REGEX := define
endif

ifeq ($(THREADS),yes)
ENABLE_THREADS := define
endif

-include Makefile

#ifneq ($(strip $(ALLFLAGS)),$(strip $(?COMPAREFLAGS)))
ifneq ($(strip $(ALLFLAGS)),$(strip $(shell echo $(COMPAREFLAGS))))
.PHONY: .cflags
.cflags:
	@echo "ALLFLAGS := $(COMPAREFLAGS)" > .cflags
	@echo "OLDCXX := $(CXX)" >> .cflags
else
ifneq ($(OLDCXX),$(CXX))
.PHONY: .cflags
.cflags:
	@echo "ALLFLAGS := $(COMPAREFLAGS)" > .cflags
	@echo "OLDCXX := $(CXX)" >> .cflags
endif
endif
