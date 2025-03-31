SRCROOT ?= $(shell pwd)
OBJROOT ?= $(SRCROOT)/obj
SYMROOT ?= $(SRCROOT)/sym
DSTROOT ?= $(SRCROOT)/dst
DRIVERKIT ?= 0
CONFIG_EXCLAVEKIT ?= 0
CONFIG_EXCLAVECORE ?= 0
CONFIG_KERNELKIT ?= 0
RC_ProjectNameAndBuildVersion ?= "Local"


ifeq (,$(SYSTEM_PREFIX))
	ifeq "$(DRIVERKIT)" "1"
		SYSTEM_PREFIX						= ${DRIVERKITROOT}
		INSTALL_KERNEL_HEADERS 	= "0"
	else ifeq "$(CONFIG_EXCLAVECORE)" "1"
		SYSTEM_PREFIX						= /System/ExclaveCore
		INSTALL_KERNEL_HEADERS 	= "0"
	else ifeq "$(CONFIG_EXCLAVEKIT)" "1"
		SYSTEM_PREFIX						= /System/ExclaveKit
		INSTALL_KERNEL_HEADERS 	= "0"
	else ifeq "$(CONFIG_KERNELKIT)" "1"
		SYSTEM_PREFIX						= /System/KernelKit
		INSTALL_KERNEL_HEADERS 	= "0"
	else
		INSTALL_KERNEL_HEADERS 	= "1"
	endif
endif



CMAKE   :=  $(shell xcrun --find cmake)
NINJA   :=  $(shell xcrun --find ninja)

cmake: CMakeLists.txt
	$(CMAKE) $(realpath $DSTROOT) -GNinja 	-DCMAKE_MAKE_PROGRAM=$(NINJA) -DSRCROOT=$(SRCROOT) -DOBJROOT=$(OBJROOT)		\
											-DSYMROOT=$(SYMROOT) -DDSTROOT=$(DSTROOT) -DDRIVERKIT=$(DRIVERKIT)			\
											-DSYSTEM_PREFIX=$(SYSTEM_PREFIX) -DCONFIG_EXCLAVEKIT=$(CONFIG_EXCLAVEKIT)	\
											-DCONFIG_EXCLAVECORE=$(CONFIG_EXCLAVECORE) -DCONFIG_KERNELKIT=$(CONFIG_KERNELKIT) \
											-DINSTALL_KERNEL_HEADERS=$(INSTALL_KERNEL_HEADERS) \
											-DAV_VERSION=$(RC_ProjectNameAndBuildVersion) \
											-S $(SRCROOT) -B $(OBJROOT)

install: cmake
	$(NINJA) -C $(OBJROOT) install

installhdrs: install

installsrc: $(SRCROOT)
	rsync -Crv --exclude '.svn' --exclude '.git' ./ $(SRCROOT)/

clean:
	rm -rf $(OBJROOT) $(SYMROOT) $(DSTROOT)
