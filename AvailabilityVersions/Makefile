SRCROOT ?= $(shell pwd)
OBJROOT ?= $(SRCROOT)/obj
SYMROOT ?= $(SRCROOT)/sym
DSTROOT ?= $(SRCROOT)/dst
DRIVERKIT ?= 0
CONFIG_EXCLAVEKIT ?= 0
CONFIG_EXCLAVECORE ?= 0

CMAKE   :=  $(shell xcrun --find cmake)
NINJA   :=  $(shell xcrun --find ninja)

cmake: CMakeLists.txt
	$(CMAKE) $(realpath $DSTROOT) -GNinja 	-DCMAKE_MAKE_PROGRAM=$(NINJA) -DSRCROOT=$(SRCROOT) -DOBJROOT=$(OBJROOT)		\
											-DSYMROOT=$(SYMROOT) -DDSTROOT=$(DSTROOT) -DDRIVERKIT=$(DRIVERKIT)			\
											-DDRIVERKITROOT=$(DRIVERKITROOT) -DCONFIG_EXCLAVEKIT=$(CONFIG_EXCLAVEKIT)	\
											-DCONFIG_EXCLAVECORE=$(CONFIG_EXCLAVECORE) -S $(SRCROOT) -B $(OBJROOT)

install: cmake
	$(NINJA) -C $(OBJROOT) install

installhdrs: install

installsrc: $(SRCROOT)
	rsync -Crv --exclude '.svn' --exclude '.git' ./ $(SRCROOT)/

clean:
	rm -rf $(OBJROOT) $(SYMROOT) $(DSTROOT)
