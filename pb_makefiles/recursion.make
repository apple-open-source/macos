##
# Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# The contents of this file constitute Original Code as defined in and
# are subject to the Apple Public Source License Version 1.1 (the
# "License").  You may not use this file except in compliance with the
# License.  Please obtain a copy of the License at
# http://www.apple.com/publicsource and read it before using this file.
# 
# This Original Code and all software distributed under the License are
# distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
#
# recursion.make
#
# Rules for recursively invoking make to build a target in a subproject.
# The format for a recursive target is target@subproject, where "target" is
# one of the standard targets and "subproject" is a subproject directory of the
# current project.  If you want to specify subrojects of subprojects, you
# must reverse-stack them.  For example, the command-line invocation "make 
# prebuild@Grandchild@Child.bproj" will run the prebuild target in the 
# Grandchild subproject of the Child bundle project of the current project.
#
# STANDARD TARGETS
#    clean, mostlyclean, all: see common.make
#    prebuild: see prebuild.make
#    build: see build.make
#    install: see install.make
#    installhdrs: see installhdrs.make
#
# IMPORTED VARIABLES
#    OTHER_RECURSIVE_VARIABLES: The names of variables which you want to be
#  	passed on the command line to recursive invocations of make.  Note that
#	the values in OTHER_*FLAGS are inherited by subprojects automatically --
#	you do not have to (and shouldn't) add OTHER_*FLAGS to 
#	OTHER_RECURSIVE_VARIABLES. 
#    BASE_TARGET: a non-standard target that you wish to apply recursively.  For 
#       example, to build bar.o in the bar subproject, you would execute the 
#       command "make foo.o@bar BASE_TARGET=foo.o"
#

RECURSABLE_DIRS = $(ALL_SUBPROJECTS) 
RECURSABLE_RULES += clean mostlyclean all prebuild build install installhdrs postinstall installsrc
OPTIMIZABLE_RULES += prebuild build

#
# Decide whether to build this particular project
#

ifneq "" "$(INCLUDED_OSS)"
ifeq "" "$(findstring $(OS), $(INCLUDED_OSS))"
SUPPRESS_BUILD = YES
endif
endif

ifneq "" "$(EXCLUDED_OSS)"
ifneq "" "$(findstring $(OS), $(EXCLUDED_OSS))"
SUPPRESS_BUILD = YES
endif
endif

ADJUSTED_TARGET_ARCHS = $(strip $(TARGET_ARCHS))

ifneq "" "$(INCLUDED_ARCHS)"
ADJUSTED_TARGET_ARCHS = $(strip $(filter $(INCLUDED_ARCHS), $(TARGET_ARCHS)))
endif

ifneq "" "$(EXCLUDED_ARCHS)"
ADJUSTED_TARGET_ARCHS = $(strip $(filter-out $(EXCLUDED_ARCHS), $(TARGET_ARCHS)) )
endif

ifeq "$(ADJUSTED_TARGET_ARCHS)" ""
SUPPRESS_BUILD = YES
endif

#
#
#

ifeq "" "$(GLOBAL_RESOURCE_DIR)"
RECURSIVE_PRODUCT_DIR = $(PRODUCT_DIR)
RECURSIVE_INSTALLDIR = $(INSTALLDIR)
else
RECURSIVE_PRODUCT_DIR = $(GLOBAL_RESOURCE_DIR)
RECURSIVE_INSTALLDIR = $(subst $(PRODUCT_DIR),$(INSTALLDIR),$(GLOBAL_RESOURCE_DIR))
endif

export RECURSIVE_VARIABLES += $(OTHER_RECURSIVE_VARIABLES)
RECURSIVE_VARIABLE_ASSIGNMENTS = $(foreach X,$(RECURSIVE_VARIABLES), "$(X)=$($(X))")

RECURSIVE_FLAGS = "MAKEFILEDIR=$(MAKEFILEDIR)"
ifeq "AGGREGATE" "$(PROJTYPE)"
RECURSIVE_FLAGS += "RECURSING="
else
RECURSIVE_FLAGS += "RECURSING=YES"
endif
RECURSIVE_FLAGS += "RECURSING_ON_TARGET=$(RECURSIVE_TARGET)"
RECURSIVE_FLAGS	+= "RECURSIVE_ELLIPSIS=$(RECURSIVE_ELLIPSIS)..."
RECURSIVE_FLAGS += "SRCROOT=`$(DOTDOTIFY) $(SRCROOT)`"
RECURSIVE_FLAGS += "SYMROOT=`$(DOTDOTIFY) $(SYMROOT)`"
RECURSIVE_FLAGS += "OBJROOT=`$(DOTDOTIFY) $(OBJROOT)`"
RECURSIVE_FLAGS += "DSTROOT=`$(DOTDOTIFY) $(DSTROOT)`"
RECURSIVE_FLAGS += "PRODUCT_DIR=`$(DOTDOTIFY) $(RECURSIVE_PRODUCT_DIR)`"
ifneq "AGGREGATE" "$(PROJTYPE)"
RECURSIVE_FLAGS += "INSTALLDIR=`$(DOTDOTIFY) $(RECURSIVE_INSTALLDIR)`"
endif
RECURSIVE_FLAGS += "OFILE_DIR=$(OFILE_DIR)/$(RECURSIVE_DIRECTORY)"
RECURSIVE_FLAGS += "SFILE_DIR=$(SFILE_DIR)/$(RECURSIVE_DIRECTORY)"
RECURSIVE_FLAGS += "SRCPATH=$(SRCPATH)/$(RECURSIVE_DIRECTORY)"
RECURSIVE_FLAGS += "LINK_SUBPROJECTS=$(LINK_SUBPROJECTS)"
RECURSIVE_FLAGS += $(RECURSIVE_VARIABLE_ASSIGNMENTS)

ifeq "AGGREGATE" "$(PROJTYPE)"
RECURSIVE_FLAGS += "JAVA_SRC_DIR=$(JAVA_SRC_DIR)/$(RECURSIVE_DIRECTORY)"
RECURSIVE_FLAGS += "JAVA_SRC_DIR_CLIENT=$(JAVA_SRC_DIR_CLIENT)/$(RECURSIVE_DIRECTORY)"
RECURSIVE_FLAGS += "JAVA_OBJ_DIR=$(JAVA_OBJ_DIR)/$(RECURSIVE_DIRECTORY)"
RECURSIVE_FLAGS += "JAVA_OBJ_DIR_CLIENT=$(JAVA_OBJ_DIR_CLIENT)/$(RECURSIVE_DIRECTORY)"
endif

# recursive target of "clean@grandchild@child" is "clean@grandchild"
RECURSIVE_TARGET = $(subst @$(RECURSIVE_DIRECTORY),,$@)
RECURSIVE_DIRECTORY = $(notdir $(subst @,/,$@))
RECURSIVE_TAGFILE = $(OFILE_DIR)/$(RECURSIVE_DIRECTORY)/lastbuildtimes/$(RECURSIVE_TARGET)

RECURSIVE_FLAGS += "SUBDIRECTORY_NAME=$(RECURSIVE_DIRECTORY)"

remove-timestamps:
	$(FIND) $(SFILE_DIR) -name '*.lastbuildtime.*' -exec rm '{}' ';'

# The foreach statement provides exact matches for all recursable directories.
# The wildcard rule matches anything else.  We cannot just use the wildcard rule
# because when one target matching the wildcard is built, all other targets
# matching the wildcard are declared up-to-date (a gnumake 'feature').

# note that the $(OPTIMIZABLE_RULES) will not be built recursively if
# nothing has changed since their last build.  This feature is disabled
# if we are using the dependencies makefile, since a subproject may
# need to be rebuilt due to a change in a header file that it includes

$(foreach RULE, $(RECURSABLE_RULES), $(addprefix $(RULE)@, $(RECURSABLE_DIRS))):
ifeq "FULL" "$(RECURSION)"
	$(SILENT) $(CD) $(RECURSIVE_DIRECTORY) && $(MAKE) $(RECURSIVE_TARGET) $(RECURSIVE_FLAGS)
else
	$(SILENT) (\
        if [ -z "$(findstring $(RECURSIVE_TARGET), $(OPTIMIZABLE_RULES))" -o -r $(SFILE_DIR)/Makefile.dependencies ] ; \
        then \
	  $(CD) $(RECURSIVE_DIRECTORY) && $(MAKE) $(RECURSIVE_TARGET) $(RECURSIVE_FLAGS) ; \
	elif $(NEWER) -s $(RECURSIVE_TAGFILE) $(RECURSIVE_DIRECTORY) ; \
 	then \
          $(ECHO) $(RECURSIVE_ELLIPSIS)...skipping $(RECURSIVE_DIRECTORY) ; \
        else \
	  $(CD) $(RECURSIVE_DIRECTORY) && $(MAKE) $(RECURSIVE_TARGET) $(RECURSIVE_FLAGS) ; \
	  $(MKDIRS) $(dir $(RECURSIVE_TAGFILE)) ; \
          $(TOUCH) $(RECURSIVE_TAGFILE) ; \
        fi )
endif

$(addprefix %@, $(RECURSABLE_DIRS)):
	$(SILENT) $(CD) $(RECURSIVE_DIRECTORY) && $(MAKE) $(RECURSIVE_TARGET) $(RECURSIVE_FLAGS)
