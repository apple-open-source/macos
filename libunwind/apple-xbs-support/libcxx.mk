##############################################################################
# Top-level targets executed by XBS for the internal build of libc++
##############################################################################

# Declare 'install' target first to make it default.
install:

# Eventually we'll also want llvm/cmake and pieces, but for now keep this
# standalone.
installsrc-paths := libcxx
include apple-xbs-support/helpers/installsrc.mk

.PHONY: installsrc
installsrc: installsrc-helper

.PHONY: install
install:
	@echo "Installing libc++.dylib"
	"${SRCROOT}/libcxx/apple-install-libcxx.sh"

.PHONY: libcxx_driverkit
libcxx_driverkit:
	@echo "Installing DriverKit libc++.dylib"
	"${SRCROOT}/libcxx/apple-install-libcxx.sh" DriverKit

.PHONY: installapi
installapi:
	@echo "We don't currently perform an installapi step here"
	# TODO: We need to create _something_ in the installapi step, or the
	# DriverKit build fails because of verifiers.
	mkdir -p "${DSTROOT}/usr/lib"

.PHONY: installhdrs
installhdrs:
	@echo "We don't currently install the libc++ headers here"

.PHONY: clean
clean:
	@echo "Nothing to clean"
