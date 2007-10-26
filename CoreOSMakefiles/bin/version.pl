#!/usr/bin/perl
#
# Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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
use POSIX ();

# Create a version string from various environment variables, from the
# argument list, or the current directory

my $name = $ARGV[0];
$name = $ENV{RC_ProjectName} unless defined($name) && $name ne '';
$name = $ENV{PROJECT_NAME} unless defined($name) && $name ne '';
my $vers = $ENV{RC_ProjectNameAndSourceVersion};
$vers = "$name-$ENV{RC_ProjectSourceVersion}" unless defined($vers) && $vers ne '' && defined($ENV{RC_ProjectSourceVersion}) && $ENV{RC_ProjectSourceVersion} ne '';
if(defined($vers) && $vers ne '') {
    if(defined($name) && $name ne '') {
	$vers =~ s/^[^-]*-/$name-/;
    } else {
	($name = $vers) =~ s/-.*//;
    }
    my $build = $ENV{RC_ProjectBuildVersion};
    $vers .= "~$build" if defined($build) && $build ne '';
} else {
    if(defined($name) && $name ne '') {
	$vers = $name;
    } else {
	require Cwd;
	$vers = Cwd::cwd();
	$name = time();
    }
}
printf "const char __%s_version[] = \"@(#) %s %s\";\n", $name, $vers,
    POSIX::strftime("%x %X %Z", localtime());
