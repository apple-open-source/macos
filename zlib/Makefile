# Common options
CONFIG ?= Release
RELEASE ?= Sunburst
SANITIZE ?= 0

# OSX build options
OSX_SDK ?= macosx.internal
OSX_ARCHS ?= x86_64 arm64e
OSX_DSTROOT := /tmp/zlib-osx.dst
OSX_LOG     := /tmp/compression-osx.log

# IOS build options
IOS_SDK ?= iphoneos.internal
IOS_ARCHS ?= arm64 arm64e

# WatchOS build options
WOS_SDK ?= watchos.internal
WOS_ARCHS ?= armv7k arm64_32

# Buildit record name
BUILDIT_RECORD_NAME ?= zlib

ifeq ($(SANITIZE),0)
  SANITIZER_OPT :=
else
  SANITIZER_OPT := -enableAddressSanitizer YES
endif

LOG_EXT := > ./build/log.txt 2>&1 || ( cat ./build/log.txt && exit 1 )
ARCH_DATE := $(shell date +"%Y%m%d")
ARCH_FILE := $(HOME)/BNNS-$(ARCH_DATE).tgz

# Color option printing
define PRINT_OPT
   @echo "\033[1;34m$(1)\033[0m = $(2)"
endef

all: osx

tests: clean osx
	xcodebuild -sdk $(OSX_SDK) -configuration $(CONFIG) -target t_zlib_verify ARCHS="$(OSX_ARCHS)" $(SANITIZER_OPT) $(LOG_EXT)
	./build/Release/t_zlib_verify -t -e 0 -d 10 /Volumes/Data/Sets/CanterburyCorpus/*
	./build/Release/t_zlib_verify -t -e 0 -d 10 -f /Volumes/Data/Sets/CanterburyCorpus/*

fuzzer:
	xcodebuild -sdk $(OSX_SDK) -configuration $(CONFIG) -target f_zlib ARCHS="$(OSX_ARCHS)" DSTROOT=$(OSX_DSTROOT) $(SANITIZER_OPT) install > $(OSX_LOG) 2>&1 || cat $(OSX_LOG)
	$(OSX_DSTROOT)/usr/local/bin/f_zlib /Volumes/Data/Work/CompressionTests.git/Fuzzing/zlib_zlib

osx:
	$(call PRINT_OPT,CONFIG   ,$(CONFIG))
	$(call PRINT_OPT,OSX_SDK  ,$(OSX_SDK))
	$(call PRINT_OPT,OSX_ARCHS,$(OSX_ARCHS))
	$(call PRINT_OPT,SANITIZE ,$(SANITIZE))
	@/bin/rm -rf $(OSX_DSTROOT)
	xcodebuild -sdk $(OSX_SDK) -configuration $(CONFIG) -target all ARCHS="$(OSX_ARCHS)" DSTROOT=$(OSX_DSTROOT) $(SANITIZER_OPT) install > $(OSX_LOG) 2>&1 || cat $(OSX_LOG)

osx-install: osx
	./scripts/t_install_roots.pl -d $(OSX_DSTROOT) -s $(OSX_SDK)
	./scripts/t_install_roots.pl -d $(OSX_DSTROOT) -s $(OSX_SDK) -r

# Run: make osx-test SANITIZE=1
osx-test: osx
	sudo ./scripts/t_zlib_torture.sh $(OSX_DSTROOT) /Volumes/Data/Work/CompressionTests.git/SilesiaCorpus
	sudo ./scripts/t_zlib_torture.sh $(OSX_DSTROOT) /Volumes/Data/Work/CompressionTests.git/CanterburyCorpus
	sudo ./scripts/t_zlib_torture.sh $(OSX_DSTROOT) /Volumes/Data/Work/CompressionTests.git/CalgaryCorpus
	sudo ./scripts/t_zlib_torture.sh $(OSX_DSTROOT) /bin

ios:
	@[ -d ./build ] || mkdir -p ./build
	xcodebuild -sdk $(IOS_SDK) -configuration $(CONFIG) -target all ARCHS="$(IOS_ARCHS)" $(SANITIZER_OPT) $(LOG_EXT)
wos:
	@[ -d ./build ] || mkdir -p ./build
	xcodebuild -sdk $(WOS_SDK) -configuration $(CONFIG) -target all ARCHS="$(WOS_ARCHS)" $(SANITIZER_OPT) $(LOG_EXT)

buildit-archive: clean
	xbs buildit . -noverify -project zlib -update Prevailing$(RELEASE) -buildAllAliases -buildRecordName zlib -archive
	@cd /tmp/$(BUILDIT_RECORD_NAME).roots/BuildRecords && find . -type f -ls | egrep '\/(SDKContent)?Root\/'

clean:
	/bin/rm -rf ./build /tmp/zlib.dst $(OSX_DSTROOT)
