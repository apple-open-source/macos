#!/usr/bin/perl
#
# Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

use strict;

# exec make, but first run make with a target of no64.  If it outputs YES,
# then remove 64-bit arches from RC_CFLAGS and RC_ARCHS, remover RC_xxx,
# where xxx is a 64-bit architecture.  If there are no archs left, just
# return success.

my $dir = '.';

for(0...$#ARGV) {
    if($ARGV[$_] eq '-C') {
	$dir = $ARGV[$_ + 1];
	last;
    }
}

my $no64 = `make -C $dir no64`;
chomp($no64);

if($no64 eq 'YES') {
    my @archs;
    my @arch64;
    my @cflags;
    my $arch = 0;
    for(split(" ", $ENV{RC_CFLAGS})) {
	if($arch) {
	    if(/64/) {
		push(@arch64, $_);
	    } else {
		push(@cflags, '-arch', $_);
		push(@archs, $_);
	    }
	    $arch = 0;
	    next;
	}
	if($_ eq '-arch') {
	    $arch = 1;
	    next;
	}
	push(@cflags, $_);
    }
    unless(scalar(@archs) > 0) {
	print "Not building:\tmake @ARGV\n";
	exit 0;
    }
    $ENV{RC_CFLAGS} = join(' ', @cflags);
    $ENV{RC_ARCHS} = join(' ', @archs);
    push(@ARGV, "RC_CFLAGS=$ENV{RC_CFLAGS}", "RC_ARCHS=$ENV{RC_ARCHS}");
    for(@arch64) {
	delete($ENV{"RC_$_"});
	push(@ARGV, "RC_$_=");
    }
}
print "make @ARGV\n";
exec {'make'} 'make', @ARGV;
