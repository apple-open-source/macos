#!/bin/sh -e
#
# Copyright (c) 2024 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
#
# "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
# Reserved.  This file contains Original Code and/or Modifications of
# Original Code as defined in and that are subject to the Apple Public
# Source License Version 1.0 (the 'License').  You may not use this file
# except in compliance with the License.  Please obtain a copy of the
# License at http://www.apple.com/publicsource and read it before using
# this file.
#
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License."
#
# @APPLE_LICENSE_HEADER_END@
#

scriptdir=$(dirname $(realpath "$0"))
mkdir -p bin

printf "#!/bin/sh\necho old" > bin/foo
printf "#!/bin/sh\necho new" > bin/newfoo

chmod 755 bin/foo bin/newfoo

# -a normally matches newfoo, since it's first...
"$scriptdir"/arg_selector_simple_a -a > sandbox.out

if ! grep -q "new" sandbox.out; then
	1>&2 echo "Valid argument didn't route to correct application"
	exit 1
fi

# ...but if we try the same within a very restrictive sandbox, then we're in
# trouble.

#sbutil builtin override "$scriptdir"/com.apple.genwrap-test-sandbox.sb

sandbox-exec -f "$scriptdir"/com.apple.genwrap-test-sandbox.sb \
    -D ALLOWED_BINARY_PATH="$(realpath bin/foo)" \
    -D PWD="$PWD" \
    "$scriptdir"/arg_selector_simple_a -a > sandbox.out

if ! grep -q "old" sandbox.out; then
	1>&2 echo "Valid argument in a sandbox didn't route to fallback application"
	exit 1
fi
