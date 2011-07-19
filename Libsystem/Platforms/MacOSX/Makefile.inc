#
# Selectable features for MacOSX
#

# Build _debug.dylib
FEATURE_DEBUG_DYLIB = 1

# Build _profile.dylib
FEATURE_PROFILE_DYLIB = 1

# Use different order file if it exists
ALTORDERFILE = /AppleInternal/OrderFiles/libSystem.order
ifeq ($(shell [ ! -f $(ALTORDERFILE) ] || echo YES),YES)
PLATFORM_ORDER_FILE = $(ALTORDERFILE)
endif
