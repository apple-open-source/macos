#
# Copyright (c) 2003-2004,2014 Apple Inc. All Rights Reserved.
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
# Makefile that coalesces pieces in Security framework

EXPORTS_DIR=/usr/local/SecurityPieces/Exports/Security
HEADERS_DIR=/usr/local/SecurityPieces/Headers/Security
PRIVATEHEADERS_DIR=/usr/local/SecurityPieces/PrivateHeaders/Security
RESOURCES_DIR=/usr/local/SecurityPieces/Resources/Security

.PHONY: $(EXPORTS_DIR) $(HEADERS_DIR) $(PRIVATEHEADERS_DIR) $(RESOURCES_DIR) build install installhdrs installsrc clean

build install: $(EXPORTS_DIR) $(HEADERS_DIR) $(PRIVATEHEADERS_DIR) $(RESOURCES_DIR)

installhdrs: $(HEADERS_DIR) $(PRIVATEHEADERS_DIR)

installsrc:

clean:

$(EXPORTS_DIR) $(HEADERS_DIR) $(PRIVATEHEADERS_DIR) $(RESOURCES_DIR):
	-@(dest="`echo $@ | sed 's;/usr/local;'\"$(BUILT_PRODUCTS_DIR)\"';'`"; \
	echo "mkdir -p \"$$dest\""; \
	mkdir -p "$$dest"; \
	find $@ -type f -print | while read file; \
	do \
		if [ ! -f "$${dest}/`basename $${file}`" ]; \
		then \
			echo "Copying $${file} to $${dest}"; \
			cp "$${file}" "$${dest}" || exit 1; \
		fi; \
	done )
