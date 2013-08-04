OBJECTS_SRC = $(subst .o,.cpp,$(OBJECTS))

# For the local development environment
# define your most precious flags in .localflags
-include .localflags

-include .cflags
-include Makefile

ifneq ($(shell GIT_CEILING_DIRECTORIES=`pwd`/.. git describe --always 2>/dev/null),)
    CPPFLAGS += -DGITINFO="\"$(shell git describe --always)\""
endif

ifeq ($(ALPM),yes)
	CPPFLAGS += -DWITH_ALPM
	LIBS += -lalpm
endif

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
