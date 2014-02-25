#!/bin/bash -e

# #ifndef __OPEN_SOURCE__

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
