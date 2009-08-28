#! /usr/bin/perl -w
#
# Class name: CPPClass
# Synopsis: Holds comments pertaining to a C++ class, as parsed by HeaderDoc
# from a C++ header
#
# Last Updated: $Date: 2009/03/30 19:38:50 $
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
package HeaderDoc::CPPClass;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash sanitize);
use HeaderDoc::APIOwner;

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::CPPClass::VERSION = '$Revision: 1.16 $';

# Inheritance
@ISA = qw( HeaderDoc::APIOwner );
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
    $self->{ISCOMINTERFACE} = 0;
    $self->{CLASS} = "HeaderDoc::CPPClass";
}

sub clone {
    my $self = shift;
    my $clone = undef;
    if (@_) {
	$clone = shift;
    } else {
	$clone = HeaderDoc::CPPClass->new();
    }

    $self->SUPER::clone($clone);

    $clone->{ISCOMINTERFACE} = $self->{ISCOMINTERFACE};
    return $clone;
}

sub isCOMInterface {
    my $self = shift;

    if (@_) {
	$self->{ISCOMINTERFACE} = shift;
    }

    return $self->{ISCOMINTERFACE};
}


sub _old_getCompositePageString { 
    my $self = shift;
    my $name = $self->name();
    my $compositePageString;
    my $contentString;
    my $list_attributes = $self->getAttributeLists(1);

    $compositePageString .= $self->compositePageAPIRef();
    
    my $abstract = $self->abstract();
    if (length($abstract)) {
	    $compositePageString .= "<h2>Abstract</h2>\n";
	    $compositePageString .= $abstract;
    }

    my $namespace = $self->namespace();
    my $availability = $self->availability();
    my $updated = $self->updated();

    if (length($namespace) || length($updated) || length($availability)) {
	    $compositePageString .= "<p></p>\n";
    }

    if (length($namespace)) {
	    $compositePageString .= "<b>Namespace:</b> $namespace<br>\n";
    }
    if (length($availability)) {
	    $compositePageString .= "<b>Availability:</b> $availability<br>\n";
    }
    if (length($updated)) {
	    $compositePageString .= "<b>Updated:</b> $updated<br>\n";
    }

    my $short_attributes = $self->getAttributes(0);
    my $long_attributes = $self->getAttributes(1);
    my $list_attributes = $self->getAttributeLists(1);
    if (length($short_attributes)) {
            $compositePageString .= "$short_attributes";
    }
    if (length($long_attributes)) {
            $compositePageString .= "$long_attributes";
    }
    if (length($list_attributes)) {
	$contentString .= $list_attributes;
    }

    my $discussion = $self->discussion();
    my $checkDisc = $self->halfbaked_discussion();
    if (length($checkDisc)) {
	    $compositePageString .= "<h2>Discussion</h2>\n";
	    $compositePageString .= $discussion;
    }
    if (length($long_attributes)) {
            $compositePageString .= "$long_attributes";
    }
    
    # if ((length($long_attributes)) || (length($discussion))) {
    # ALWAYS....
	    $compositePageString .= "<hr><br>";
    # }

    my $etoc = $self->_getClassEmbeddedTOC(1);
    if (length($etoc)) {
	$compositePageString .= $etoc;
	$compositePageString .= "<hr><br>";
    }

    $contentString= $self->_getFunctionDetailString(1);
    if (length($contentString)) {
	    $compositePageString .= "<h2>Member Functions</h2>\n";
		# $contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getVarDetailString(1);
    if (length($contentString)) {
	    $compositePageString .= "<h2>Member Data</h2>\n";
		# $contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getConstantDetailString(1);
    if (length($contentString)) {
	    $compositePageString .= "<h2>Constants</h2>\n";
		# $contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getTypedefDetailString(1);
    if (length($contentString)) {
	    $compositePageString .= "<h2>Typedefs</h2>\n";
		# $contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getStructDetailString(1);
    if (length($contentString)) {
	    $compositePageString .= "<h2>Structs and Unions</h2>\n";
		# $contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getEnumDetailString(1);
    if (length($contentString)) {
	    $compositePageString .= "<h2>Enumerations</h2>\n";
		# $contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }

    $contentString= $self->_getPDefineDetailString(1);
    if (length($contentString)) {
	    $compositePageString .= "<h2>#defines</h2>\n";
		# $contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }  
    return $compositePageString;
}

sub getMethodType {
    my $self = shift;

	my $declaration = shift;
	my $methodType = "instm";
	
	if ($declaration =~ /^\s*static/o) {
	    $methodType = "clm";
	}
	if ($self->sublang() eq "C") {
		# COM interfaces, C pseudoclasses
		$methodType = "intfm";
	}
	return $methodType;
}

sub old_getParamSignature {
    my $self = shift;
	my $declaration = shift;
	my $sig;
	my @params;
	
	$declaration =~ s/^[^(]+\(([^)]*)\).*/$1/o;
	@params = split (/,/, $declaration);
	foreach my $paramString (@params) {
	    my @paramElements = split (/\s+/, $paramString);
	    my $lastElement = pop @paramElements;
	    $sig .= join ("", @paramElements);
	    if ($lastElement =~ /^\*.*/o) {$sig .= "*";};  #if the arg was a pointer
	}
	return $sig;
}

# we add the apple_ref markup to the navigator comment to identify
# to Project Builder and other applications indexing the documentation
# that this is the entry point for documentation for this class
sub docNavigatorComment {
    my $self = shift;
    my $name = $self->name();
    $name =~ s/;//sgo;
    # my $uid = "//apple_ref/cpp/cl/$name";
    my $type = "cl";

    if ($self->fields()) {
	# $uid = "//apple_ref/cpp/tmplt/$name";
	$type = "tmpl";
    }
    # registerUID($uid);

    my $uid = $self->apiuid($type);

    my $indexgroup = $self->indexgroup(); my $igstring = "";
    if (length($indexgroup)) { $igstring = "indexgroup=$indexgroup;"; }

    my $appleRef = "<a name=\"$uid\"></a>";
    my $navComment = "<!-- headerDoc=cl; uid=$uid; $igstring name=$name-->";
    
    return "$navComment\n$appleRef";
}

################## Misc Functions ###################################


sub objName { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   return (lc($obj1->name()) cmp lc($obj2->name()));
}

sub byLinkage { # used for sorting
    my $obj1 = $a;
    my $obj2 = $b;
    return (lc($obj1->linkageState()) cmp lc($obj2->linkageState()));
}

sub byAccessControl { # used for sorting
    my $obj1 = $a;
    my $obj2 = $b;
    return (lc($obj1->accessControl()) cmp lc($obj2->accessControl()));
}

sub objGroup { # used for sorting
    my $obj1 = $a;
    my $obj2 = $b;
    return (lc($obj1->group()) cmp lc($obj2->group()));
}

sub linkageAndObjName { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   my $linkAndName1 = $obj1->linkageState() . $obj1->name();
   my $linkAndName2 = $obj2->linkageState() . $obj2->name();
   if ($HeaderDoc::sort_entries) {
        return (lc($linkAndName1) cmp lc($linkAndName2));
   } else {
        return byLinkage($obj1, $obj2);
   }
}

##################### Debugging ####################################

sub printObject {
    my $self = shift;
 
    $self->SUPER::printObject();
    print STDERR "CPPClass\n";
    print STDERR "\n";
}

1;

