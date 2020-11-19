##############################################################################
# Top-level targets executed by XBS for the internal build of libc++abi
##############################################################################

# Declare 'install' target first to make it default.
install:

# Eventually we'll also want llvm/cmake and pieces, but for now keep this
# standalone.
installsrc-paths := libcxxabi
include apple-xbs-support/helpers/installsrc.mk

.PHONY: installsrc
installsrc: installsrc-helper

.PHONY: install
install:
	@echo "Installing libc++abi.dylib and the various libc++abi-static.a"
	"${SRCROOT}/libcxxabi/apple-install-libcxxabi.sh"
	"${SRCROOT}/libcxxabi/apple-install-libcxxabi.sh" dyld

# TODO: For some reason, bridgeOS calls that target instead of 'install'.
.PHONY: libc++abi
libc++abi: install

.PHONY: libcxxabi_driverkit
libcxxabi_driverkit:
	@echo "Installing DriverKit libc++abi.dylib"
	"${SRCROOT}/libcxxabi/apple-install-libcxxabi.sh" DriverKit

.PHONY: installapi
installapi:
	@echo "We don't currently perform an installapi step here"
	# TODO: We need to create _something_ in the installapi step, or the
	# DriverKit build fails because of verifiers.
	mkdir -p "${DSTROOT}/usr/lib"

.PHONY: installhdrs
installhdrs:
	@echo "TODO: we need to install headers here"

.PHONY: clean
clean:
	@echo "Nothing to clean"
