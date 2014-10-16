#
# Copyright (c) 2001,2004,2011,2014 Apple Inc. All Rights Reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
# 
# @APPLE_LICENSE_HEADER_END@
#
# generator.mk -- Makefile for generated files.

$(TARGET)/cssmexports.gen	$(TARGET)/funcnames.gen	$(TARGET)/generator.rpt	$(TARGET)/transition.gen: \
	$(SOURCEDIR)/cssmapi.h \
	$(SOURCEDIR)/cssmaci.h \
	$(SOURCEDIR)/cssmcspi.h \
	$(SOURCEDIR)/cssmdli.h \
	$(SOURCEDIR)/cssmcli.h \
	$(SOURCEDIR)/cssmtpi.h
	mkdir -p $(TARGET)
	/usr/bin/perl lib/generator.pl $(SOURCEDIR) $(CONFIG) $(TARGET)
