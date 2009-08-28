#! /usr/bin/perl -w
#
# Class name: ObjCContainer
# Synopsis: Container for doc declared in an Objective-C interface.
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
package HeaderDoc::ObjCContainer;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use HeaderDoc::APIOwner;

# Inheritance
@ISA = qw( HeaderDoc::APIOwner );

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::ObjCContainer::VERSION = '$Revision: 1.9 $';

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
    $self->{CLASS} = "HeaderDoc::ObjCContainer";
}

sub _old_getCompositePageString { 
    my $self = shift;
    my $name = $self->name();
    my $compositePageString;
    my $contentString;

    $compositePageString .= $self->compositePageAPIRef();

    my $abstract = $self->abstract();
    if (length($abstract)) {
	    $compositePageString .= "<h2>Abstract</h2>\n";
	    $compositePageString .= $abstract;
    }

    my $discussion = $self->discussion();
    my $checkDisc = $self->halfbaked_discussion();
    if (length($checkDisc)) {
	    $compositePageString .= "<h2>Discussion</h2>\n";
	    $compositePageString .= $discussion;
    }
    
    # if ((length($abstract)) || (length($discussion))) {
    # ALWAYS....
	    $compositePageString .= "<hr><br>";
    # }

    my $etoc = $self->_getClassEmbeddedTOC(1);
    if (length($etoc)) {
	$compositePageString .= $etoc;
	$compositePageString .= "<hr><br>";
    }

    $contentString= $self->_getMethodDetailString(1);
    if (length($contentString)) {
	    $compositePageString .= "<h2>Methods</h2>\n";
		# $contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }

    $contentString= $self->_getVarDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Variables</h2>\n";
		# $contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }

    $contentString= $self->_getConstantDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Constants</h2>\n";
		# $contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    return $compositePageString;
}


sub getMethodPrefix {
    my $self = shift;
	my $obj = shift;
	my $prefix;
	my $type;
	
	$type = $obj->isInstanceMethod();
	
	if ($type =~ /YES/o) {
	    $prefix = "- ";
	} elsif ($type =~ /NO/o) {
	    $prefix = "+ ";
	} else {
	    $prefix = "";
	}
	
	return $prefix;
}

sub docNavigatorComment {
    my $self = shift;
    my $name = $self->name();
    $name =~ s/;//sgo;
    
    return "<!-- headerDoc=cl; name=$name-->";
}

################## Misc Functions ###################################
sub objName { # used for sorting
    my $obj1 = $a;
    my $obj2 = $b;

    return (lc($obj1->name()) cmp lc($obj2->name()));
}

sub byMethodType { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   if ($HeaderDoc::sort_entries) {
        return ($obj1->isInstanceMethod() cmp $obj2->isInstanceMethod());
   } else {
        return (1 cmp 2);
   }
}

sub byAccessControl { # used for sorting
    my $obj1 = $a;
    my $obj2 = $b;
    return (lc($obj1->accessControl()) cmp lc($obj2->accessControl()));
}

sub objGroup { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   # if ($HeaderDoc::sort_entries) {
        return (lc($obj1->group()) cmp lc($obj2->group()));
   # } else {
        # return (1 cmp 2);
   # }
}

sub conformsToList {
    my $self = shift;
    my $string = shift;
    my $localDebug = 0;

    print STDERR "ObjC object ".$self->name." conforms to: ".$string."\n" if ($localDebug);
    $string =~ s/\s*//sg;
    $string =~ s/,/\cA/g;

    if ($string ne "") {
	$self->attribute("Conforms&nbsp;to", $string, 0, 1);
    }
}

##################### Debugging ####################################

sub printObject {
    my $self = shift;
 
    print STDERR "------------------------------------\n";
    print STDERR "ObjCContainer\n";
    print STDERR "    - no ivars\n";
    print STDERR "Inherits from:\n";
    $self->SUPER::printObject();
}

1;
