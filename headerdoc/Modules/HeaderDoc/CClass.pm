#! /usr/bin/perl -w
#
# Class name: CClass
# Synopsis: Holds comments pertaining to a C++ class, as parsed by HeaderDoc
# from a C++ header
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2003/08/27 23:55:51 $
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
package HeaderDoc::CClass;

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
    
    my $tocString;

    if ($self->outputformat eq "hdxml") {
 	$tocString = "XMLFIX<h3><a href=\"$contentFrameName\" target=\"doc\">Introduction</a></h3>\n";
    } elsif ($self->outputformat eq "html") {
 	$tocString = "<nobr>&nbsp;<a href=\"$contentFrameName\" target=\"doc\">Introduction</a>\n";
    } else {
	$tocString = "UNKNOWN OUTPUT FORMAT TYPE";
    }

    # output list of functions as TOC
    if (@funcs) {
        my @publics;
        my @protecteds;
        my @privates;
	    if ($self->outputformat eq "hdxml") {
	        $tocString .= "XMLFIX<br><h4>Member Functions</h4><hr>\n";
	    } elsif ($self->outputformat eq "html") {
	        $tocString .= "<br><h4>Member Functions</h4><hr>\n";
            } else {
	    }
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
	        if ($self->outputformat eq "hdxml") {
	            $tocString .= "XMLFIX<h5>Public</h5>\n";
	        } elsif ($self->outputformat eq "html") {
	            $tocString .= "<h5>Public</h5>\n";
		} else {
		}
		    foreach my $obj (sort objName @publics) {
	        	my $name = $obj->name();
	        	if ($self->outputformat eq "hdxml") {
	        	    $tocString .= "XMLFIX<nobr>&nbsp;<a href=\"Functions/Functions.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
		        } elsif ($self->outputformat eq "html") {
	        	    $tocString .= "<nobr>&nbsp;<a href=\"Functions/Functions.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
			} else {
			}
	        }
	    }
	    if (@protecteds) {
	        if ($self->outputformat eq "hdxml") {
	            $tocString .= "XMLFIX<h5>Protected</h5>\n";
	        } elsif ($self->outputformat eq "html") {
	            $tocString .= "<h5>Protected</h5>\n";
		} else {
		}
		    foreach my $obj (sort objName @protecteds) {
	        	my $name = $obj->name();
		        if ($self->outputformat eq "hdxml") {
	        	    $tocString .= "XMLFIX<nobr>&nbsp;<a href=\"Functions/Functions.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
		        } elsif ($self->outputformat eq "html") {
	        	    $tocString .= "<nobr>&nbsp;<a href=\"Functions/Functions.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
			} else {
			}
	        }
	    }
	    if (@privates) {
	        if ($self->outputformat eq "hdxml") {
	            $tocString .= "XMLFIX<h5>Private</h5>\n";
	        } elsif ($self->outputformat eq "html") {
	            $tocString .= "<h5>Private</h5>\n";
		} else {
		}
		    foreach my $obj (sort objName @privates) {
	        	my $name = $obj->name();
	        	if ($self->outputformat eq "hdxml") {
	        	    $tocString .= "XMLFIX<nobr>&nbsp;<a href=\"Functions/Functions.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
		        } elsif ($self->outputformat eq "html") {
	        	    $tocString .= "<nobr>&nbsp;<a href=\"Functions/Functions.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
			} else {
			}
	        }
	    }
    }
    if (@typedefs) {
       	    if ($self->outputformat eq "hdxml") {
	        $tocString .= "XMLFIX<h4>Defined Types</h4>\n";
	    } elsif ($self->outputformat eq "html") {
	        $tocString .= "<h4>Defined Types</h4>\n";
	    } else {
	    }
	    foreach my $obj (sort objName @typedefs) {
	        my $name = $obj->name();
       	        if ($self->outputformat eq "hdxml") {
	            $tocString .= "XMLFIX<nobr>&nbsp;<a href=\"DataTypes/DataTypes.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	        } elsif ($self->outputformat eq "html") {
	            $tocString .= "<nobr>&nbsp;<a href=\"DataTypes/DataTypes.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
		} else {
		}
	    }
    }
    if (@structs) {
	    $tocString .= "<h4>Structs</h4>\n";
	    foreach my $obj (sort objName @structs) {
	        my $name = $obj->name();
       	        if ($self->outputformat eq "hdxml") {
	            $tocString .= "XMLFIX<nobr>&nbsp;<a href=\"Structs/Structs.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	        } elsif ($self->outputformat eq "html") {
	            $tocString .= "<nobr>&nbsp;<a href=\"Structs/Structs.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
		} else {
		}
	    }
    }
    if (@constants) {
	    $tocString .= "<h4>Constants</h4>\n";
	    foreach my $obj (sort objName @constants) {
	        my $name = $obj->name();
       	        if ($self->outputformat eq "hdxml") {
	            $tocString .= "XMLFIX<nobr>&nbsp;<a href=\"Constants/Constants.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	        } elsif ($self->outputformat eq "html") {
	            $tocString .= "<nobr>&nbsp;<a href=\"Constants/Constants.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
		} else {
		}
	    }
	}
    if (@enums) {
       	    if ($self->outputformat eq "hdxml") {
	        $tocString .= "XMLFIX<h4>Enumerations</h4>\n";
	    } elsif ($self->outputformat eq "html") {
	        $tocString .= "<h4>Enumerations</h4>\n";
	    } else {
	    }
	    foreach my $obj (sort objName @enums) {
	        my $name = $obj->name();
       	        if ($self->outputformat eq "hdxml") {
	            $tocString .= "XMLFIX<nobr>&nbsp;<a href=\"Enums/Enums.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	        } elsif ($self->outputformat eq "html") {
	            $tocString .= "<nobr>&nbsp;<a href=\"Enums/Enums.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
		} else {
		}
	    }
	}
    if (@vars) {
        my @publics;
        my @protecteds;
        my @privates;

       	    if ($self->outputformat eq "hdxml") {
	        $tocString .= "XMLFIX<br><h4>Member Data</h4><hr>\n";
	    } elsif ($self->outputformat eq "html") {
	        $tocString .= "<br><h4>Member Data</h4><hr>\n";
	    } else {
	    }
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
	        	$tocString .= "<nobr>&nbsp;<a href=\"Vars/Vars.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	        }
	    }
	    if (@protecteds) {
	        $tocString .= "<h5>Protected</h5>\n";
		    foreach my $obj (sort objName @protecteds) {
	        	my $name = $obj->name();
	        	$tocString .= "<nobr>&nbsp;<a href=\"Vars/Vars.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	        }
	    }
	    if (@privates) {
	        $tocString .= "<h5>Private</h5>\n";
		    foreach my $obj (sort objName @privates) {
	        	my $name = $obj->name();
	        	$tocString .= "<nobr>&nbsp;<a href=\"Vars/Vars.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	        }
	    }
	}
	$tocString .= "<br><h4>Other Reference</h4><hr>\n";
	$tocString .= "<nobr>&nbsp;<a href=\"../../$defaultFrameName\" target=\"_top\">Header</a></nobr><br>\n";
    $tocString .= "<br><hr><a href=\"$compositePageName\" target=\"_blank\">[Printable HTML Page]</a>\n";
    
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
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getVarDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Member Data</h2>\n";
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getConstantDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Constants</h2>\n";
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getTypedefDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Typedefs</h2>\n";
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getStructDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Structs</h2>\n";
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    $contentString= $self->_getEnumDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Enumerations</h2>\n";
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }

    $contentString= $self->_getPDefineDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>#defines</h2>\n";
		$contentString = $self->stripAppleRefs($contentString);
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
	my $throws = $obj->throws();
        my $abstract = $obj->abstract();
        my $availability = $obj->availability();
        my $updated = $obj->updated();
        my $declaration = $obj->declarationInHTML();
        my $declarationRaw = $obj->declaration();
        my $accessControl = $obj->accessControl();
        my @params = $obj->taggedParameters();
        my $result = $obj->result();

	$contentString .= "<hr>";
	# if ($declaration !~ /#define/) { # not sure how to handle apple_refs with macros yet
	        my $paramSignature = $self->getParamSignature($declarationRaw);
	        my $methodType = $self->getMethodType($declarationRaw);
        	my $uid = "//$apiUIDPrefix/cpp/$methodType/$className/$name/$paramSignature";
		HeaderDoc::APIOwner->register_uid($uid);
        	$contentString .= "<a name=\"$uid\"></a>\n";
        # }
	$contentString .= "<table border=\"0\"  cellpadding=\"2\" cellspacing=\"2\" width=\"300\">";
	$contentString .= "<tr>";
	$contentString .= "<td valign=\"top\" height=\"12\" colspan=\"5\">";
	$contentString .= "<h2><a name=\"$name\">$name</a></h2>\n";
	$contentString .= "</td>";
	$contentString .= "</tr></table>";
	$contentString .= "<hr>";

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
	                $paramContentString .= "<dt><tt><em>$pName</em></tt></dt><dd>$pDesc</dd>\n";
	            }
	        }
	        if (length ($paramContentString)){
		        $contentString .= "<h5><font face=\"Lucida Grande,Helvetica,Arial\">Parameter Descriptions</font></h5>\n";
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
    return $contentString;
}

sub XMLdocumentationBlock {
    my $self = shift;
    my $compositePageString = "";
    my $name = $self->name();
    my $availability = $self->availability();
    my $updated = $self->updated();
    my $abstract = $self->abstract();
    my $discussion = $self->discussion();
    my $contentString;
    
    $compositePageString .= "<class type=\"C++\">";

    if (length($name)) {
	$compositePageString .= "<name>$name</name>\n";
    }

    if (length($abstract)) {
	$compositePageString .= "<abstract>$abstract</abstract>\n";
    }
    if (length($availability)) {
	$contentString .= "<b>Availabilty:</b> $availability\n";
    }
    if (length($updated)) {
	$contentString .= "<b>Updated:</b> $updated\n";
    }
    if (length($discussion)) {
	$compositePageString .= "<discussion>$discussion</discussion>\n";
    }

    $contentString= $self->_getFunctionXMLDetailString();
    if (length($contentString)) {
	$contentString = $self->stripAppleRefs($contentString);
	$compositePageString .= "<functions>$contentString</functions>\n";
    }

    $contentString= $self->_getMethodXMLDetailString();
    if (length($contentString)) {
	$contentString = $self->stripAppleRefs($contentString);
	$compositePageString .= "<methods>$contentString</methods>\n";
    }
    
    $contentString= $self->_getVarXMLDetailString();
    if (length($contentString)) {
	$contentString = $self->stripAppleRefs($contentString);
	$compositePageString .= "<globals>$contentString</globals>\n";
    }
    
    $contentString= $self->_getConstantXMLDetailString();
    if (length($contentString)) {
	$contentString = $self->stripAppleRefs($contentString);
	$compositePageString .= "<constants>$contentString</constants>\n";
    }
    
    $contentString= $self->_getTypedefXMLDetailString();
    if (length($contentString)) {
	$contentString = $self->stripAppleRefs($contentString);
	$compositePageString .= "<typedefs>$contentString</typedefs>";
    }
    
    $contentString= $self->_getStructXMLDetailString();
    if (length($contentString)) {
	$contentString = $self->stripAppleRefs($contentString);
	$compositePageString .= "<structs>$contentString</structs>";
    }
    
    $contentString= $self->_getEnumXMLDetailString();
    if (length($contentString)) {
	$contentString = $self->stripAppleRefs($contentString);
	$compositePageString .= "<enums>$contentString</enums>";
    }

    $contentString= $self->_getPDefineXMLDetailString();
    if (length($contentString)) {
	$contentString = $self->stripAppleRefs($contentString);
	$compositePageString .= "<defines>$contentString</defines>";
    }  

    $compositePageString .= "</class>";
    return $compositePageString;
}

sub _getVarDetailString {
    my $self = shift;
    my @varObjs = $self->vars();
    my $contentString;

    foreach my $obj (sort objName @varObjs) {
        my $name = $obj->name();
	my $abstract = $obj->abstract();
	my $availability = $obj->availability();
	my $updated = $obj->updated();
        my $desc = $obj->discussion();
        my $declaration = $obj->declarationInHTML();
        my $accessControl = $obj->accessControl();
        my @fields = ();
        my $fieldHeading;
        if ($obj->can('fields')) { # for Structs and Typedefs
            @fields = $obj->fields();
            $fieldHeading = "Field Descriptions";
        } elsif ($obj->can('constants')) { # for enums
            @fields = $obj->constants();
            $fieldHeading = "Constants";
        }
        if ($obj->can('isFunctionPointer')) {
        	if ($obj->isFunctionPointer()) {
            	$fieldHeading = "Parameter Descriptions";
        	}
        }

	my $methodType = "var"; # $self->getMethodType($declarationRaw);
	my $apiUIDPrefix = HeaderDoc::APIOwner->apiUIDPrefix();
	my $headerObject = HeaderDoc::APIOwner->headerObject();
	my $className = (HeaderDoc::APIOwner->headerObject())->name();
	$contentString .= "<hr>";
	my $uid = "//$apiUIDPrefix/c/$methodType/$className/$name";
	HeaderDoc::APIOwner->register_uid($uid);
	$contentString .= "<a name=\"$uid\"></a>\n";
        
	$contentString .= "<table border=\"0\"  cellpadding=\"2\" cellspacing=\"2\" width=\"300\">";
	$contentString .= "<tr>";
	$contentString .= "<td valign=\"top\" height=\"12\" colspan=\"5\">";
	$contentString .= "<h2><a name=\"$name\">$name</a></h2>\n";
	$contentString .= "</td>";
	$contentString .= "</tr></table>";
	$contentString .= "<hr>";
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
	        $contentString .= "<table border=\"1\"  width=\"90%\">\n";
	        $contentString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>\n";
		# Not updating this table into a definition list because
		# this code path can never be called for valid C code.
	        foreach my $element (@fields) {
	            my $fName = $element->name();
	            my $fDesc = $element->discussion();
	            $contentString .= "<tr><td align=\"center\"><tt>$fName</tt></td><td>$fDesc</td></tr>\n";
	        }
	        $contentString .= "</table>\n</blockquote>\n";
	    }
	    # if (length($updated)) {
		# $contentString .= "<b>Availability:</b> $availability\n";
	    # }
	    # if (length($updated)) {
		# $contentString .= "<b>Updated:</b> $updated\n";
	    # }
	    # $contentString .= "<hr>\n";
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

# we add the apple_ref markup to the navigator comment to identify
# to Project Builder and other applications indexing the documentation
# that this is the entry point for documentation for this class
sub docNavigatorComment {
    my $self = shift;
    my $name = $self->name();
    my $navComment = "<!-- headerDoc=cl; name=$name-->";
    my $appleRef = "<a name=\"//apple_ref/cpp/cl/$name\"></a>";
    
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

sub byLinkage { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   # if ($HeaderDoc::sort_entries) {
        return ($obj1->linkageState() cmp $obj2->linkageState());
   # } else {
        # return (1 cmp 2);
   # }
}

sub byAccessControl { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   # if ($HeaderDoc::sort_entries) {
        return ($obj1->accessControl() cmp $obj2->accessControl());
   # } else {
        # return (1 cmp 2);
   # }
}

sub linkageAndObjName { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   my $linkAndName1 = $obj1->linkageState() . $obj1->name();
   my $linkAndName2 = $obj2->linkageState() . $obj2->name();
   if ($HeaderDoc::sort_entries) {
        return ($linkAndName1 cmp $linkAndName2);
   } else {
        return byLinkage($obj1, $obj2); # (1 cmp 2);
   }
}

##################### Debugging ####################################

sub printObject {
    my $self = shift;
 
    $self->SUPER::printObject();
    print "CClass\n";
    print "\n";
}

1;

