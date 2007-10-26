#! /usr/bin/perl -w
#
# Class name: CPPClass
# Synopsis: Holds comments pertaining to a C++ class, as parsed by HeaderDoc
# from a C++ header
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2005/10/15 01:26:29 $
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
$VERSION = '$Revision: 1.13.2.14.2.33 $';

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
    if (length($discussion)) {
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



# overriding inherited method to add access type on line above declaration
sub _old__getFunctionDetailString {
    my $self = shift;
    my $composite = shift;
    my @funcObjs = $self->functions();
    my $className = $self->name();
    my $contentString;
    # my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();

    $contentString = $self->_getFunctionEmbeddedTOC($composite);

    my @tempobjs = ();
    if ($HeaderDoc::sort_entries) {
	@tempobjs = sort objName @funcObjs;
    } else {
	@tempobjs = @funcObjs;
    }

    foreach my $obj (@tempobjs) {
        my $name = $obj->name();
        my $desc = $obj->discussion();
	my $throws = $obj->throws();
        my $abstract = $obj->abstract();
        my $availability = $obj->availability();
        my $updated = $obj->updated();
        my $declaration = $obj->declarationInHTML();
        my $declarationRaw = $obj->declaration();
        my $accessControl = $obj->accessControl();
        my @params = $obj->taggedParameters();
        my $result = $obj->result();
	my $list_attributes = $obj->getAttributeLists($composite);

	$contentString .= "<hr>";
	# if ($declaration !~ /#define/o) { # not sure how to handle apple_refs with macros yet
	        my $methodType = $self->getMethodType($declarationRaw);
		# registerUID($uid);
        	# $contentString .= "<a name=\"$uid\"></a>\n";
		my $apiref = "";
		$apiref = $obj->apiref($composite);
		$contentString .= $apiref;
        # }
	my $parentclass = $obj->origClass();
	if (length($parentclass)) { $parentclass .= "::"; }
	if ($self->CClass()) {
		# Don't do this for pseudo-classes.
		$parentclass = "";
	}
	$contentString .= "<table border=\"0\"  cellpadding=\"2\" cellspacing=\"2\" width=\"300\">";
	$contentString .= "<tr>";
	$contentString .= "<td valign=\"top\" height=\"12\" colspan=\"5\">";
	my $urlname = sanitize($name, 1);
	$contentString .= "<h2><a name=\"$urlname\">$parentclass$name</a></h2>\n";
	$contentString .= "</td>";
	$contentString .= "</tr></table>";
	# $contentString .= "<hr>";

	if (length($throws)) {  
		$contentString .= "<b>Throws:</b>\n$throws<BR>\n";
	}
	if (length($abstract)) {
            # $contentString .= "<b>Abstract:</b> $abstract<BR>\n";
            $contentString .= "$abstract<BR>\n";
        }
	if (length($availability)) {
	    $contentString .= "<b>Availability:</b> $availability<BR>\n";
	}
	if (length($updated)) {
	    $contentString .= "<b>Updated:</b> $updated<BR>\n";
	}
	if (length($list_attributes)) {
	    $contentString .= $list_attributes;
	}
        $contentString .= "<blockquote><pre><tt>$accessControl</tt>\n<br>$declaration</pre></blockquote>\n";
        if (length($desc)) {$contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">Discussion</font></h5><p>$desc</p>\n"; }
	    my $arrayLength = @params;
	    if ($arrayLength > 0) {
	        my $paramContentString;
	        foreach my $element (@params) {
	            my $pName = $element->name();
	            my $pDesc = $element->discussion();
	            if (length ($pName)) {
	                # $paramContentString .= "<tr><td align=\"center\"><tt>$pName</tt></td><td>$pDesc</td></tr>\n";
	                $paramContentString .= "<dt><tt>$pName</tt></dt><dd>$pDesc</dd>\n";
	            }
	        }
	        if (length ($paramContentString)){
		        $contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">Parameters</font></h5>\n";
		        $contentString .= "<blockquote>\n";
		        # $contentString .= "<table border=\"1\"  width=\"90%\">\n";
		        # $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
			$contentString .= "<dl>\n";
	            $contentString .= $paramContentString;
		        # $contentString .= "</table>\n</blockquote>\n";
		        $contentString .= "</dl>\n</blockquote>\n";
		    }
	    }
	if (length($result)) {
            $contentString .= "<dl><dt><i>function result</i></dt><dd>$result</dd></dl>\n";
        }
	    # $contentString .= "<hr>\n";
    }
    $contentString .= "<hr>\n";
    return $contentString;
}

sub _old__getVarDetailString {
    my $self = shift;
    my $composite = shift;
    my @varObjs = $self->vars();
    my $contentString;

    my @tempobjs = ();
    if ($HeaderDoc::sort_entries) {
	@tempobjs = sort objName @varObjs;
    } else {
	@tempobjs = @varObjs;
    }
    foreach my $obj (@tempobjs) {
        my $name = $obj->name();
	my $abstract = $obj->abstract();
	my $availability = $obj->availability();
	my $updated = $obj->updated();
        my $desc = $obj->discussion();
        my $declaration = $obj->declarationInHTML();
        my $accessControl = $obj->accessControl();
        my @fields = ();
        my $fieldHeading;
        if ($obj->can('fields')) { # for Structs, Unions, and Typedefs
            @fields = $obj->fields();
            $fieldHeading = "Fields";
        } elsif ($obj->can('constants')) { # for enums
            @fields = $obj->constants();
            $fieldHeading = "Constants";
        }
        if ($obj->can('isFunctionPointer')) {
        	if ($obj->isFunctionPointer()) {
            	$fieldHeading = "Parameters";
        	}
        }

	my $methodType = "data"; # $self->getMethodType($declarationRaw);
	# my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();
	my $apiowner = $self->apiOwner();
	my $headerObject = $apiowner->headerObject();
	# my $className = (HeaderDoc::APIOwner->headerObject())->name();
	my $className = $self->name();
	$contentString .= "<hr>";
	# my $uid = "//$apiUIDPrefix/cpp/$methodType/$className/$name";
	# registerUID($uid);
	# $contentString .= "<a name=\"$uid\"></a>\n";
	# Don't potentially change the uid....
	my $apiref = $obj->apiref($composite); # , $methodType);
	$contentString .= $apiref;
        
	$contentString .= "<table border=\"0\"  cellpadding=\"2\" cellspacing=\"2\" width=\"300\">";
	$contentString .= "<tr>";
	$contentString .= "<td valign=\"top\" height=\"12\" colspan=\"5\">";
	my $urlname = sanitize($name, 1);
	$contentString .= "<h2><a name=\"$urlname\">$name</a></h2>\n";
	$contentString .= "</td>";
	$contentString .= "</tr></table>";
	# $contentString .= "<hr>";
	if (length($abstract)) {
		# $contentString .= "<b>Abstract:</b> $abstract<BR>\n";
		$contentString .= "$abstract<BR>\n";
	}
	if (length($availability)) {
		$contentString .= "<b>Availability:</b> $availability<BR>\n";
	}
	if (length($updated)) {
		$contentString .= "<b>Updated:</b> $updated<BR>\n";
	}
        $contentString .= "<blockquote><tt>$accessControl</tt> $declaration</blockquote>\n";
        if (length($desc)) {$contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">Discussion</font></h5><p>$desc</p>\n"; }
        # $contentString .= "<p>$desc</p>\n";
	    my $arrayLength = @fields;
	    if ($arrayLength > 0) {
	        $contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">$fieldHeading</font></h5>\n";
	        $contentString .= "<blockquote>\n";
	        # $contentString .= "<table border=\"1\"  width=\"90%\">\n";
	        # $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
		$contentString .= "<dl>\n";
	        foreach my $element (@fields) {
	            my $fName = $element->name();
	            my $fDesc = $element->discussion();
	            # $contentString .= "<tr><td align=\"center\"><tt>$fName</tt></td><td>$fDesc</td></tr>\n";
	            $contentString .= "<dt><tt>$fName</tt></dt><dd>$fDesc</dd>\n";
	        }
	        # $contentString .= "</table>\n</blockquote>\n";
	        $contentString .= "</dl>\n</blockquote>\n";
	    }
	    # if (length($updated)) {
		# $contentString .= "<b>Updated:</b> $updated\n";
	    # }
    }
    $contentString .= "<hr>\n";
    return $contentString;
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
    print "CPPClass\n";
    print "\n";
}

1;

