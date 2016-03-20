#!/bin/bash -e
#
# Copyright (c) 2010-2011 Apple Inc. All rights reserved.
#
# @APPLE_APACHE_LICENSE_HEADER_START@
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# @APPLE_APACHE_LICENSE_HEADER_END@
#

# #ifndef __OPEN_SOURCE__
# <rdar://problem/8834394>: Rewrite headers pending <rdar://problem/8492436>
availability="${SDKROOT}/usr/local/libexec/availability.pl"
verifier="${SDKROOT}/AppleInternal/Library/Perl/5.10/Verification/Verifier/availability_verifier"

if [ -n "${DSTROOT}" -a -x "${availability}" -a -d "${verifier}" ]; then
	mac_a=($("${availability}" --macosx)); mac_v=${mac_a[((${#mac_a[@]}-1))]}
	ios_a=($("${availability}" --ios)); ios_v=${ios_a[((${#ios_a[@]}-1))]}
	cd "${DSTROOT}" && find . -type f -name "*.h" | perl -e "
		use lib qw(${verifier}); use availability_rewriter;
		print(\"Rewrite headers pending <rdar://problem/8492436>:\n\");
		while (<STDIN>) {
			chomp(\$_);
			print(\"  rewriting \$_\n\");
			Availability_Rewriter::rewrite(\$_, \"${mac_v}\", \"${ios_v}\");
		}
		print (\"Done\n\n\");"
fi
# #endif __OPEN_SOURCE__
