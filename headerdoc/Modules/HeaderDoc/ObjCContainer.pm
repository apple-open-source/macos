#! /usr/bin/perl -w
#
# Class name: ObjCContainer
# Synopsis: Container for doc declared in an Objective-C interface.
#
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2001/11/30 22:43:17 $
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
package HeaderDoc::ObjCContainer;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use HeaderDoc::APIOwner;

# Inheritance
@ISA = qw( HeaderDoc::APIOwner );

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
    $self->tocTitlePrefix('Class:');
}

sub _getCompositePageString { 
    my $self = shift;
    my $name = $self->name();
    my $compositePageString;
    my $contentString;
    
    my $abstract = $self->abstract();
    if (length($abstract)) {
	    $compositePageString .= "<h2>Abstract</h2>\n";
	    $compositePageString .= $abstract;
    }

    my $discussion = $self->discussion();
    if (length($discussion)) {
	    $compositePageString .= "<h2>Discussion</h2>\n";
	    $compositePageString .= $discussion;
    }
    
    if ((length($abstract)) || (length($discussion))) {
	    $compositePageString .= "<hr><br>";
    }

    $contentString= $self->_getMethodDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Methods</h2>\n";
	    $compositePageString .= $contentString;
    }

    $contentString= $self->_getConstantDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Constants</h2>\n";
	    $compositePageString .= $contentString;
    }
    
    return $compositePageString;
}

sub tocString {
    my $self = shift;
    my $contentFrameName = $self->name();
    $contentFrameName =~ s/(.*)\.h/$1/; 
    # for now, always shorten long names since some files may be moved to a Mac for browsing
    if (1 || $isMacOS) {$contentFrameName = &safeName(filename => $contentFrameName);};
    $contentFrameName = $contentFrameName . ".html";
    my $header = $self->headerObject();
    my @meths = $self->methods();
	my $compositePageName = HeaderDoc::APIOwner->compositePageName();
	my $defaultFrameName = HeaderDoc::APIOwner->defaultFrameName();
    
    my $tocString = "<h3><a href=\"$contentFrameName\" target =\"doc\">Introduction</a></h3>\n";

    # output list of functions as TOC
    if (@meths) {
        my @classMethods;
        my @instanceMethods;
	    $tocString .= "<br><h4>Methods</h4><hr>\n";
	    foreach my $obj (sort byMethodType @meths) {
	        my $type = $obj->isInstanceMethod();
	        
	        if ($type =~ /NO/){
	            push (@classMethods, $obj);
	        } elsif ($type =~ /YES/){
	            push (@instanceMethods, $obj);
	        } else {
	            push (@instanceMethods, $obj);
	        }
	    }
	    if (@classMethods) {
	        $tocString .= "<h5>Class Methods</h5>\n";
		    foreach my $obj (sort objName @classMethods) {
	        	my $name = $obj->name();
	        	my $prefix = $self->getMethodPrefix($obj);
	        	$tocString .= "<nobr>&nbsp;<a href = \"Methods/Methods.html#$name\" target =\"doc\">$prefix$name</a></nobr><br>\n";
	        }
	    }
	    if (@instanceMethods) {
	        $tocString .= "<h5>Instance Methods</h5>\n";
		    foreach my $obj (sort objName @instanceMethods) {
	        	my $name = $obj->name();
	        	my $prefix = $self->getMethodPrefix($obj);
	        	$tocString .= "<nobr>&nbsp;<a href = \"Methods/Methods.html#$name\" target =\"doc\">$prefix$name</a></nobr><br>\n";
	        }
	    }
    }
	$tocString .= "<br><h4>Other Reference</h4><hr>\n";
	$tocString .= "<nobr>&nbsp;<a href = \"../../$defaultFrameName\" target =\"_top\">Header</a></nobr><br>\n";
    $tocString .= "<br><hr><a href=\"$compositePageName\" target =\"_blank\">[Printable HTML Page]</a>\n";
    return $tocString;
}

sub getMethodPrefix {
    my $self = shift;
	my $obj = shift;
	my $prefix;
	my $type;
	
	$type = $obj->isInstanceMethod();
	
	if ($type =~ /YES/) {
	    $prefix = "- ";
	} elsif ($type =~ /NO/) {
	    $prefix = "+ ";
	} else {
	    $prefix = "";
	}
	
	return $prefix;
}

sub docNavigatorComment {
    my $self = shift;
    my $name = $self->name();
    
    return "<-- headerDoc=cl; name=$name-->";
}

################## Misc Functions ###################################
sub objName { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   return ($obj1->name() cmp $obj2->name());
}

sub byMethodType { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   return ($obj1->isInstanceMethod() cmp $obj2->isInstanceMethod());
}

##################### Debugging ####################################

sub printObject {
    my $self = shift;
 
    print "------------------------------------\n";
    print "ObjCContainer\n";
    print "    - no ivars\n";
    print "Inherits from:\n";
    $self->SUPER::printObject();
}

1;