# Monorepo makefile: redirect to project-specific .mk for B&I logic.

ifeq "$(RC_ProjectName)" ""
define NEWLINE


endef
projects := $(sort $(patsubst apple-xbs-support/%.mk,%, \
                     $(wildcard apple-xbs-support/*.mk)))
$(error "RC_ProjectName not set, try one of:"$(NEWLINE)$(NEWLINE) \
  $(foreach p,$(projects),$$ make RC_ProjectName=$p$(NEWLINE))    \
  $(NEWLINE))
endif

ifeq "$(SRCROOT)" ""
$(error "SRCROOT not set")
endif

APPLE_XBS_SUPPORT_MK := apple-xbs-support/$(RC_ProjectName).mk

# If this project name doesn't exist, drop the prefix.  This handles Variant_*
# versions of projects.
ifeq "$(shell stat $(APPLE_XBS_SUPPORT_MK) 2>/dev/null)" ""
APPLE_XBS_SUPPORT_MK_DROP_PREFIX := $(shell printf "%s" "$(APPLE_XBS_SUPPORT_MK)" | sed -e 's,/[^_]*_,/,')
ifneq "$(shell stat $(APPLE_XBS_SUPPORT_MK_DROP_PREFIX) 2>/dev/null)" ""
APPLE_XBS_SUPPORT_MK := $(APPLE_XBS_SUPPORT_MK_DROP_PREFIX)
endif
endif

$(info $(RC_ProjectName) => $(APPLE_XBS_SUPPORT_MK))
include $(APPLE_XBS_SUPPORT_MK)
