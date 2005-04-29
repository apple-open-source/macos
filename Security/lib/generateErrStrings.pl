#!/usr/bin/perl
#
# Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
# generatorX.pl - create error strings files from the Security header files
#
# John Hurley, Summer 2003. Based on generator.pl, Perry The Cynic, Fall 1999.
#
# Usage:
#	perl generatorX.pl input-directory output-directory <files>
#
#	Currently supported files are SecBase.h, SecureTransport.h and Authorization.h
#
#	perl generatorX.pl `pwd` `pwd` SecBase2.h SecureTransport2.h Authorization.h
#
#	Input will be like:
#
#		errSSLProtocol				= -9800,	/* SSL protocol error */
#		errSSLNegotiation			= -9801,	/* Cipher Suite negotiation failure */
#
#	Output should be like (in Unicode):
#
#		/* errSSLProtocol */
#		"-9800" = "SSL protocol error";
#
#		/* errSSLNegotiation */
#		"-9801" = "Cipher Suite negotiation failure";
#
# Note that the list of errors must be numerically unique across all input files, or the strings file
# will be invalid.Comments that span multiple lines will be ignored, as will lines with no comment. C++
# style comments are not supported.
#
use strict;
use Encode;

my $INPUTFILE=$ARGV[0];				# list of input files
my $FRAMEWORK=$ARGV[1];				# directory containing Security.framework
my $TARGETFILE=$ARGV[2];			# where to put the output file

my $tabs = "\t\t\t";	# argument indentation (noncritical)
my $warning = "This file was automatically generated. Do not edit on penalty of futility!";

#
# Read error headers into memory (all just concatenated blindly)
#
open(ERR, "$INPUTFILE") or die "Cannot open $INPUTFILE";
$/=undef;	# big gulp mode
$_ = <ERR>;
close(ERR);

#
# Prepare output file
#
open(OUT, ">$TARGETFILE") or die "Cannot write $TARGETFILE: $^E";
my $msg = "//\n// Security error code tables.\n// $warning\n//\n";


#
# Extract errors from accumulated header text. Format:
#   errBlahWhatever = number, /* text */
#
my @errorlines =
	m{(?:^\s*)(err[Sec|Authorization|SSL]\w+)(?:\s*=\s*)(-?\d+)(?:\s*,?\s*)(?:/\*\s*)(.*)(?:\*/)(?:$\s*)}gm;
while (my $errx = shift @errorlines)
{
    my $value = shift @errorlines;	# or die;
    my $str = shift @errorlines;	# or die;
    $str =~ s/\s*$//;		# drop trailing white space
    if ( $value != 0)  		# can't output duplicate error codes
    {
        $msg = $msg . "\n/* $errx */\n\"$value\" = \"$str\";\n";
    }
};
$msg = $msg . "\n";


#
# Extract errors from CSSM headers. Format:
#  CSSMERR_whatever = some compile-time C expression
# [We just build a C program and running it. So sue us.]
#
my $PROG = "/tmp/cssmerrors.$$.c";
my $PROGB = "/tmp/cssmerrors.$$";

open(PROG, ">$PROG") or die "Cannot open $PROG";
print PROG <<END;
#include <Security/cssmerr.h>
#include <Security/cssmapple.h>
#include <stdio.h>
int main() {
END
@errorlines =
	m{(?:^\s*)CSSMERR_([A-Z_]+)\s+=}gm;
for my $error (@errorlines) {
	print PROG "printf(\"\\n/* CSSMERR_$error */\\n\\\"%ld\\\" = \\\"$error\\\";\\n\", CSSMERR_$error);\n";
}
print PROG "}\n";
close(PROG);

system("cc", "-o", $PROGB, $PROG, "-I$FRAMEWORK/SecurityPieces/Headers") == 0 or die "cannot build CSSM collector";
open(PROGR, "$PROGB|") or die "Cannot run CSSM collector";
$msg .= <PROGR>;
close(PROGR);

#
# Write output file and clean up
#
print OUT encode("UTF-16", $msg,  Encode::FB_PERLQQ);
close(OUT);
