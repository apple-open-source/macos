#! /usr/bin/perl -w
#
# Class name: ObjCCategory
# Synopsis: Holds comments pertaining to an ObjC category, as parsed by HeaderDoc
# from an ObjC header
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
package HeaderDoc::ObjCCategory;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash registerUID unregisterUID);
use HeaderDoc::ObjCContainer;

# Inheritance
@ISA = qw( HeaderDoc::ObjCContainer );

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::ObjCCategory::VERSION = '$Revision: 1.6 $';

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
    $self->tocTitlePrefix('Category:');
    $self->{CLASS} = "HeaderDoc::ObjCCategory";
}

sub className {
    my $self = shift;
    my ($className, $categoryName) = &getClassAndCategoryName($self->name(), $self->fullpath(), $self->linenum());
    return $className;
}

sub categoryName {
    my $self = shift;
    my ($className, $categoryName) = &getClassAndCategoryName($self->name(), $self->fullpath(), $self->linenum());
    return $categoryName;
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
		## # my $fullpath = $HeaderDoc::headerObject->fullpath();
		## my $fullpath = $self->fullpath();
		## my $linenum = $self->linenum();
		## if (!$HeaderDoc::ignore_apiuid_errors) {
			## print STDERR "$fullpath:$linenum: warning: Unable to determine whether declaration is for an instance or class method[cat]. '$declaration'\n";
		## }
	}
	return $methodType;
}

# we add the apple_ref markup to the navigator comment to identify
# to Project Builder and other applications indexing the documentation
# that this is the entry point for documentation for this category
sub docNavigatorComment {
    my $self = shift;
    my $name = $self->name();

    my $olduid = $self->apiuid();
    
    # regularize name by removing spaces and semicolons, if any
    $name =~ s/\s+//go;
    $name =~ s/;//sgo;

    my $indexgroup = $self->indexgroup(); my $igstring = "";
    if (length($indexgroup)) { $igstring = "indexgroup=$indexgroup;"; }
    
    my $uid = $self->apiuid("cat"); # "//apple_ref/occ/cat/$name";
    my $navComment = "<!-- headerDoc=cat; uid=$uid; $igstring name=$name-->";
    my $appleRef = "<a name=\"$uid\"></a>";

    unregisterUID($olduid, $name, $self);
    registerUID($uid, $name, $self);
    
    return "$navComment\n$appleRef";
}

################## Misc Functions ###################################
sub objName { # used for sorting
    my $obj1 = $a;
    my $obj2 = $b;
    return (lc($obj1->name()) cmp lc($obj2->name()));
}

sub getClassAndCategoryName {
    my $fullName = shift;
    my $className = '';
    my $categoryName = '';
    my $fullpath = shift; # $HeaderDoc::headerObject->fullpath();
    my $linenum = shift; 

    if ($fullName =~ /(\w+)\s*(\((.*)\))?/o) {
    	$className = $1;
    	$categoryName =$3;
    	if (!length ($className)) {
            print STDERR "$fullpath:$linenum: warning: Couldn't determine class name from category name '$fullName'.\n";
    	}
    	if (!length ($categoryName)) {
            print STDERR "$fullpath:$linenum: warning: Couldn't determine category name from category name '$fullName'.\n";
    	}
    } else {
        print STDERR "$fullpath:$linenum: warning: Specified category name '$fullName' isn't complete. Expecting a name of the form 'MyClass(CategoryName)'\n";
    }
    return ($className, $categoryName);
}


##################### Debugging ####################################

sub printObject {
    my $self = shift;
    my $className = $self->className();
    my $categoryName = $self->categoryName();
 
    print STDERR "------------------------------------\n";
    print STDERR "ObjCCategory\n";
    print STDERR "    associated with class: $className\n";
    print STDERR "    category name: $categoryName\n";
    print STDERR "Inherits from:\n";
    $self->SUPER::printObject();
}

1;

