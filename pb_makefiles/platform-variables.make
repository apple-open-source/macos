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
# platform-variables.make
#
# Variable definitions that need to get included by platform.make at run-time
# during project builds.  Note that during the build of the pb_makefiles
# project itself, a new streamlined version of platform.make gets generated 
# under the control of the install-resources rule in Makefile.postamble.
#


#
# Paths to system directories for new directory layout.  These variables
# are defined here so that they can be used before common.make is included
# (e.g., when including $(LOCAL_MAKEFILEDIR)/<projecttype>.make.preamble.
#


-include $(MAKEFILEDIR)/platform-variables-extra.make


ifeq "MACOS" "$(OS)"

SYSTEM_APPS_DIR = /Applications
SYSTEM_ADMIN_APPS_DIR = /Applications/Utilities
SYSTEM_DEMOS_DIR = /Applications/Extras
SYSTEM_DEVELOPER_DIR = /Developer
SYSTEM_DEVELOPER_APPS_DIR = /Developer/Applications
SYSTEM_DEVELOPER_JAVA_TOOLS_DIR = /Developer/Applications/Java Tools
SYSTEM_DEVELOPER_PERFORMANCE_TOOLS_DIR = /Developer/Applications/Performance Tools
SYSTEM_DEVELOPER_GRAPHICS_TOOLS_DIR = /Developer/Applications/Graphics Tools
SYSTEM_DEVELOPER_UTILITIES_DIR = /Developer/Applications/Utilities
SYSTEM_DEVELOPER_DEMOS_DIR = /Developer/Applications/Utilities/Built Examples
SYSTEM_DEVELOPER_DOC_DIR = /Developer/ADC Reference Library
SYSTEM_DEVELOPER_TOOLS_DOC_DIR = /Developer/ADC Reference Library/documentation/DeveloperTools
SYSTEM_DEVELOPER_RELEASENOTES_DIR = /Developer/ADC Reference Library/releasenotes
SYSTEM_DEVELOPER_TOOLS_RELEASENOTES_DIR = /Developer/ADC Reference Library/releasenotes/DeveloperTools
SYSTEM_LIBRARY_DIR = /System/Library
SYSTEM_CORE_SERVICES_DIR = /System/Library/CoreServices
SYSTEM_DOCUMENTATION_DIR = /Library/Documentation
SYSTEM_LIBRARY_EXECUTABLES_DIR =
SYSTEM_DEVELOPER_EXECUTABLES_DIR =
LOCAL_ADMIN_APPS_DIR = /Applications/Utilities
LOCAL_APPS_DIR = /Applications
LOCAL_DEVELOPER_DIR = /Library/Developer
LOCAL_DEVELOPER_EXECUTABLES_DIR =
LOCAL_LIBRARY_DIR = /Library
USER_APPS_DIR = $(HOME)/Applications
USER_LIBRARY_DIR = $(HOME)/Library
MAN_PAGE_DIRECTORIES = /usr/share/man

else
ifeq "NEXTSTEP" "$(OS)"

SYSTEM_APPS_DIR = /System/Applications
SYSTEM_ADMIN_APPS_DIR = /System/Administration
SYSTEM_DEMOS_DIR = /System/Demos
SYSTEM_DEVELOPER_DIR = /System/Developer
SYSTEM_DEVELOPER_APPS_DIR = /System/Developer/Applications
SYSTEM_DEVELOPER_DOC_DIR = /System/Documentation/Developer
SYSTEM_DEVELOPER_TOOLS_DOC_DIR = /System/Developer/Documentation/DeveloperTools
SYSTEM_DEVELOPER_RELEASENOTES_DIR = /System/Developer/Documentation/ReleaseNotes
SYSTEM_DEVELOPER_TOOLS_RELEASENOTES_DIR = /System/Developer/Documentation/ReleaseNotes/DeveloperTools
SYSTEM_LIBRARY_DIR = /System/Library
SYSTEM_CORE_SERVICES_DIR = /System/Library/CoreServices
SYSTEM_DOCUMENTATION_DIR = /System/Documentation
SYSTEM_LIBRARY_EXECUTABLES_DIR =
SYSTEM_DEVELOPER_EXECUTABLES_DIR =
LOCAL_ADMIN_APPS_DIR = /Local/Administration
LOCAL_APPS_DIR = /Local/Applications
LOCAL_DEVELOPER_DIR = /Local/Developer
LOCAL_DEVELOPER_EXECUTABLES_DIR =
LOCAL_LIBRARY_DIR = /Local/Library
USER_APPS_DIR = $(HOME)/Applications
USER_LIBRARY_DIR = $(HOME)/Library

else

# PDO platforms use same layout as Windows.
SYSTEM_APPS_DIR = /Demos
SYSTEM_ADMIN_APPS_DIR = /Demos
SYSTEM_DEMOS_DIR = /Demos
SYSTEM_DEVELOPER_DIR = /Developer
SYSTEM_DEVELOPER_APPS_DIR = /Developer/Applications
SYSTEM_DEVELOPER_DOC_DIR = /Documentation/Developer
SYSTEM_DEVELOPER_TOOLS_DOC_DIR = /Developer/Documentation/DeveloperTools
SYSTEM_DEVELOPER_RELEASENOTES_DIR = /Developer/Documentation/ReleaseNotes
SYSTEM_DEVELOPER_TOOLS_RELEASENOTES_DIR = /Developer/Documentation/ReleaseNotes/DeveloperTools
SYSTEM_LIBRARY_DIR = /Library
SYSTEM_CORE_SERVICES_DIR = /Library/CoreServices
SYSTEM_LIBRARY_EXECUTABLES_DIR = /Library/Executables
SYSTEM_DEVELOPER_EXECUTABLES_DIR = /Developer/Executables
SYSTEM_DOCUMENTATION_DIR = /Documentation
LOCAL_ADMIN_APPS_DIR = /Local/Administration
LOCAL_APPS_DIR = /Local/Applications
LOCAL_DEVELOPER_DIR = /Local/Developer
LOCAL_DEVELOPER_EXECUTABLES_DIR = /Local/Developer/Executables
LOCAL_LIBRARY_DIR = /Local/Library
USER_APPS_DIR = $(HOME)/Applications
USER_LIBRARY_DIR = $(HOME)/Library

endif
endif

