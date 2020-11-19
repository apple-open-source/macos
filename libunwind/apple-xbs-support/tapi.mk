#
# Apple B&I Makefile
#
INSTALL := $(shell xcrun -f install)
BUILD_TAPI := $(SRCROOT)/tapi/utils/buildit/build_tapi

#
# TAPI supports installapi per default. Allow B&I to change the default and
# print a status message to the log file.
#
SUPPORTS_TEXT_BASED_API ?= YES
$(info SUPPORTS_TEXT_BASED_API=$(SUPPORTS_TEXT_BASED_API))

#
# Common settings
#
TAPI_VERSION := 11.0.0
TAPI_INSTALL_PREFIX := $(DSTROOT)/$(DT_VARIANT)/$(TOOLCHAIN_INSTALL_DIR)
TAPI_LIBRARY_PATH := $(TAPI_INSTALL_PREFIX)/usr/lib
TAPI_HEADER_PATH := $(TAPI_INSTALL_PREFIX)/usr/local/include

TAPI_COMMON_OPTS := -dynamiclib \
		    -xc++ \
		    -std=c++11 \
		    $(RC_ARCHS:%=-arch %) \
		    -install_name @rpath/libtapi.dylib \
		    -current_version $(RC_ProjectSourceVersion) \
		    -compatibility_version 1 \
		    -allowable_client ld \
		    -I$(TAPI_HEADER_PATH) \
		    -o $(OBJROOT)/libtapi.tbd

TAPI_VERIFY_OPTS := $(TAPI_COMMON_OPTS) \
		    --verify-mode=Pedantic \
        	--verify-against=$(TAPI_LIBRARY_PATH)/libtapi.dylib


.PHONY: installsrc installhdrs installapi install tapi build installapi-verify clean

#
# Only run the verify target if installapi is enabled.
#
ifeq ($(SUPPORTS_TEXT_BASED_API),YES)
install: installapi-verify
endif

# Install source uses the shared helper, but also fetches PGO data during
# submission to a B&I train.
installsrc-paths := \
	llvm			\
	clang			\
	tapi

include apple-xbs-support/helpers/installsrc.mk

installsrc: installsrc-helper

installhdrs: $(DSTROOT)
	@echo
	@echo ++++++++++++++++++++++
	@echo + Installing headers +
	@echo ++++++++++++++++++++++
	@echo
	ditto $(SRCROOT)/tapi/include/tapi/*.h $(TAPI_HEADER_PATH)/tapi/
	# Generate Version.inc
	echo "$(TAPI_VERSION)" | awk -F. '{                        \
	  printf "#define TAPI_VERSION %d.%d.%d\n", $$1, $$2, $$3; \
	  printf "#define TAPI_VERSION_MAJOR %dU\n", $$1;          \
	  printf "#define TAPI_VERSION_MINOR %dU\n", $$2;          \
	  printf "#define TAPI_VERSION_PATCH %dU\n", $$3;          \
	}' > $(TAPI_HEADER_PATH)/tapi/Version.inc

installapi: installhdrs $(OBJROOT) $(DSTROOT)
	@echo
	@echo ++++++++++++++++++++++
	@echo + Running InstallAPI +
	@echo ++++++++++++++++++++++
	@echo

	@if [ "$(SUPPORTS_TEXT_BASED_API)" != "YES" ]; then \
	  echo "installapi for target 'tapi' was requested, but SUPPORTS_TEXT_BASED_API has been disabled."; \
	  exit 1; \
	fi

	xcrun --sdk $(SDKROOT) tapi installapi \
	  $(TAPI_COMMON_OPTS) \
	  $(TAPI_INSTALL_PREFIX)

	$(INSTALL) -d -m 0755 $(TAPI_LIBRARY_PATH)
	$(INSTALL) -c -m 0755 $(OBJROOT)/libtapi.tbd $(TAPI_LIBRARY_PATH)/libtapi.tbd

tapi: install
install: build

build: $(OBJROOT) $(SYMROOT) $(DSTROOT)
	@echo
	@echo +++++++++++++++++++++
	@echo + Build and Install +
	@echo +++++++++++++++++++++
	@echo
	cd $(OBJROOT) && $(BUILD_TAPI) $(TAPI_VERSION)

installapi-verify: build
	@echo
	@echo +++++++++++++++++++++++++++++++++
	@echo + Running InstallAPI and Verify +
	@echo +++++++++++++++++++++++++++++++++
	@echo

	@if [ "$(SUPPORTS_TEXT_BASED_API)" != "YES" ]; then \
	  echo "installapi for target 'tapi' was requested, but SUPPORTS_TEXT_BASED_API has been disabled."; \
	  exit 1; \
	fi

	xcrun --sdk $(SDKROOT) tapi installapi \
	  $(TAPI_VERIFY_OPTS) \
	  $(TAPI_INSTALL_PREFIX)

	$(INSTALL) -d -m 0755 $(TAPI_LIBRARY_PATH)
	$(INSTALL) -c -m 0755 $(OBJROOT)/libtapi.tbd $(TAPI_LIBRARY_PATH)/libtapi.tbd

clean:
	@echo
	@echo ++++++++++++
	@echo + Cleaning +
	@echo ++++++++++++
	@echo
	@if [ -d "$(OBJROOT)" -a "$(OBJROOT)" != "/" ]; then \
	  echo '*** DELETING ' $(OBJROOT); \
	  rm -rf "$(OBJROOT)"; \
	fi
	@if [ -d "$(SYMROOT)" -a "$(SYMROOT)" != "/" ]; then \
	  echo '*** DELETING ' $(SYMROOT); \
	  rm -rf "$(SYMROOT)"; \
	fi
	@if [ -d "$(DSTROOT)" -a "$(DSTROOT)" != "/" ]; then \
	  echo '*** DELETING ' $(DSTROOT); \
	  rm -rf "$(DSTROOT)"; \
	fi

$(OBJROOT) $(SYMROOT) $(DSTROOT):
	mkdir -p "$@"
