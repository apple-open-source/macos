#! /usr/bin/perl -w
#
# Class name: ObjCCategory
# Synopsis: Holds comments pertaining to an ObjC category, as parsed by HeaderDoc
# from an ObjC header
#
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2003/07/23 23:00:55 $
# 
# Copyright (c) 1999-2001 Apple Computer, Inc.  All Rights Reserved.
# The contents of this file constitute Original Code as defined in and are
# subject to the Apple Public Source License Version 1.1 (the "License").
# You may not use this file except in compliance with the License.  Please
# obtain a copy of the License at http://www.apple.com/publicsource and
# read it before using this file.
#
# This Original Code and all software distributed under the License are
# distributed on an TAS ISU basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the License for
# the specific language governing rights and limitations under the
# License.
#
######################################################################
BEGIN {
	foreach (qw(Mac::Files)) {
	    $MOD_AVAIL{$_} = eval "use $_; 1";
    }
}
package HeaderDoc::ObjCCategory;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use HeaderDoc::ObjCContainer;

# Inheritance
@ISA = qw( HeaderDoc::ObjCContainer );

use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';

################ Portability ###################################
my $isMacOS;
my $pathSeparator;
if ($^O =~ /MacOS/i) {
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
my $theTime = time();
my ($sec, $min, $hour, $dom, $moy, $year, @rest);
($sec, $min, $hour, $dom, $moy, $year, @rest) = localtime($theTime);
$moy++;
$year += 1900;
my $dateStamp = "$moy/$dom/$year";
######################################################################

sub _initialize {
    my($self) = shift;
	$self->SUPER::_initialize();
    $self->tocTitlePrefix('Category:');
}

sub className {
    my $self = shift;
    my ($className, $categoryName) = &getClassAndCategoryName($self->name());
    return $className;
}

sub categoryName {
    my $self = shift;
    my ($className, $categoryName) = &getClassAndCategoryName($self->name());
    return $categoryName;
}

sub getMethodType {
    my $self = shift;
	my $declaration = shift;
	my $methodType = "";
		
	if ($declaration =~ /^\s*-/) {
	    $methodType = "instm";
	} elsif ($declaration =~ /^\s*\+/) {
	    $methodType = "clm";
	} else {
		my $filename = $HeaderDoc::headerObject->filename();
		print "$filename:0:Unable to determine whether declaration is for an instance or class method.\n";
		print "$filename:0:     '$declaration'\n";
	}
	return $methodType;
}

# we add the apple_ref markup to the navigator comment to identify
# to Project Builder and other applications indexing the documentation
# that this is the entry point for documentation for this category
sub docNavigatorComment {
    my $self = shift;
    my $name = $self->name();
    
    # regularize name by removing spaces, if any
    $name =~ s/\s+//g;
    
    my $navComment = "<!-- headerDoc=cat; name=$name-->";
    my $appleRef = "<a name=\"//apple_ref/occ/cat/$name\"></a>";
    
    return "$navComment\n$appleRef";
}

################## Misc Functions ###################################
sub objName { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   if ($HeaderDoc::sort_entries) {
        return ($obj1->name() cmp $obj2->name());
   } else {
        return (1 cmp 2);
   }
}

sub getClassAndCategoryName {
    my $fullName = shift;
	my $className = '';
	my $categoryName = '';

    if ($fullName =~ /(\w+)\s*(\((.*)\))?/) {
    	$className = $1;
    	$categoryName =$3;
    	if (!length ($className)) {
	    my $filename = $HeaderDoc::headerObject->filename();
            print "$filename:0:Couldn't determine class name from category name '$fullName'.\n";
    	}
    	if (!length ($categoryName)) {
	    my $filename = $HeaderDoc::headerObject->filename();
            print "$filename:0:Couldn't determine category name from category name '$fullName'.\n";
    	}
    } else {
	my $filename = $HeaderDoc::headerObject->filename();
        print "$filename:0:Specified category name '$fullName' isn't complete.\n";
        print "$filename:0:Expecting a name of the form 'MyClass(CategoryName)'\n";
    }
    return ($className, $categoryName);
}


##################### Debugging ####################################

sub printObject {
    my $self = shift;
    my $className = $self->className();
    my $categoryName = $self->categoryName();
 
    print "------------------------------------\n";
    print "ObjCCategory\n";
    print "    associated with class: $className\n";
    print "    category name: $categoryName\n";
    print "Inherits from:\n";
    $self->SUPER::printObject();
}

1;

