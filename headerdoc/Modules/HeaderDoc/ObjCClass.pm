#! /usr/bin/perl -w
#
# Class name: ObjCClass
# Synopsis: Container for doc declared in an Objective-C class.
#
# Initial modifications: SKoT McDonald <skot@tomandandy.com> Aug 2001
#
# Last Updated: $Date: 2009/03/30 19:38:51 $
# 
# Copyright (c) 1999-2004 Apple Computer, Inc.  All rights reserved.
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
######################################################################
BEGIN {
	foreach (qw(Mac::Files)) {
	    $MOD_AVAIL{$_} = eval "use $_; 1";
    }
}
package HeaderDoc::ObjCClass;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use HeaderDoc::ObjCContainer;

# Inheritance
@ISA = qw( HeaderDoc::ObjCContainer );

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::ObjCClass::VERSION = '$Revision: 1.4 $';

################ Portability ###################################
my $isMacOS;
my $pathSeparator;
if ($^O =~ /MacOS/io) {
	$pathSeparator = ":";
	$isMacOS = 1;
} else {
	$pathSeparator = "/";
	$isMacOS = 0;
}
################ General Constants ###################################
my $debugging = 0;
my $tracing = 0;
my $outputExtension = ".html";
my $tocFrameName = "toc.html";
# my $theTime = time();
# my ($sec, $min, $hour, $dom, $moy, $year, @rest);
# ($sec, $min, $hour, $dom, $moy, $year, @rest) = localtime($theTime);
# $moy++;
# $year += 1900;
# my $dateStamp = "$moy/$dom/$year";
######################################################################

sub _initialize {
    my($self) = shift;
    $self->SUPER::_initialize();
    $self->tocTitlePrefix('Class:');
    $self->{CLASS} = "HeaderDoc::ObjCClass";
}

sub getMethodType {
    my $self = shift;
	my $declaration = shift;
	my $methodType = "";
		
	if ($declaration =~ /^\s*-/o) {
	    $methodType = "instm";
	} elsif ($declaration =~ /^\s*\+/o) {
	    $methodType = "clm";
	} else {
		$methodType = HeaderDoc::CPPClass::getMethodType($self, $declaration);
		## # my $filename = $HeaderDoc::headerObject->filename();
		## my $filename = $self->filename();
		## my $linenum = $self->linenum();
		## if (!$HeaderDoc::ignore_apiuid_errors) {
			## print STDERR "$filename:$linenum: warning: Unable to determine whether declaration is for an instance or class method[class]. '$declaration'\n";
		## }
	}
	return $methodType;
}

# we add the apple_ref markup to the navigator comment to identify
# to Project Builder and other applications indexing the documentation
# that this is the entry point for documentation for this class
sub docNavigatorComment {
    my $self = shift;
    my $name = $self->name();
    $name =~ s/;//sgo;
    my $uid = $self->apiuid("cl"); # "//apple_ref/occ/cl/$name";

    my $indexgroup = $self->indexgroup(); my $igstring = "";
    if (length($indexgroup)) { $igstring = "indexgroup=$indexgroup;"; }

    my $navComment = "<!-- headerDoc=cl; uid=$uid; $igstring name=$name-->";
    my $appleRef = "<a name=\"$uid\"></a>";
    
    return "$navComment\n$appleRef";
}

################## Misc Functions ###################################
sub objName { # used for sorting
    my $obj1 = $a;
    my $obj2 = $b;
    return (lc($obj1->name()) cmp lc($obj2->name()));
}


1;
