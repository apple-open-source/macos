##
# Makefile support for subproject action target recursion
##
# Daniel A. Steffen <das@users.sourceforge.net>
##

##
# Set these variables as needed, then include this file
#
#  SubProjects
#  Actions
#  Actions_nodeps
#
##

define subproj_action_targets
$(1)_subprojs        := $$(SubProjects:%=$(1)-%)
targets              += $$($(1)_subprojs)
$$($(1)_subprojs):      TARGET := $(1)
$$($(1)_subprojs):      $(1)-%:
$(1)::                  $(if $(2),,$$($(1)_subprojs))
endef

$(foreach action,$(Actions),$(eval $(call subproj_action_targets,$(action),)))
ifdef Actions_nodeps
$(foreach action,$(Actions_nodeps),$(eval $(call subproj_action_targets,$(action),nodeps)))
endif

$(targets):
	$(_v) $(MAKE) -C $(SRCROOT)/$* $(TARGET) $(MAKE_ARGS) \
	        SRCROOT=$(SRCROOT)/$* OBJROOT=$(OBJROOT)/$* SYMROOT=$(SYMROOT)/$* DSTROOT=$(DSTROOT)

.PHONY: $(Actions) $(Actions_nodeps) $(targets)
.NOTPARALLEL:
