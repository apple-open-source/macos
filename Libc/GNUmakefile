# Remove any NEXT_ROOT argument
override MAKEOVERRIDES := $(filter-out NEXT_ROOT=%,$(MAKEOVERRIDES))
override MAKEFILEPATH := $(subst $(NEXT_ROOT),,$(MAKEFILEPATH))
unexport NEXT_ROOT

include $(MAKEFILEPATH)/CoreOS/Standard/Commands.make

all:
	@$(BSDMAKE)

.DEFAULT:
	@$(BSDMAKE) $@
