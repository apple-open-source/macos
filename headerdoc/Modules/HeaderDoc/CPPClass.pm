#! /usr/bin/perl -w
#
# Class name: CPPClass
# Synopsis: Holds comments pertaining to a C++ class, as parsed by HeaderDoc
# from a C++ header
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
package HeaderDoc::CPPClass;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash);
use HeaderDoc::APIOwner;

use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';

# Inheritance
@ISA = qw( HeaderDoc::APIOwner );
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

sub tocString {
    my $self = shift;
    my $localDebug = 0;
    my $contentFrameName = $self->name();
    $contentFrameName =~ s/(.*)\.h/$1/; 
    # for now, always shorten long names since some files may be moved to a Mac for browsing
    if (1 || $isMacOS) {$contentFrameName = &safeName(filename => $contentFrameName);};
    $contentFrameName = $contentFrameName . ".html";
    my $header = $self->headerObject();
    my @funcs = $self->functions();    
    my @constants = $self->constants();
    my @typedefs = $self->typedefs();
    my @structs = $self->structs();
    my @enums = $self->enums();
    my @vars = $self->vars();
	my $compositePageName = HeaderDoc::APIOwner->compositePageName();
	my $defaultFrameName = HeaderDoc::APIOwner->defaultFrameName();
    
    my $tocString = "<h3><a href=\"$contentFrameName\" target =\"doc\">Introduction</a></h3>\n";

    
    # output list of functions as TOC
    if (@funcs) {
        my @publics;
        my @protecteds;
        my @privates;
	    $tocString .= "<br><h4>Member Functions</h4><hr>\n";
	    foreach my $obj (sort byAccessControl @funcs) {
	        my $access = $obj->accessControl();
	        
	        if ($access =~ /public/){
	            push (@publics, $obj);
	        } elsif ($access =~ /protected/){
	            push (@protecteds, $obj);
	        } elsif ($access =~ /private/){
	            push (@privates, $obj);
	        }
	    }
	    if (@publics) {
	        $tocString .= "<h5>Public</h5>\n";
		    foreach my $obj (sort objName @publics) {
	        	my $name = $obj->name();
	        	$tocString .= "<nobr>&nbsp;<a href = \"Functions/Functions.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	        }
	    }
	    if (@protecteds) {
	        $tocString .= "<h5>Protected</h5>\n";
		    foreach my $obj (sort objName @protecteds) {
	        	my $name = $obj->name();
	        	$tocString .= "<nobr>&nbsp;<a href = \"Functions/Functions.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	        }
	    }
	    if (@privates) {
	        $tocString .= "<h5>Private</h5>\n";
		    foreach my $obj (sort objName @privates) {
	        	my $name = $obj->name();
	        	$tocString .= "<nobr>&nbsp;<a href = \"Functions/Functions.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	        }
	    }
    }
    if (@typedefs) {
	    $tocString .= "<h4>Defined Types</h4>\n";
	    foreach my $obj (sort objName @typedefs) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href = \"DataTypes/DataTypes.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	    }
    }
    if (@structs) {
	    $tocString .= "<h4>Structs</h4>\n";
	    foreach my $obj (sort objName @structs) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href = \"Structs/Structs.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	    }
    }
    if (@constants) {
	    $tocString .= "<h4>Constants</h4>\n";
	    foreach my $obj (sort objName @constants) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href = \"Constants/Constants.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	    }
	}
    if (@enums) {
	    $tocString .= "<h4>Enumerations</h4>\n";
	    foreach my $obj (sort objName @enums) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href = \"Enums/Enums.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	    }
	}
    if (@vars) {
        my @publics;
        my @protecteds;
        my @privates;

	    $tocString .= "<br><h4>Member Data</h4><hr>\n";
	    foreach my $obj (sort byAccessControl @vars) {
	        my $access = $obj->accessControl();

	        if ($access =~ /public/){
	            push (@publics, $obj);
	        } elsif ($access =~ /protected/){
	            push (@protecteds, $obj);
	        } elsif ($access =~ /private/){
	            push (@privates, $obj);
	        }
	    }
	    if (@publics) {
	        $tocString .= "<h5>Public</h5>\n";
		    foreach my $obj (sort objName @publics) {
	        	my $name = $obj->name();
	        	$tocString .= "<nobr>&nbsp;<a href = \"Vars/Vars.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	        }
	    }
	    if (@protecteds) {
	        $tocString .= "<h5>Protected</h5>\n";
		    foreach my $obj (sort objName @protecteds) {
	        	my $name = $obj->name();
	        	$tocString .= "<nobr>&nbsp;<a href = \"Vars/Vars.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	        }
	    }
	    if (@privates) {
	        $tocString .= "<h5>Private</h5>\n";
		    foreach my $obj (sort objName @privates) {
	        	my $name = $obj->name();
	        	$tocString .= "<nobr>&nbsp;<a href = \"Vars/Vars.html#$name\" target =\"doc\">$name</a></nobr><br>\n";
	        }
	    }
	}
	$tocString .= "<br><h4>Other Reference</h4><hr>\n";
	$tocString .= "<nobr>&nbsp;<a href = \"../../$defaultFrameName\" target =\"_top\">Header</a></nobr><br>\n";
    $tocString .= "<br><hr><a href=\"$compositePageName\" target =\"_blank\">[Printable HTML Page]</a>\n";
    
	print "*** finished toc\n" if ($localDebug);
	
    return $tocString;
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

    $contentString= $self->_getFunctionDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Member Functions</h2>\n";
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getVarDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Member Data</h2>\n";
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getConstantDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Constants</h2>\n";
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getTypedefDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Typedefs</h2>\n";
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getStructDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Structs</h2>\n";
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getEnumDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Enumerations</h2>\n";
	    $compositePageString .= $contentString;
    }

    $contentString= $self->_getPDefineDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>#defines</h2>\n";
	    $compositePageString .= $contentString;
    }  
    return $compositePageString;
}



# overriding inherited method to add access type on line above declaration
sub _getFunctionDetailString {
    my $self = shift;
    my @funcObjs = $self->functions();
    my $className = $self->name();
    my $contentString;
    my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();

    foreach my $obj (sort objName @funcObjs) {
        my $name = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
        my $declaration = $obj->declarationInHTML();
        my $declarationRaw = $obj->declaration();
        my $accessControl = $obj->accessControl();
        my @params = $obj->taggedParameters();
        my $result = $obj->result();
		if ($declaration !~ /#define/) { # not sure how to handle apple_refs with macros yet
	        my $paramSignature = $self->getParamSignature($declarationRaw);
	        my $methodType = $self->getMethodType($declarationRaw);
        	$contentString .= "<a name=\"//$apiUIDPrefix/cpp/$methodType/$className/$name/$paramSignature\"></a>\n";
        }
        $contentString .= "<h3><a name=\"$name\">$name</a></h3>\n";
	    if (length($abstract)) {
            $contentString .= "<b>Abstract:</b> $abstract\n";
        }
        $contentString .= "<blockquote><pre><tt>$accessControl</tt>\n<br>$declaration</pre></blockquote>\n";
        $contentString .= "<p>$desc</p>\n";
	    my $arrayLength = @params;
	    if ($arrayLength > 0) {
	        my $paramContentString;
	        foreach my $element (@params) {
	            my $pName = $element->name();
	            my $pDesc = $element->discussion();
	            if (length ($pName)) {
	                $paramContentString .= "<tr><td align = \"center\"><tt>$pName</tt></td><td>$pDesc</td><tr>\n";
	            }
	        }
	        if (length ($paramContentString)){
		        $contentString .= "<h4>Parameters</h4>\n";
		        $contentString .= "<blockquote>\n";
		        $contentString .= "<table border = \"1\"  width = \"90%\">\n";
		        $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
	            $contentString .= $paramContentString;
		        $contentString .= "</table>\n</blockquote>\n";
		    }
	    }
	    if (length($result)) {
            $contentString .= "<b>Result:</b> $result\n";
        }
	    $contentString .= "<hr>\n";
    }
    return $contentString;
}

sub _getVarDetailString {
    my $self = shift;
    my @varObjs = $self->vars();
    my $contentString;

    foreach my $obj (sort objName @varObjs) {
        my $name = $obj->name();
        my $desc = $obj->discussion();
        my $declaration = $obj->declarationInHTML();
        my $accessControl = $obj->accessControl();
        my @fields = ();
        my $fieldHeading;
        if ($obj->can('fields')) { # for Structs and Typedefs
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
        
        $contentString .= "<h3><a name=\"$name\">$name</a></h3>\n";
        $contentString .= "<blockquote><tt>$accessControl</tt>$declaration</blockquote>\n";
        $contentString .= "<p>$desc</p>\n";
	    my $arrayLength = @fields;
	    if ($arrayLength > 0) {
	        $contentString .= "<h4>$fieldHeading</h4>\n";
	        $contentString .= "<blockquote>\n";
	        $contentString .= "<table border = \"1\"  width = \"90%\">\n";
	        $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
	        foreach my $element (@fields) {
	            my $fName = $element->name();
	            my $fDesc = $element->discussion();
	            $contentString .= "<tr><td align = \"center\"><tt>$fName</tt></td><td>$fDesc</td><tr>\n";
	        }
	        $contentString .= "</table>\n</blockquote>\n";
	    }
	    $contentString .= "<hr>\n";
    }
    return $contentString;
}

sub getMethodType {
    my $self = shift;
	my $declaration = shift;
	my $methodType = "instm";
	
	if ($declaration =~ /^\s*static/) {
	    $methodType = "clm";
	}
	return $methodType;
}

sub getParamSignature {
    my $self = shift;
	my $declaration = shift;
	my $sig;
	my @params;
	
	$declaration =~ s/^[^(]+\(([^)]*)\).*/$1/;
	@params = split (/,/, $declaration);
	foreach my $paramString (@params) {
	    my @paramElements = split (/\s+/, $paramString);
	    my $lastElement = pop @paramElements;
	    $sig .= join ("", @paramElements);
	    if ($lastElement =~ /^\*.*/) {$sig .= "*";};  #if the arg was a pointer
	}
	return $sig;
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

sub byLinkage { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   return ($obj1->linkageState() cmp $obj2->linkageState());
}

sub byAccessControl { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   return ($obj1->accessControl() cmp $obj2->accessControl());
}

sub linkageAndObjName { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   my $linkAndName1 = $obj1->linkageState() . $obj1->name();
   my $linkAndName2 = $obj2->linkageState() . $obj2->name();
   return ($linkAndName1 cmp $linkAndName2);
}

##################### Debugging ####################################

sub printObject {
    my $self = shift;
 
    $self->SUPER::printObject();
    print "CPPClass\n";
    print "\n";
}

1;

