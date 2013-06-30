OBJECTS_SRC = ${OBJECTS:C/\.o/.cpp/g}

GITINFO != git describe --always 2>/dev/null
.if $(GITINFO) != ""
    CPPFLAGS += -DGITINFO="\"$(GITINFO)\""
.endif

.if exists(.localflags)
.  include ".localflags"
.endif

.if exists(.cflags)
.  include ".cflags"
.endif

.include "Makefile"

.if !defined(ALLFLAGS) || !defined(OLDCXX) \
    || $(ALLFLAGS) != $(COMPAREFLAGS) \
    || $(OLDCXX) != $(CXX)
.PHONY: .cflags
# the first echo needs to use single quotes otherwise the .if
# comparisons fail
.cflags:
	@echo 'ALLFLAGS := $(COMPAREFLAGS)' > .cflags
	@echo "OLDCXX := $(CXX)" >> .cflags
.endif
