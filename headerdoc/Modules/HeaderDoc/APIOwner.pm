#! /usr/bin/perl -w
#
# Class name: APIOwner
# Synopsis: Abstract superclass for Header and OO structures
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2003/08/12 01:25:21 $
# 
# Method additions by SKoT McDonald <skot@tomandandy.com> Aug 2001 
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
package HeaderDoc::APIOwner;

BEGIN {
	foreach (qw(Mac::Files)) {
	    $MOD_AVAIL{$_} = eval "use $_; 1";
    }
}
use HeaderDoc::HeaderElement;
use HeaderDoc::DBLookup;
use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash);

use strict;
use vars qw($VERSION @ISA);
$VERSION = '1.20';

# Inheritance
@ISA = qw(HeaderDoc::HeaderElement);
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
my $theTime = time();
my ($sec, $min, $hour, $dom, $moy, $year, @rest);
($sec, $min, $hour, $dom, $moy, $year, @rest) = localtime($theTime);
$moy++;
$year += 1900;
my $dateStamp = "$moy/$dom/$year";
######################################################################

my @uid_list = ();


# class variables and accessors
{
    my $_copyrightOwner;
    my $_defaultFrameName;
    my $_compositePageName;
    my $_htmlHeader;
    my $_apiUIDPrefix;
    my $_headerObject;
    
    sub copyrightOwner {    
        my $class = shift;
        if (@_) {
            $_copyrightOwner = shift;
        }
        return $_copyrightOwner;
    }

    sub defaultFrameName {    
        my $class = shift;
        if (@_) {
            $_defaultFrameName = shift;
        }
        return $_defaultFrameName;
    }

    sub compositePageName {    
        my $class = shift;
        if (@_) {
            $_compositePageName = shift;
        }
        return $_compositePageName;
    }

    sub htmlHeader {
        my $class = shift;
        if (@_) {
            $_htmlHeader = shift;
        }
        return $_htmlHeader;
    }

    sub apiUIDPrefix {    
        my $class = shift;
        if (@_) {
            $_apiUIDPrefix = shift;
        }
        return $_apiUIDPrefix;
    }

    sub headerObject {
	my $class = shift;

	if (@_) {
            $_headerObject = shift;
	}
	return $_headerObject;
    }
}


sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
    bless($self, $class);
    $self->_initialize();
    return($self);
}

sub _initialize {
    my($self) = shift;

    $self->SUPER::_initialize();
    
    $self->{OUTPUTDIR} = undef;
    $self->{CONSTANTS} = ();
    $self->{FUNCTIONS} = ();
    $self->{METHODS} = ();
    $self->{TYPEDEFS} = ();
    $self->{STRUCTS} = ();
    $self->{VARS} = ();
    $self->{PDEFINES} = ();
    $self->{ENUMS} = ();
    $self->{CONSTANTSDIR} = undef;
    $self->{DATATYPESDIR} = undef;
    $self->{STRUCTSDIR} = undef;
    $self->{VARSDIR} = undef;
    $self->{FUNCTIONSDIR} = undef;
    $self->{METHODSDIR} = undef;
    $self->{PDEFINESDIR} = undef;
    $self->{ENUMSDIR} = undef;
    $self->{EXPORTSDIR} = undef;
    $self->{EXPORTINGFORDB} = 0;
    $self->{TOCTITLEPREFIX} = 'GENERIC_OWNER:';
    $self->{HEADEROBJECT} = undef;
    $self->{NAMESPACE} = "";
    $self->{UPDATED} = "";
} 

sub outputDir {
    my $self = shift;

    if (@_) {
        my $rootOutputDir = shift;
		if (-e $rootOutputDir) {
			if (! -d $rootOutputDir) {
			    die "Error: $rootOutputDir is not a directory. Exiting.\n\t$!\n";
			} elsif (! -w $rootOutputDir) {
			    die "Error: Output directory $rootOutputDir is not writable. Exiting.\n$!\n";
			}
		} else {
			unless (mkdir ("$rootOutputDir", 0777)) {
			    die ("Error: Can't create output folder $rootOutputDir.\n$!\n");
			}
	    }
        $self->{OUTPUTDIR} = $rootOutputDir;
	    $self->constantsDir("$rootOutputDir$pathSeparator"."Constants");
	    $self->datatypesDir("$rootOutputDir$pathSeparator"."DataTypes");
	    $self->structsDir("$rootOutputDir$pathSeparator"."Structs");
	    $self->functionsDir("$rootOutputDir$pathSeparator"."Functions");
	    $self->methodsDir("$rootOutputDir$pathSeparator"."Methods");
	    $self->varsDir("$rootOutputDir$pathSeparator"."Vars");
	    $self->pDefinesDir("$rootOutputDir$pathSeparator"."PDefines");
	    $self->enumsDir("$rootOutputDir$pathSeparator"."Enums");
	    $self->exportsDir("$rootOutputDir$pathSeparator"."Exports");
    }
    return $self->{OUTPUTDIR};
}

# /*! @function make_classref
#     @abstract This function turns a classname into a pseudo-link
#  */
sub make_classref
{
    my $self = shift;
    my $classname = shift;
    my $apiUIDPrefix = $self->apiUIDPrefix();
    my $localDebug = 0;
    my $retval = "";

    # Not yet implemented
    # my $lang = $self->lang();

    my $lang = "c";
    my $class = ref($self) || $self;

    if ($class =~ /^HeaderDoc::CPPClass$/) {
	$lang = "cpp";
    } elsif ($class =~ /^HeaderDoc::ObjC/) {
	$lang = "occ";
    }

    $retval = "//$apiUIDPrefix/$lang/cl/$classname";

    print "make_classref: ref is $retval\n" if ($localDebug);;

    return $retval;
}



sub tocTitlePrefix {
    my $self = shift;

    if (@_) {
        $self->{TOCTITLEPREFIX} = shift;
    }
    return $self->{TOCTITLEPREFIX};
}

sub exportingForDB {
    my $self = shift;

    if (@_) {
        $self->{EXPORTINGFORDB} = shift;
    }
    return $self->{EXPORTINGFORDB};
}

sub exportsDir {
    my $self = shift;

    if (@_) {
        $self->{EXPORTSDIR} = shift;
    }
    return $self->{EXPORTSDIR};
}

sub constantsDir {
    my $self = shift;

    if (@_) {
        $self->{CONSTANTSDIR} = shift;
    }
    return $self->{CONSTANTSDIR};
}


sub datatypesDir {
    my $self = shift;

    if (@_) {
        $self->{DATATYPESDIR} = shift;
    }
    return $self->{DATATYPESDIR};
}

sub structsDir {
    my $self = shift;

    if (@_) {
        $self->{STRUCTSDIR} = shift;
    }
    return $self->{STRUCTSDIR};
}

sub varsDir {
    my $self = shift;

    if (@_) {
        $self->{VARSDIR} = shift;
    }
    return $self->{VARSDIR};
}

sub pDefinesDir {
    my $self = shift;

    if (@_) {
        $self->{PDEFINESDIR} = shift;
    }
    return $self->{PDEFINESDIR};
}

sub enumsDir {
    my $self = shift;

    if (@_) {
        $self->{ENUMSDIR} = shift;
    }
    return $self->{ENUMSDIR};
}


sub functionsDir {
    my $self = shift;

    if (@_) {
        $self->{FUNCTIONSDIR} = shift;
    }
    return $self->{FUNCTIONSDIR};
}

sub methodsDir {
    my $self = shift;

    if (@_) {
        $self->{METHODSDIR} = shift;
    }
    return $self->{METHODSDIR};
}

sub tocString {
    my $self = shift;
    my $contentFrameName = $self->filename();
    $contentFrameName =~ s/(.*)\.h/$1/; 
    $contentFrameName = &safeName(filename => $contentFrameName);  
    $contentFrameName = $contentFrameName . ".html";
    
    my @funcs = $self->functions();
    my @methods = $self->methods();
    my @constants = $self->constants();
    my @typedefs = $self->typedefs();
    my @structs = $self->structs();
    my @enums = $self->enums();
    my @pDefines = $self->pDefines();
    my $tocString = "<nobr>&nbsp;<a href=\"$contentFrameName\" target=\"doc\">Introduction</a>\n";

    # output list of functions as TOC
    if (@funcs) {
	    my @groups = ("");
	    my $localDebug = 0;
	    my $lastgroup = "";
	    foreach my $obj (sort objGroup @funcs) {
		my $group = $obj->group;
		if (!($group eq $lastgroup)) {
			push (@groups, $group);
			if ($localDebug) {
			    print "Added $group\n";
			    print "List is:";
			    foreach my $printgroup (@groups) {
				print " $printgroup";
			    }
			    print "\n";
			}
			$lastgroup = $group;
		}
	    }
	    $tocString .= "<h4>Functions</h4>\n";

	    my $done_one = 0;
	    foreach my $group (sort @groups) {
		print "Sorting group $group\n" if ($localDebug);
		if (!($group eq "")) {
			if ($done_one) { $tocString .= "&nbsp;<br>" }
			$tocString .= "<i>$group</i><br>&nbsp;<br>";
		}

		foreach my $obj (sort objName @funcs) {
		    if ($obj->group() eq $group) {
			$done_one = 1;
	        	my $name = $obj->name();
	        	$tocString .= "<nobr>&nbsp;<a href=\"Functions/Functions.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
		    }
		}
	    }
	    # }
    }
    if (@methods) {
	    $tocString .= "<h4>Methods</h4>\n";
	    foreach my $obj (sort objName @methods) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href=\"Methods/Methods.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	    }
    }
    if (@typedefs) {
	    $tocString .= "<h4>Defined Types</h4>\n";
	    foreach my $obj (sort objName @typedefs) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href=\"DataTypes/DataTypes.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	    }
    }
    if (@structs) {
	    $tocString .= "<h4>Structs</h4>\n";
	    foreach my $obj (sort objName @structs) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href=\"Structs/Structs.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	    }
    }
    if (@constants) {
	    $tocString .= "<h4>Constants</h4>\n";
	    foreach my $obj (sort objName @constants) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href=\"Constants/Constants.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	    }
	}
    if (@enums) {
	    $tocString .= "<h4>Enumerations</h4>\n";
	    foreach my $obj (sort objName @enums) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href=\"Enums/Enums.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	    }
	}
    if (@pDefines) {
	    $tocString .= "<h4>#defines</h4>\n";
	    foreach my $obj (sort objName @pDefines) {
	        my $name = $obj->name();
	        $tocString .= "<nobr>&nbsp;<a href=\"PDefines/PDefines.html#$name\" target=\"doc\">$name</a></nobr><br>\n";
	    }
	}
    return $tocString;
}

sub enums {
    my $self = shift;

    if (@_) {
        @{ $self->{ENUMS} } = @_;
    }
    ($self->{ENUMS}) ? return @{ $self->{ENUMS} } : return ();
}

sub addToEnums {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{ENUMS} }, $item);
        }
    }
    return @{ $self->{ENUMS} };
}

sub pDefines {
    my $self = shift;

    if (@_) {
        @{ $self->{PDEFINES} } = @_;
    }
    ($self->{PDEFINES}) ? return @{ $self->{PDEFINES} } : return ();
}

sub addToPDefines {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{PDEFINES} }, $item);
        }
    }
    return @{ $self->{PDEFINES} };
}

sub constants {
    my $self = shift;

    if (@_) {
        @{ $self->{CONSTANTS} } = @_;
    }
    ($self->{CONSTANTS}) ? return @{ $self->{CONSTANTS} } : return ();
}

sub addToConstants {
    my $self = shift;
    
    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{CONSTANTS} }, $item);
        }
    }
    return @{ $self->{CONSTANTS} };
}


sub functions {
    my $self = shift;

    if (@_) {
        @{ $self->{FUNCTIONS} } = @_;
    }
    ($self->{FUNCTIONS}) ? return @{ $self->{FUNCTIONS} } : return ();
}

sub addToFunctions {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{FUNCTIONS} }, $item);
        }
    }
    return @{ $self->{FUNCTIONS} };
}

sub methods {
    my $self = shift;

    if (@_) {
        @{ $self->{METHODS} } = @_;
    }
    ($self->{METHODS}) ? return @{ $self->{METHODS} } : return ();
}

sub addToMethods {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{METHODS} }, $item);
        }
    }
    return @{ $self->{METHODS} };
}

sub typedefs {
    my $self = shift;

    if (@_) {
        @{ $self->{TYPEDEFS} } = @_;
    }
    ($self->{TYPEDEFS}) ? return @{ $self->{TYPEDEFS} } : return ();
}

sub addToTypedefs {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{TYPEDEFS} }, $item);
        }
    }
    return @{ $self->{TYPEDEFS} };
}

sub structs {
    my $self = shift;

    if (@_) {
        @{ $self->{STRUCTS} } = @_;
    }
    ($self->{STRUCTS}) ? return @{ $self->{STRUCTS} } : return ();
}

sub addToStructs {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{STRUCTS} }, $item);
        }
    }
    return @{ $self->{STRUCTS} };
}

sub vars {
    my $self = shift;

    if (@_) {
        @{ $self->{VARS} } = @_;
    }
    ($self->{VARS}) ? return @{ $self->{VARS} } : return ();
}

sub addToVars {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{VARS} }, $item);
        }
    }
    return @{ $self->{VARS} };
}

sub fields {
    my $self = shift;
    if (@_) { 
        @{ $self->{FIELDS} } = @_;
    }
    ($self->{FIELDS}) ? return @{ $self->{FIELDS} } : return ();
}

sub addToFields {
    my $self = shift;
    if (@_) { 
        push (@{$self->{FIELDS}}, @_);
    }
    return @{ $self->{FIELDS} };
}

sub namespace {
    my $self = shift;
    my $localDebug = 0;

    if (@_) { 
        $self->{NAMESPACE} = shift;
    }
    print "namespace ".$self->{NAMESPACE}."\n" if ($localDebug);
    return $self->{NAMESPACE};
}

sub availability {
    my $self = shift;

    if (@_) {
        $self->{AVAILABILITY} = shift;
    }
    return $self->{AVAILABILITY};
}

sub updated {
    my $self = shift;
    my $localDebug = 0;
    
    if (@_) {
	my $updated = shift;
        # $self->{UPDATED} = shift;
	my $month; my $day; my $year;

	$month = $day = $year = $updated;

	print "updated is $updated\n" if ($localDebug);
	if (!($updated =~ /\d\d\d\d-\d\d-\d\d/ )) {
	    if (!($updated =~ /\d\d-\d\d-\d\d\d\d/ )) {
		if (!($updated =~ /\d\d-\d\d-\d\d/ )) {
		    my $filename = $HeaderDoc::headerObject->filename();
		    print "$filename:0:Bogus date format: $updated.\n";
		    print "Valid formats are MM-DD-YYYY, MM-DD-YY, and YYYY-MM-DD\n";
		    return $self->{UPDATED};
		} else {
		    $month =~ s/(\d\d)-\d\d-\d\d/$1/smg;
		    $day =~ s/\d\d-(\d\d)-\d\d/$1/smg;
		    $year =~ s/\d\d-\d\d-(\d\d)/$1/smg;

		    my $century;
		    $century = `date +%C`;
		    $century *= 100;
		    $year += $century;
		    # $year += 2000;
		    print "YEAR: $year" if ($localDebug);
		}
	    } else {
		print "03-25-2003 case.\n" if ($localDebug);
		    $month =~ s/(\d\d)-\d\d-\d\d\d\d/$1/smg;
		    $day =~ s/\d\d-(\d\d)-\d\d\d\d/$1/smg;
		    $year =~ s/\d\d-\d\d-(\d\d\d\d)/$1/smg;
	    }
	} else {
		    $year =~ s/(\d\d\d\d)-\d\d-\d\d/$1/smg;
		    $month =~ s/\d\d\d\d-(\d\d)-\d\d/$1/smg;
		    $day =~ s/\d\d\d\d-\d\d-(\d\d)/$1/smg;
	}
	$month =~ s/\n*//smg;
	$day =~ s/\n*//smg;
	$year =~ s/\n*//smg;
	$month =~ s/\s*//smg;
	$day =~ s/\s*//smg;
	$year =~ s/\s*//smg;

	# Check the validity of the modification date

	my $invalid = 0;
	my $mdays = 28;
	if ($month == 2) {
		if ($year % 4) {
			$mdays = 28;
		} elsif ($year % 100) {
			$mdays = 29;
		} elsif ($year % 400) {
			$mdays = 28;
		} else {
			$mdays = 29;
		}
	} else {
		my $bitcheck = (($month & 1) ^ (($month & 8) >> 3));
		if ($bitcheck) {
			$mdays = 31;
		} else {
			$mdays = 30;
		}
	}

	if ($month > 12 || $month < 1) { $invalid = 1; }
	if ($day > $mdays || $day < 1) { $invalid = 1; }
	if ($year < 1970) { $invalid = 1; }

	if ($invalid) {
		my $filename = $HeaderDoc::headerObject->filename();
		print "$filename:0:Invalid date (year = $year, month = $month, day = $day).\n";
		print "$filename:0:Valid formats are MM-DD-YYYY, MM-DD-YY, and YYYY-MM-DD\n";
		return $self->{UPDATED};
	} else {
		$self->{UPDATED} = "$year-$month-$day";
		print "date set to ".$self->{UPDATED}."\n" if ($localDebug);
	}
    }
    return $self->{UPDATED};
}


##################################################################

sub createMetaFile {
    my $self = shift;
    my $outDir = $self->outputDir();
    my $outputFile = "$outDir$pathSeparator"."book.xml";
    my $text = $self->metaFileText();

    open(OUTFILE, ">$outputFile") || die "Can't write $outputFile. \n$!\n";
    if ($isMacOS) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};
    print OUTFILE "$text";
    close OUTFILE;
}

sub createFramesetFile {
    my $self = shift;
    my $docNavigatorComment = $self->docNavigatorComment();
    my $class = ref($self);
    my $defaultFrameName = $class->defaultFrameName();

    my $filename = $self->filename();
    my $name = $self->name();
    my $title = $filename;
    if (!length($name)) {
	$name = "$filename";
    } else {
	$title = "$name ($filename)";
    }

    my $outDir = $self->outputDir();
    
    my $outputFile = "$outDir$pathSeparator$defaultFrameName";    
    my $rootFileName = $self->filename();
    $rootFileName =~ s/(.*)\.h/$1/; 
    $rootFileName = &safeName(filename => $rootFileName);

    open(OUTFILE, ">$outputFile") || die "Can't write $outputFile. \n$!\n";
    if ($isMacOS) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};
	print OUTFILE "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Frameset//EN\"\n    \"http://www.w3.org/TR/1999/REC-html401-19991224/frameset.dtd\">\n";
    print OUTFILE "<html><head>\n    <title>Documentation for $title</title>\n	<meta name=\"generator\" content=\"HeaderDoc\">\n</head>\n";
    print OUTFILE "<frameset cols=\"190,100%\">\n";
    print OUTFILE "<frame src=\"toc.html\" name=\"toc\">\n";
    print OUTFILE "<frame src=\"$rootFileName.html\" name=\"doc\">\n";
    print OUTFILE "</frameset></html>\n";
    print OUTFILE "$docNavigatorComment\n";
    close OUTFILE;
}

# Overridden by subclasses to return HTML comment that identifies the 
# index file (Header vs. Class, name, etc.). gatherHeaderDoc uses this
# information to create a master TOC of the generated doc.
#
sub docNavigatorComment {
    return "";
}

sub createTOCFile {
    my $self = shift;
    my $rootDir = $self->outputDir();
    my $tocTitlePrefix = $self->tocTitlePrefix();
    my $outputFileName = "toc.html";    
    my $outputFile = "$rootDir$pathSeparator$outputFileName";    
    my $fileString = $self->tocString();    

    my $filename = $self->filename();
    my $name = $self->name();
    my $title = $filename;
    if (!length($name)) {
	$name = "$filename";
    } else {
	$title = "$name ($filename)";
    }

	open(OUTFILE, ">$outputFile") || die "Can't write $outputFile.\n$!\n";
    if ($isMacOS) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};
	print OUTFILE "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n    \"http://www.w3.org/TR/1998/REC-html40-19980424/loose.dtd\">\n";
	print OUTFILE "<html>";
	print OUTFILE "<style type=\"text/css\">";
	print OUTFILE "<!--";
	print OUTFILE "a:link {text-decoration: none; font-family: Verdana, Geneva, Helvetica, Arial, sans-serif; font-size: small}";
	print OUTFILE "a:visited {text-decoration: none; font-family: Verdana, Geneva, Helvetica, Arial, sans-serif; font-size: small}";
	print OUTFILE "a:active {text-decoration: none; font-family: Verdana, Geneva, Helvetica, Arial, sans-serif; font-size: small}";
	print OUTFILE "a:hover {text-decoration: underline; font-family: Verdana, Geneva, Helvetica, Arial, sans-serif; font-size: small}";
	print OUTFILE "h4 {text-decoration: none; font-family: Verdana,Geneva,Arial,Helvetica,sans-serif; size: tiny; font-weight: bold}"; # bold
	print OUTFILE "-->";
	print OUTFILE "</style>";
	print OUTFILE "<head>\n    <title>Documentation for $title</title>\n	<meta name=\"generator\" content=\"HeaderDoc\">\n</head>\n";
	print OUTFILE "<body bgcolor=\"#cccccc\" link=\"#000099\" vlink=\"#660066\"\n";
	print OUTFILE "leftmargin=\"0\" topmargin=\"0\" marginwidth=\"1\"\n"; 
	print OUTFILE "marginheight=\"0\">\n";

	print OUTFILE "<table width=\"100%\">";
	print OUTFILE "<tr bgcolor=\"#999999\"><td>&nbsp;</td></tr>";
	print OUTFILE "</table><br>";

	print OUTFILE "<table border=\"0\" cellpadding=\"0\" cellspacing=\"2\" width=\"148\">\n";
	print OUTFILE "<tr><td colspan=\"2\"><font size=\"5\" color=\"#330066\"><b>$tocTitlePrefix</b></font></td></tr>\n";
	print OUTFILE "<tr><td width=\"15\"></td><td><b><font size=\"+1\">$filename</font></b></td></tr>\n";
	print OUTFILE "</table><p>&nbsp;<p>\n";
	print OUTFILE $fileString;
	print OUTFILE "</body></html>\n";
	close OUTFILE;
}

sub createContentFile {

    my $self = shift;
    my $class = ref($self);
    my $copyrightOwner = $class->copyrightOwner();
    my $filename = $self->filename();
    my $name = $self->name();
    my $title = $filename;
    if (!length($name)) {
	$name = "$filename";
    } else {
	$title = "$name ($filename)";
    }

    my $rootFileName = $self->filename();

    if ($class eq "HeaderDoc::Header") {
	my $headercopyright = $self->headerCopyrightOwner();
	if (!($headercopyright eq "")) {
	    $copyrightOwner = $headercopyright;
	}
    }

    my $HTMLmeta = "";
    if ($class eq "HeaderDoc::Header") {
	$HTMLmeta = $self->HTMLmeta();
    }

    my $fileString = "";

    $rootFileName =~ s/(.*)\.h/$1/; 
    # for now, always shorten long names since some files may be moved to a Mac for browsing
    if (1 || $isMacOS) {$rootFileName = &safeName(filename => $rootFileName);};
    my $outputFileName = "$rootFileName.html";    
    my $rootDir = $self->outputDir();
    my $outputFile = "$rootDir$pathSeparator$outputFileName";    
   	open (OUTFILE, ">$outputFile") || die "Can't write header-wide content page $outputFile. \n$!\n";
    if ($isMacOS) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};


    my $headerDiscussion = $self->discussion();    
    my $headerAbstract = $self->abstract();  
    if ((length($headerDiscussion)) || (length($headerAbstract))) {
		$fileString .= "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n    \"http://www.w3.org/TR/1998/REC-html40-19980424/loose.dtd\">\n";
		$fileString .= "<html><HEAD>\n    <title>API Documentation</title>\n	$HTMLmeta <meta name=\"generator\" content=\"HeaderDoc\">\n</HEAD>\n<BODY bgcolor=\"#ffffff\">\n";
		if ($HeaderDoc::insert_header) {
			$fileString .= "<!-- start of header -->\n";
			$fileString .= $self->htmlHeader()."\n";
			$fileString .= "<!-- end of header -->\n";
		}
		$fileString .= "<H1>$name</H1><hr>\n";
		if (length($headerAbstract)) {
		    # $fileString .= "<b>Abstract: </b>$headerAbstract<hr><br>\n";    
		    $fileString .= "$headerAbstract<br>\n";    
		}

		my $namespace = $self->namespace();
		my $availability = $self->availability();
		my $updated = $self->updated();
	 	if (length($updated) || length($namespace)) {
		    $fileString .= "<p></p>\n";
		}

		if (length($namespace)) {
		    $fileString .= "<b>Namespace:</b> $namespace<br>\n";
		}
		if (length($availability)) {      
		    $fileString .= "<b>Availability:</b> $availability<br>\n";
		}
		if (length($updated)) {      
		    $fileString .= "<b>Updated:</b> $updated<br>\n";
		}
                my $short_attributes = $self->getAttributes(0);
                my $long_attributes = $self->getAttributes(1);
                my $list_attributes = $self->getAttributeLists();
                if (length($short_attributes)) {
                        $fileString .= "$short_attributes";
                }
                if (length($list_attributes)) {
                        $fileString .= "$list_attributes";
                }
	 	if (length($updated) || length($availability) || length($namespace) || length($headerAbstract) || length($short_attributes) || length($list_attributes)) {
		    $fileString .= "<p></p>\n";
		    $fileString .= "<hr><br>\n";
		}

		$fileString .= "$headerDiscussion<br><br>\n";
                if (length($long_attributes)) {
                        $fileString .= "$long_attributes";
                }
    } else {
        # warn "No header-wide comment found. Creating dummy file for default content page.\n";
		$fileString .= "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n    \"http://www.w3.org/TR/1998/REC-html40-19980424/loose.dtd\">\n";
		$fileString .= "<html><HEAD>\n    <title>API Documentation</title>\n	$HTMLmeta <meta name=\"generator\" content=\"HeaderDoc\">\n</HEAD>\n<BODY bgcolor=\"#ffffff\">\n";
		if ($HeaderDoc::insert_header) {
			$fileString .= "<!-- start of header -->\n";
			$fileString .= $self->htmlHeader()."\n";
			$fileString .= "<!-- end of header -->\n";
		}

		$fileString .= "<H1>$name</H1>\n";
		$fileString .= "<hr>Use the links in the table of contents to the left to access documentation.<br>\n";    
    }
		my @fields = $self->fields();
		if (@fields) {
			$fileString .= "<hr><h5><font face=\"Lucida Grande,Helvetica,Arial\">Template Parameter Descriptions</font></h5>";
			# print "\nGOT fields.\n";
			# $fileString .= "<table width=\"90%\" border=1>";
			# $fileString .= "<thead><tr><th>Name</th><th>Description</th></tr></thead>";
			$fileString .= "<dl>";
			for my $field (@fields) {
				my $name = $field->name();
				my $desc = $field->discussion();
				# print "field $name $desc\n";
				# $fileString .= "<tr><td><tt>$name</tt></td><td>$desc</td></tr>";
				$fileString .= "<dt><tt>$name</tt></dt><dd>$desc</dd>";
			}
			# $fileString .= "</table>\n";
			$fileString .= "</dl>\n";
		}
	$fileString .= "<hr><br><center>";
	$fileString .= "&#169; $copyrightOwner " if (length($copyrightOwner));
	my $filedate = $self->updated();
	if (length($filedate)) {
	    $fileString .= "(Last Updated $filedate)\n";
	} else {
	    $fileString .= "(Last Updated $dateStamp)\n";
	}
	$fileString .= "<br>";
	$fileString .= "<font size=\"-1\">HTML documentation generated by <a href=\"http://www.opensource.apple.com/projects\" target=\"_blank\">HeaderDoc</a></font>\n";    
	$fileString .= "</center>\n";
	$fileString .= "</BODY>\n</HTML>\n";

	print OUTFILE toplevel_html_fixup_links($self, $fileString);

	close OUTFILE;
}

sub writeHeaderElements {
    my $self = shift;
    my $rootOutputDir = $self->outputDir();
    my $functionsDir = $self->functionsDir();
    my $methodsDir = $self->methodsDir();
    my $dataTypesDir = $self->datatypesDir();
    my $structsDir = $self->structsDir();
    my $constantsDir = $self->constantsDir();
    my $varsDir = $self->varsDir();
    my $enumsDir = $self->enumsDir();
    my $pDefinesDir = $self->pDefinesDir();

	if (! -e $rootOutputDir) {
		unless (mkdir ("$rootOutputDir", 0777)) {die ("Can't create output folder $rootOutputDir. \n$!");};
    }
    
    if ($self->functions()) {
		if (! -e $functionsDir) {
			unless (mkdir ("$functionsDir", 0777)) {die ("Can't create output folder $functionsDir. \n$!");};
	    }
	    $self->writeFunctions();
    }
    if ($self->methods()) {
		if (! -e $methodsDir) {
			unless (mkdir ("$methodsDir", 0777)) {die ("Can't create output folder $methodsDir. \n$!");};
	    }
	    $self->writeMethods();
    }
    
    if ($self->constants()) {
		if (! -e $constantsDir) {
			unless (mkdir ("$constantsDir", 0777)) {die ("Can't create output folder $constantsDir. \n$!");};
	    }
	    $self->writeConstants();
    }
    
    if ($self->typedefs()) {
		if (! -e $dataTypesDir) {
			unless (mkdir ("$dataTypesDir", 0777)) {die ("Can't create output folder $dataTypesDir. \n$!");};
	    }
	    $self->writeTypedefs();
    }
    
    if ($self->structs()) {
		if (! -e $structsDir) {
			unless (mkdir ("$structsDir", 0777)) {die ("Can't create output folder $structsDir. \n$!");};
	    }
	    $self->writeStructs();
    }
    if ($self->vars()) {
		if (! -e $varsDir) {
			unless (mkdir ("$varsDir", 0777)) {die ("Can't create output folder $varsDir. \n$!");};
	    }
	    $self->writeVars();
    }
    if ($self->enums()) {
		if (! -e $enumsDir) {
			unless (mkdir ("$enumsDir", 0777)) {die ("Can't create output folder $enumsDir. \n$!");};
	    }
	    $self->writeEnums();
    }

    if ($self->pDefines()) {
		if (! -e $pDefinesDir) {
			unless (mkdir ("$pDefinesDir", 0777)) {die ("Can't create output folder $pDefinesDir. \n$!");};
	    }
	    $self->writePDefines();
    }
}


sub writeHeaderElementsToXMLPage { # All API in a single XML page
    my $self = shift;
    my $class = ref($self);
    my $compositePageName = $self->filename();
    $compositePageName =~ s/\.(h|i)$//;
    $compositePageName .= ".xml";
    my $rootOutputDir = $self->outputDir();
    my $name = $self->name();
    my $XMLPageString = $self->_getXMLPageString();
    my $outputFile = $rootOutputDir.$pathSeparator.$compositePageName;
# print "cpn = $compositePageName\n";
    
	if (! -e $rootOutputDir) {
		unless (mkdir ("$rootOutputDir", 0777)) {die ("Can't create output folder $rootOutputDir. $!");};
    }
    $self->_createXMLOutputFile($outputFile, xml_fixup_links($self, $XMLPageString), "$name");
}

sub writeHeaderElementsToCompositePage { # All API in a single HTML page -- for printing
    my $self = shift;
    my $class = ref($self);
    my $compositePageName = $class->compositePageName();
    my $rootOutputDir = $self->outputDir();
    my $name = $self->name();
    my $compositePageString = $self->_getCompositePageString();
    my $outputFile = $rootOutputDir.$pathSeparator.$compositePageName;

	if (! -e $rootOutputDir) {
		unless (mkdir ("$rootOutputDir", 0777)) {die ("Can't create output folder $rootOutputDir. $!");};
    }
    my $processed_string = toplevel_html_fixup_links($self, $compositePageString);
    $self->_createHTMLOutputFile($outputFile, $processed_string, "$name");
}

sub _getXMLPageString {
    my $self = shift;
    my $name = $self->name();
    my $compositePageString;
    my $contentString;
    
    my $abstract = $self->XMLabstract();
    if (length($abstract)) {
	    $compositePageString .= "<abstract>";
	    $compositePageString .= $abstract;
	    $compositePageString .= "</abstract>\n";
    }


    my $discussion = $self->XMLdiscussion();
    if (length($discussion)) {
	    $compositePageString .= "<discussion>";
	    $compositePageString .= $discussion;
	    $compositePageString .= "</discussion>\n";
    }
    
    $contentString= $self->_getClassXMLDetailString();
    if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    $contentString= $self->_getCategoryXMLDetailString();
    if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    $contentString= $self->_getProtocolXMLDetailString();
    if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    $contentString= $self->_getFunctionXMLDetailString();
    if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= "<functions>";
	    $compositePageString .= $contentString;
	    $compositePageString .= "</functions>\n";
    }
    $contentString= $self->_getMethodXMLDetailString();
    if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= "<methods>";
	    $compositePageString .= $contentString;
	    $compositePageString .= "</methods>\n";
    }
    
    $contentString= $self->_getConstantXMLDetailString();
    if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= "<constants>";
	    $compositePageString .= $contentString;
	    $compositePageString .= "</constants>\n";
    }
    
    $contentString= $self->_getTypedefXMLDetailString();
    if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= "<typedefs>";
	    $compositePageString .= $contentString;
	    $compositePageString .= "</typedefs>\n";
    }
    
    $contentString= $self->_getStructXMLDetailString();
    if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= "<structs>";
	    $compositePageString .= $contentString;
	    $compositePageString .= "</structs>\n";
    }
    
    $contentString= $self->_getVarXMLDetailString();
    if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= "<globals>";
	    $compositePageString .= $contentString;
	    $compositePageString .= "</globals>\n";
    }
    
    $contentString = $self->_getEnumXMLDetailString();
    if (length($contentString)) {
            $compositePageString .= "<enums>";
            $compositePageString .= $contentString;
            $compositePageString .= "</enums>\n";
    }
    
    $contentString= $self->_getPDefineXMLDetailString();
    if (length($contentString)) {
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= "<defines>";
	    $compositePageString .= $contentString;
	    $compositePageString .= "</defines>\n";
    }
    # $compositePageString =~ s/^\<br\>\<br\>$//smg;
    # $compositePageString =~ s/\<br\>/<br\/>/smg;

    # global substitutions
    $compositePageString =~ s/\<h1\>//smig;
    $compositePageString =~ s/\<\/h1\>//smig;
    $compositePageString =~ s/\<h2\>//smig;
    $compositePageString =~ s/\<\/h2\>//smig;
    $compositePageString =~ s/\<h3\>//smig;
    $compositePageString =~ s/\<\/h3\>//smig;
    $compositePageString =~ s/\<hr\>//smig;
    $compositePageString =~ s/\<br\>//smig;
    $compositePageString =~ s/&lt;tt&gt;//smig;
    $compositePageString =~ s/&lt;\/tt&gt;//smig;
    $compositePageString =~ s/&lt;pre&gt;//smig;
    $compositePageString =~ s/&lt;\/pre&gt;//smig;
    $compositePageString =~ s/&nbsp;/ /smig;

    # note: in theory, the paragraph tag can be left open,
    # which could break XML parsers.  While this is common
    # in web pages, it doesn't seem to be common in
    # headerdoc comments, so ignoring it for now.

    # case standardize tags.
    $compositePageString =~ s/<ul>/<ul>/smig;
    $compositePageString =~ s/<\/ul>/<\/ul>/smig;
    $compositePageString =~ s/<ol>/<ol>/smig;
    $compositePageString =~ s/<\/ol>/<\/ol>/smig;
    $compositePageString =~ s/<li>/<li>/smig;
    $compositePageString =~ s/<\/li>/<\/li>/smig;
    $compositePageString =~ s/<b>/<b>/smig;
    $compositePageString =~ s/<\/b>/<\/b>/smig;
    $compositePageString =~ s/<i>/<i>/smig;
    $compositePageString =~ s/<\/i>/<\/i>/smig;

    my @compositearray = split(/<li>/i, $compositePageString);
    my $newstring = "";

    my $done_one = 0;
    foreach my $listelement (@compositearray) {
	# We depend on the fact that the page can't legally start with
	# an <li> tag.  :-)
	if (!($done_one)) {
	    $done_one = 1;
	    $newstring .= "$listelement";
# print "first\n";
	} else {
# print "not first\n";
	    if ($listelement =~ /<\/[uo]l>/i) {
		$done_one = 0;
		my $insert = 0;
		if ($listelement =~ /^<\/[uo]l>/i) {
			$done_one = 1; $insert = 1;
		}
		my @elementbits = split(/<\/[uo]l>/i, $listelement);
		my $newelement = "";
		foreach my $elementbit (@elementbits) {
		    if ($done_one) {
			if ($insert) {
			    $newelement .= "</li>";
			}
			if ($listelement =~ /<\/ul>/i) {
				$newelement .= "</ul>";
			} else {
				$newelement .= "</ol>";
			}
			$newelement .= "$elementbit";
		    } else {
			$done_one = 1;
			if (!($listelement =~ /<\/li>/i)) {
			    $insert = 1;
			}
			$newelement .= "$elementbit";
		    }
		}
		$done_one = 1;
		$listelement = $newelement;
	    } else  {
		if (!($listelement =~ /<\/li>/i)) {
		    $listelement .= "</li>";
		}
	    }
	    $newstring .= "<li>";
	    $newstring .= "$listelement";
	}
    }

    $compositePageString  = $newstring;
    
    return $compositePageString;
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
	    $compositePageString .= "<h2>Functions</h2>\n";
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    $contentString= $self->_getMethodDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Methods</h2>\n";
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
    
    $contentString= $self->_getVarDetailString();
    if (length($contentString)) {
	    $compositePageString .= "<h2>Globals</h2>\n";
		$contentString = $self->stripAppleRefs($contentString);
	    $compositePageString .= $contentString;
    }
    
    $contentString = $self->_getEnumDetailString();
    if (length($contentString)) {
            $compositePageString .= "<h2>Enumerations</h2>\n";
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

# apple_ref markup is a named anchor that uniquely identifies each API symbol.  For example, an Objective-C
# class named Foo would have the anchor <a name="//apple_ref/occ/cl/Foo"></a>.  This markup is already in the
# primary documentation pages, so we don't want duplicates in the composite pages, thus this method.  See
# the APIAnchors.html file in HeaderDoc's documentation to learn more about apple_ref markup.
sub stripAppleRefs {
    my $self = shift;
    my $string = shift;
	$string =~ s|<a\s+name\s*=\s*\"//apple_ref/[^"]+?\"><\s*/a\s*>||g;
	return $string;
}

sub writeFunctions {
    my $self = shift;
    my $functionFile = $self->functionsDir().$pathSeparator."Functions.html";
    $self->_createHTMLOutputFile($functionFile, $self->_getFunctionDetailString(), "Functions");
}

sub _getFunctionDetailString {
    my $self = shift;
    my @funcObjs = $self->functions();
    my $contentString = "";

    foreach my $obj (sort objName @funcObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub _getFunctionXMLDetailString {
    my $self = shift;
    my @funcObjs = $self->functions();
    my $contentString = "";

    foreach my $obj (sort objName @funcObjs) {
        my $documentationBlock = $obj->XMLdocumentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}


sub _getClassXMLDetailString {
    my $self = shift;
    my @classObjs = $self->classes();
    my $contentString = "";

    foreach my $obj (sort objName @classObjs) {
	# print "outputting class ".$obj->name.".";
	my $documentationBlock = $obj->XMLdocumentationBlock();
	$contentString .= $documentationBlock;
    }
    return $contentString;
}

sub _getCategoryXMLDetailString {
    my $self = shift;
    my @classObjs = $self->categories();
    my $contentString = "";

    foreach my $obj (sort objName @classObjs) {
	# print "outputting category ".$obj->name.".";
	my $documentationBlock = $obj->XMLdocumentationBlock();
	$contentString .= $documentationBlock;
    }
    return $contentString;
}

sub _getProtocolXMLDetailString {
    my $self = shift;
    my @classObjs = $self->protocols();
    my $contentString = "";

    foreach my $obj (sort objName @classObjs) {
	# print "outputting protocol ".$obj->name.".";
	my $documentationBlock = $obj->XMLdocumentationBlock();
	$contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeMethods {
    my $self = shift;
    my $methodFile = $self->methodsDir().$pathSeparator."Methods.html";
    $self->_createHTMLOutputFile($methodFile, $self->_getMethodDetailString(), "Methods");
}

sub _getMethodDetailString {
    my $self = shift;
    my @methObjs = $self->methods();
    my $contentString = "";
    my $localDebug = 0;

    foreach my $obj (sort objName @methObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub _getMethodXMLDetailString {
    my $self = shift;
    my @methObjs = $self->methods();
    my $contentString = "";

    foreach my $obj (sort objName @methObjs) {
        my $documentationBlock = $obj->XMLdocumentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeConstants {
    my $self = shift;
    my $constantsFile = $self->constantsDir().$pathSeparator."Constants.html";
    $self->_createHTMLOutputFile($constantsFile, $self->_getConstantDetailString(), "Constants");
}

sub _getConstantDetailString {
    my $self = shift;
    my @constantObjs = $self->constants();
    my $contentString;

    foreach my $obj (sort objName @constantObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub _getConstantXMLDetailString {
    my $self = shift;
    my @constantObjs = $self->constants();
    my $contentString;

    foreach my $obj (sort objName @constantObjs) {
        my $documentationBlock = $obj->XMLdocumentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeTypedefs {
    my $self = shift;
    my $typedefsFile = $self->datatypesDir().$pathSeparator."DataTypes.html";
    $self->_createHTMLOutputFile($typedefsFile, $self->_getTypedefDetailString(), "Defined Types");
}

sub _getTypedefDetailString {
    my $self = shift;
    my @typedefObjs = $self->typedefs();
    my $contentString;

    foreach my $obj (sort objName @typedefObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub _getTypedefXMLDetailString {
    my $self = shift;
    my @typedefObjs = $self->typedefs();
    my $contentString;

    foreach my $obj (sort objName @typedefObjs) {
        my $documentationBlock = $obj->XMLdocumentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeStructs {
    my $self = shift;
    my $structsFile = $self->structsDir().$pathSeparator."Structs.html";
    $self->_createHTMLOutputFile($structsFile, $self->_getStructDetailString(), "Structs");
}

sub _getStructDetailString {
    my $self = shift;
    my @structObjs = $self->structs();
    my $contentString;

    foreach my $obj (sort objName @structObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub _getStructXMLDetailString {
    my $self = shift;
    my @structObjs = $self->structs();
    my $contentString;

    foreach my $obj (sort objName @structObjs) {
        my $documentationBlock = $obj->XMLdocumentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeVars {
    my $self = shift;
    my $varsFile = $self->varsDir().$pathSeparator."Vars.html";
    $self->_createHTMLOutputFile($varsFile, $self->_getVarDetailString(), "Data Members");
}

sub _getVarDetailString {
    my $self = shift;
    my @varObjs = $self->vars();
    my $contentString;
    foreach my $obj (sort objName @varObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub _getVarXMLDetailString {
    my $self = shift;
    my @varObjs = $self->vars();
    my $contentString;

    foreach my $obj (sort objName @varObjs) {
        my $documentationBlock = $obj->XMLdocumentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writeEnums {
    my $self = shift;
    my $enumsFile = $self->enumsDir().$pathSeparator."Enums.html";
    $self->_createHTMLOutputFile($enumsFile, $self->_getEnumDetailString(), "Enumerations");
}

sub _getEnumDetailString {
    my $self = shift;
    my @enumObjs = $self->enums();
    my $contentString;

    foreach my $obj (sort objName @enumObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub _getEnumXMLDetailString {
    my $self = shift;
    my @enumObjs = $self->enums();
    my $contentString;

    foreach my $obj (sort objName @enumObjs) {
        my $documentationBlock = $obj->XMLdocumentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub writePDefines {
    my $self = shift;
    my $pDefinesFile = $self->pDefinesDir().$pathSeparator."PDefines.html";
    $self->_createHTMLOutputFile($pDefinesFile, $self->_getPDefineDetailString(), "#defines");
}

sub _getPDefineDetailString {
    my $self = shift;
    my @pDefineObjs = $self->pDefines();
    my $contentString;

    foreach my $obj (sort objName @pDefineObjs) {
        my $documentationBlock = $obj->documentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}

sub _getPDefineXMLDetailString {
    my $self = shift;
    my @pDefineObjs = $self->pDefines();
    my $contentString;

    foreach my $obj (sort objName @pDefineObjs) {
        my $documentationBlock = $obj->XMLdocumentationBlock();
        $contentString .= $documentationBlock;
    }
    return $contentString;
}


sub writeExportsWithName {
    my $self = shift;
    my $name = shift;
    my $exportsDir = $self->exportsDir();
    my $functionsFile = $exportsDir.$pathSeparator.$name.".ftab";
    my $methodsFile = $exportsDir.$pathSeparator.$name.".ftab";
    my $parametersFile = $exportsDir.$pathSeparator.$name.".ptab";
    my $structsFile = $exportsDir.$pathSeparator.$name.".stab";
    my $fieldsFile = $exportsDir.$pathSeparator.$name.".mtab";
    my $enumeratorsFile = $exportsDir.$pathSeparator.$name.".ktab";
    my $funcString;
    my $methString;
    my $paramString;
    my $dataTypeString;
    my $typesFieldString;
    my $enumeratorsString;
    
	if (! -e $exportsDir) {
		unless (mkdir ("$exportsDir", 0777)) {die ("Can't create output folder $exportsDir. $!");};
    }
    ($funcString, $paramString) = $self->_getFunctionsAndParamsExportString();
    ($methString, $paramString) = $self->_getMethodsAndParamsExportString();
    ($dataTypeString, $typesFieldString) = $self->_getDataTypesAndFieldsExportString();
    $enumeratorsString = $self->_getEnumeratorsExportString();
    
    $self->_createExportFile($functionsFile, $funcString);
    $self->_createExportFile($methodsFile, $methString);
    $self->_createExportFile($parametersFile, $paramString);
    $self->_createExportFile($structsFile, $dataTypeString);
    $self->_createExportFile($fieldsFile, $typesFieldString);
    $self->_createExportFile($enumeratorsFile, $enumeratorsString);
}

sub _getFunctionsAndParamsExportString {
    my $self = shift;
    my @funcObjs = $self->functions();
    my $tmpString = "";
    my @funcLines;
    my @paramLines;
    my $funcString;
    my $paramString;
    my $sep = "<tab>";       
    
    foreach my $obj (sort objName @funcObjs) {
        my $funcName = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
		my $declaration = $obj->declaration();
        my @taggedParams = $obj->taggedParameters();
        my @parsedParams = $obj->parsedParameters();
        my $result = $obj->result();
        my $funcID = HeaderDoc::DBLookup->functionIDForName($funcName);
        # unused fields--declaring them for visibility in the string below
        my $managerID = "";
        my $funcEnglishName = "";
        my $specialConsiderations = "";
        my $versionNotes = "";
        my $groupName = "";
        my $order = "";
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract, $declaration, $result) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $managerID.$sep.$funcID.$sep.$funcName.$sep.$funcEnglishName.$sep.$abstract.$sep.$desc.$sep.$result.$sep.$specialConsiderations.$sep.$versionNotes.$sep.$groupName.$sep.$order;
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@funcLines, "$tmpString");
        
        if (@taggedParams) {
            my %parsedParmNameToObjHash;
        	# make lookup hash of parsed params
        	foreach my $parsedParam (@parsedParams) {
        		$parsedParmNameToObjHash{$parsedParam->name()} = $parsedParam;
        	}
        	foreach my $taggedParam (@taggedParams) {
        	    my $tName = $taggedParam->name();
        	    my $pObj;
		        my $pos = "UNKNOWN_POSITION";
		        my $type = "UNKNOWN_TYPE";

		        if (exists $parsedParmNameToObjHash{$tName}) {
		            $pObj = $parsedParmNameToObjHash{$tName};
		        	$pos = $pObj->position();
		        	$type = $pObj->type();
		        } else {

				my $filename = $HeaderDoc::headerObject->name();
		        	print "$filename:0:---------------------------------------------------------------------------\n";
		        	warn "$filename:0:Tagged parameter '$tName' not found in declaration of function $funcName.\n";
		        	warn "$filename:0:Parsed declaration for $funcName is:\n$declaration\n";
		        	warn "$filename:0:Parsed params for $funcName are:\n";
		        	foreach my $pp (@parsedParams) {
		        	    my $n = $pp->name();
		        	    print "$filename:0:$n\n";
		        	}
		        	print "$filename:0:---------------------------------------------------------------------------\n";
		        }
	        	my $paramName = $taggedParam->name();
	        	my $disc = $taggedParam->discussion();
	     		$disc =~ s/\n<br><br>\n/\n\n/g;
	     		$disc =~ s/([^\n])\n([^\n])/$1 $2/g;
	        	my $tmpParamString = "";
	        	
	        	$tmpParamString = $funcID.$sep.$funcName.$sep.$pos.$sep.$disc.$sep.$sep.$sep.$paramName.$sep.$type;
        		$tmpParamString = &convertCharsForFileMaker($tmpParamString);
        		$tmpParamString =~ s/$sep/\t/g;
	        	push (@paramLines, "$tmpParamString");
        	}
        }
    }
    $funcString = join ("\n", @funcLines);
    $paramString = join ("\n", @paramLines);
    $funcString .= "\n";
    $paramString .= "\n";
    return ($funcString, $paramString);
}

sub _getMethodsAndParamsExportString {
    my $self = shift;
    my @methObjs = $self->methods();
    my $tmpString = "";
    my @methLines;
    my @paramLines;
    my $methString;
    my $paramString;
    my $sep = "<tab>";       
    
    foreach my $obj (sort objName @methObjs) {
        my $methName = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
		my $declaration = $obj->declaration();
        my @taggedParams = $obj->taggedParameters();
        my @parsedParams = $obj->parsedParameters();
        my $result = $obj->result();
        my $methID = HeaderDoc::DBLookup->methodIDForName($methName);
        # unused fields--declaring them for visibility in the string below
        my $managerID = "";
        my $methEnglishName = "";
        my $specialConsiderations = "";
        my $versionNotes = "";
        my $groupName = "";
        my $order = "";
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract, $declaration, $result) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $managerID.$sep.$methID.$sep.$methName.$sep.$methEnglishName.$sep.$abstract.$sep.$desc.$sep.$result.$sep.$specialConsiderations.$sep.$versionNotes.$sep.$groupName.$sep.$order;
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@methLines, "$tmpString");
        
        if (@taggedParams) {
            my %parsedParmNameToObjHash;
        	# make lookup hash of parsed params
        	foreach my $parsedParam (@parsedParams) {
        		$parsedParmNameToObjHash{$parsedParam->name()} = $parsedParam;
        	}
        	foreach my $taggedParam (@taggedParams) {
        	    my $tName = $taggedParam->name();
        	    my $pObj;
		        my $pos = "UNKNOWN_POSITION";
		        my $type = "UNKNOWN_TYPE";

		        if (exists $parsedParmNameToObjHash{$tName}) {
		            $pObj = $parsedParmNameToObjHash{$tName};
		        	$pos = $pObj->position();
		        	$type = $pObj->type();
		        } else {
				my $filename = $HeaderDoc::headerObject->name();
		        	print "$filename:0:---------------------------------------------------------------------------\n";
		        	warn "$filename:0:Tagged parameter '$tName' not found in declaration of method $methName.\n";
		        	warn "$filename:0:Parsed declaration for $methName is:\n$declaration\n";
		        	warn "$filename:0:Parsed params for $methName are:\n";
		        	foreach my $pp (@parsedParams) {
		        	    my $n = $pp->name();
		        	    print "$filename:0:$n\n";
		        	}
		        	print "$filename:0:---------------------------------------------------------------------------\n";
		        }
	        	my $paramName = $taggedParam->name();
	        	my $disc = $taggedParam->discussion();
	     		$disc =~ s/\n<br><br>\n/\n\n/g;
	     		$disc =~ s/([^\n])\n([^\n])/$1 $2/g;
	        	my $tmpParamString = "";
	        	
	        	$tmpParamString = $methID.$sep.$methName.$sep.$pos.$sep.$disc.$sep.$sep.$sep.$paramName.$sep.$type;
        		$tmpParamString = &convertCharsForFileMaker($tmpParamString);
        		$tmpParamString =~ s/$sep/\t/g;
	        	push (@paramLines, "$tmpParamString");
        	}
        }
    }
    $methString = join ("\n", @methLines);
    $paramString = join ("\n", @paramLines);
    $methString .= "\n";
    $paramString .= "\n";
    return ($methString, $paramString);
}


sub _getDataTypesAndFieldsExportString {
    my $self = shift;
    my @structObjs = $self->structs();
    my @typedefs = $self->typedefs();
    my @constants = $self->constants();
    my @enums = $self->enums();
    my @dataTypeLines;
    my @fieldLines;
    my $dataTypeString;
    my $fieldString;

    my $sep = "<tab>";       
    my $tmpString = "";
    my $contentString = "";
    
    # unused fields -- here for clarity
    my $englishName = "";
    my $specialConsiderations = "";
    my $versionNotes = "";
    
    # get Enumerations
    foreach my $obj (sort objName @enums) {
        my $name = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
        my $declaration = $obj->declaration();
        my $enumID = HeaderDoc::DBLookup->typeIDForName($name);
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract, $declaration) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $enumID.$sep.$name.$sep.$englishName.$sep.$abstract.$sep.$desc.$sep.$specialConsiderations.$sep.$versionNotes."\n";
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@dataTypeLines, "$tmpString");
    }
    # get Constants
    foreach my $obj (sort objName @constants) {
        my $name = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
        my $constID = HeaderDoc::DBLookup->typeIDForName($name);
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $constID.$sep.$name.$sep.$englishName.$sep.$abstract.$sep.$desc.$sep.$specialConsiderations.$sep.$versionNotes."\n";
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@dataTypeLines, "$tmpString");
    }
    # get Structs
    foreach my $obj (sort objName @structObjs) {
        my $name = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
        my $declaration = $obj->declaration();
        my @fields = $obj->fields();
        my $structID = HeaderDoc::DBLookup->typeIDForName($name);
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract, $declaration) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $structID.$sep.$name.$sep.$englishName.$sep.$abstract.$sep.$desc.$sep.$specialConsiderations.$sep.$versionNotes."\n";
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@dataTypeLines, "$tmpString");
        
        if (@fields) {
        	foreach my $field (@fields) {
        	    my $fName = $field->name();
        	    my $discussion = $field->discussion();
	     		$discussion =~ s/\n<br><br>\n/\n\n/g;
	     		$discussion =~ s/([^\n])\n([^\n])/$1 $2/g;
        	    my $pos = 0;
        	    
        	    $pos = $self->_positionOfNameInBlock($fName, $declaration);
		        if (!$pos) {
				my $filename = $HeaderDoc::headerObject->name();
		        	print "$filename:0:---------------------------------------------------------------------------\n";
		        	warn "$filename:0:Tagged parameter '$fName' not found in declaration of struct $name.\n";
		        	warn "$filename:0:Declaration for $name is:\n$declaration\n";
		        	print "$filename:0:---------------------------------------------------------------------------\n";
		        	$pos = "UNKNOWN_POSITION";
		        }
	        	my $tmpFieldString = "";
	        	$tmpFieldString = $structID.$sep.$name.$sep.$pos.$sep.$discussion.$sep.$sep.$sep.$fName;
        		$tmpFieldString = &convertCharsForFileMaker($tmpFieldString);
        		$tmpFieldString =~ s/$sep/\t/g;
	        	push (@fieldLines, "$tmpFieldString");
        	}
        }
    }
    # get Typedefs
    foreach my $obj (sort objName @typedefs) {
        my $name = $obj->name();
        my $desc = $obj->discussion();
        my $abstract = $obj->abstract();
        my $declaration = $obj->declaration();
        my @fields = $obj->fields();
        my $isFunctionPointer = $obj->isFunctionPointer();
        my $typedefID = HeaderDoc::DBLookup->typeIDForName($name);
        
        # Replace single internal carriage returns in fields with one space
        # headerDoc2HTML already changes two \n's to \n<br><br>\n, so we'll
        # just remove the breaks
        foreach my $string ($desc, $abstract, $declaration) {
     		$string =~ s/\n<br><br>\n/\n\n/g;
     		$string =~ s/([^\n])\n([^\n])/$1 $2/g;
        }
        $tmpString = $typedefID.$sep.$name.$sep.$englishName.$sep.$abstract.$sep.$desc.$sep.$specialConsiderations.$sep.$versionNotes."\n";
        $tmpString = &convertCharsForFileMaker($tmpString);
        $tmpString =~ s/$sep/\t/g;
        push (@dataTypeLines, "$tmpString");
        if (@fields) {
        	foreach my $field (@fields) {
        	    my $fName = $field->name();
        	    my $discussion = $field->discussion();
	     		$discussion =~ s/\n<br><br>\n/\n\n/g;
	     		$discussion =~ s/([^\n])\n([^\n])/$1 $2/g;
        	    my $pos = 0;
        	    
        	    if ($isFunctionPointer) {
        	        $pos = $self->_positionOfNameInFuncPtrDec($fName, $declaration);
        	    } else {
        	        $pos = $self->_positionOfNameInBlock($fName, $declaration);
        	    }
		        if (!$pos) {
				my $filename = $HeaderDoc::headerObject->name();
		        	print "$filename:0:---------------------------------------------------------------------------\n";
		        	warn "$filename:0:Tagged parameter '$fName' not found in declaration of struct $name.\n";
		        	warn "$filename:0:Declaration for $name is:\n$declaration\n";
		        	print "$filename:0:---------------------------------------------------------------------------\n";
		        	$pos = "UNKNOWN_POSITION";
		        }
	        	my $tmpFieldString = "";
	        	$tmpFieldString = $typedefID.$sep.$name.$sep.$pos.$sep.$discussion.$sep.$sep.$sep.$fName;
        		$tmpFieldString = &convertCharsForFileMaker($tmpFieldString);
        		$tmpFieldString =~ s/$sep/\t/g;
	        	push (@fieldLines, "$tmpFieldString");
        	}
        }
    }
    $dataTypeString = join ("\n", @dataTypeLines);
    $fieldString = join ("\n", @fieldLines);
	$dataTypeString .= "\n";
	$fieldString .= "\n";
    return ($dataTypeString, $fieldString);
}


sub _getEnumeratorsExportString {
    my $self = shift;
    my @enums = $self->enums();
    my @fieldLines;
    my $fieldString;

    my $sep = "<tab>";       
    my $tmpString = "";
    my $contentString = "";
    
    # get Enumerations
    foreach my $obj (sort objName @enums) {
        my $name = $obj->name();
        my @constants = $obj->constants();
        my $declaration = $obj->declaration();
        my $enumID = HeaderDoc::DBLookup->typeIDForName($name);
        
        if (@constants) {
        	foreach my $enumerator (@constants) {
        	    my $fName = $enumerator->name();
        	    my $discussion = $enumerator->discussion();
        	    my $pos = 0;
        	    
	     		$discussion =~ s/\n<br><br>\n/\n\n/g;
	     		$discussion =~ s/([^\n])\n([^\n])/$1 $2/g;
        	    $pos = $self->_positionOfNameInEnum($fName, $declaration);
		        if (!$pos) {
				my $filename = $HeaderDoc::headerObject->name();
		        	print "$filename:0:---------------------------------------------------------------------------\n";
		        	warn "$filename:0:Tagged parameter '$fName' not found in declaration of enum $name.\n";
		        	warn "$filename:0:Declaration for $name is:\n$declaration\n";
		        	print "$filename:0:---------------------------------------------------------------------------\n";
		        	$pos = "UNKNOWN_POSITION";
		        }
	        	my $tmpFieldString = "";
	        	$tmpFieldString = $enumID.$sep.$name.$sep.$pos.$sep.$discussion.$sep.$sep.$sep.$fName;
        		$tmpFieldString = &convertCharsForFileMaker($tmpFieldString);
        		$tmpFieldString =~ s/$sep/\t/g;
	        	push (@fieldLines, "$tmpFieldString");
        	}
        }
    }
    $fieldString = join ("\n", @fieldLines);
	$fieldString .= "\n";
    return $fieldString;
}

# this is simplistic is various ways--should be made more robust.
sub _positionOfNameInBlock {
    my $self = shift;
    my $name = shift;    
    my $block = shift;
    $block =~ s/\n/ /g;
    
    my $pos = 0;
    my $i = 0;
    my @chunks = split (/;/, $block);
    foreach my $string (@chunks) {
        $i++;
        $string = quotemeta($string);
        if ($string =~ /$name/) {
            $pos = $i;
            last;
        }
    }
    return $pos;
}

sub _positionOfNameInEnum {
    my $self = shift;
    my $name = shift;    
    my $block = shift;
    $block =~ s/\n/ /g;
    
    my $pos = 0;
    my $i = 0;
    my @chunks = split (/,/, $block);
    foreach my $string (@chunks) {
        $i++;
        $string = quotemeta($string);
        if ($string =~ /$name/) {
            $pos = $i;
            last;
        }
    }
    return $pos;
}

sub _positionOfNameInFuncPtrDec {
    my $self = shift;
    my $name = shift;    
    my $dec = shift;
    $dec =~ s/\n/ /g;
    
    my @decParts = split (/\(/, $dec);
    my $paramList = pop @decParts;
    
    
    my $pos = 0;
    my $i = 0;
    my @chunks = split (/,/, $paramList);
    foreach my $string (@chunks) {
        $i++;
        $string = quotemeta($string);
        if ($string =~ /$name/) {
            $pos = $i;
            last;
        }
    }
    return $pos;
}

sub _positionOfNameInMethPtrDec {
    my $self = shift;
    my $name = shift;    
    my $dec = shift;
    $dec =~ s/\n/ /g;
    
    my @decParts = split (/\(/, $dec);
    my $paramList = pop @decParts;
    
    my $pos = 0;
    my $i = 0;
    my @chunks = split (/,/, $paramList);
    foreach my $string (@chunks) {
        $i++;
        $string = quotemeta($string);
        if ($string =~ /$name/) {
            $pos = $i;
            last;
        }
    }
    return $pos;
}

sub _createExportFile {
    my $self = shift;
    my $outputFile = shift;    
    my $fileString = shift;    

	open(OUTFILE, ">$outputFile") || die "Can't write $outputFile.\n";
    if ($^O =~ /MacOS/i) {MacPerl::SetFileInfo('R*ch', 'TEXT', "$outputFile");};
	print OUTFILE $fileString;
	close OUTFILE;
}

sub _createXMLOutputFile {
    my $self = shift;
    my $class = ref($self);
    my $copyrightOwner = $class->copyrightOwner();
    my $outputFile = shift;    
    my $fileString = shift;    
    my $heading = shift;
    my $fullpath = $self->fullpath();

    if ($class eq "HeaderDoc::Header") {
	my $headercopyright = $self->headerCopyrightOwner();
	if (!($headercopyright eq "")) {
	    $copyrightOwner = $headercopyright;
	}
    }

    my $HTMLmeta = "";
    if ($class eq "HeaderDoc::Header") {
	$HTMLmeta = $self->HTMLmeta();
    }

	open(OUTFILE, ">$outputFile") || die "Can't write $outputFile.\n";
    if ($^O =~ /MacOS/i) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};
	print OUTFILE "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	print OUTFILE "<header filename=\"$heading\" headerpath=\"$fullpath\" headerclass=\"\">";
	print OUTFILE "<title>$heading</title>\n";

	# Need to get the C++ Class Abstract and Discussion....
	# my $headerDiscussion = $self->discussion();   
	# my $headerAbstract = $self->abstract(); 

	# print OUTFILE "<abstract>$headerAbstract</abstract>\n";
	# print OUTFILE "<discussion>$headerDiscussion</discussion>\n";

	print OUTFILE $fileString;
	print OUTFILE "<copyrightinfo>&#169; $copyrightOwner</copyrightinfo>" if (length($copyrightOwner));
	print OUTFILE "<timestamp>$dateStamp</timestamp>\n";
	print OUTFILE "</header>";
	close OUTFILE;
}

sub _createHTMLOutputFile {
    my $self = shift;
    my $class = ref($self);
    my $copyrightOwner = $class->copyrightOwner();
    my $outputFile = shift;    
    my $orig_fileString = shift;    
    my $heading = shift;    

    if ($class eq "HeaderDoc::Header") {
	my $headercopyright = $self->headerCopyrightOwner();
	if (!($headercopyright eq "")) {
	    $copyrightOwner = $headercopyright;
	}
    }

    my $HTMLmeta = "";
    if ($class eq "HeaderDoc::Header") {
	$HTMLmeta = $self->HTMLmeta();
    }

	my $fileString = html_fixup_links($self, $orig_fileString);

	open(OUTFILE, ">$outputFile") || die "Can't write $outputFile.\n";
    if ($^O =~ /MacOS/i) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};
	print OUTFILE "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n    \"http://www.w3.org/TR/1998/REC-html40-19980424/loose.dtd\">\n";
	print OUTFILE "<html>";
        print OUTFILE "<style type=\"text/css\">";
        print OUTFILE "<!--";
        print OUTFILE "a:link {text-decoration: none; font-family: Verdana, Geneva, Helvetica, Arial, sans-serif; font-size: small}";
        print OUTFILE "a:visited {text-decoration: none; font-family: Verdana, Geneva, Helvetica, Arial, sans-serif; font-size: small}";
        print OUTFILE "a:active {text-decoration: none; font-family: Verdana, Geneva, Helvetica, Arial, sans-serif; font-size: small}";
        print OUTFILE "a:hover {text-decoration: underline; font-family: Verdana, Geneva, Helvetica, Arial, sans-serif; font-size: small}";
        print OUTFILE "h4 {text-decoration: none; font-family: Verdana,Geneva,Arial,Helvetica,sans-serif; size: tiny; font-weight: bold}"; # bold 
        print OUTFILE "-->";
        print OUTFILE "</style>";
	print OUTFILE "<head>\n    <title>$heading</title>\n	$HTMLmeta <meta name=\"generator\" content=\"HeaderDoc\">\n";
	print OUTFILE "</head><body bgcolor=\"#ffffff\">\n";
	if ($HeaderDoc::insert_header) {
		print OUTFILE "<!-- start of header -->\n";
		print OUTFILE $self->htmlHeader()."\n";
		print OUTFILE "<!-- end of header -->\n";
	}
	print OUTFILE "<h1><font face=\"Geneva,Arial,Helvtica\">$heading</font></h1><br>\n";
	print OUTFILE $fileString;
    print OUTFILE "<p>";
    print OUTFILE "<p>&#169; $copyrightOwner " if (length($copyrightOwner));
    # print OUTFILE "(Last Updated $dateStamp)\n";
    my $filedate = $self->updated();
    if (length($filedate)) {
	    print OUTFILE "(Last Updated $filedate)\n";
    } else {
	    print OUTFILE "(Last Updated $dateStamp)\n";
    }
    print OUTFILE "</p>";
	print OUTFILE "</body></html>\n";
	close OUTFILE;
}

sub objGroup { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   # if ($HeaderDoc::sort_entries) {
	return ($obj1->group() cmp $obj2->group());
   # } else {
	# return (1 cmp 2);
   # }
}

sub objName { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   if ($HeaderDoc::sort_entries) {
        return ($obj1->name() cmp $obj2->name());
   } else {
	return (1 cmp 2);
   }
}

##################### Debugging ####################################

sub printObject {
    my $self = shift;
 
    print "------------------------------------\n";
    print "APIOwner\n";
    print "outputDir: $self->{OUTPUTDIR}\n";
    print "constantsDir: $self->{CONSTANTSDIR}\n";
    print "datatypesDir: $self->{DATATYPESDIR}\n";
    print "functionsDir: $self->{FUNCTIONSDIR}\n";
    print "methodsDir: $self->{METHODSDIR}\n";
    print "typedefsDir: $self->{TYPEDEFSDIR}\n";
    print "constants:\n";
    &printArray(@{$self->{CONSTANTS}});
    print "functions:\n";
    &printArray(@{$self->{FUNCTIONS}});
    print "methods:\n";
    &printArray(@{$self->{METHODS}});
    print "typedefs:\n";
    &printArray(@{$self->{TYPEDEFS}});
    print "structs:\n";
    &printArray(@{$self->{STRUCTS}});
    print "Inherits from:\n";
    $self->SUPER::printObject();
}

sub fixup_links
{
    my $self = shift;
    my $string = shift;
    my $mode = shift;
    my $ret = "";
    my $localDebug = 0;
    my $toplevel = 0;

    if ($mode > 1) {
	$mode = $mode - 2;
	$toplevel = 1;
    }

    my @elements = split(/</, $string);
    foreach my $element (@elements) {
	if ($element =~ /^hd_link (.*?)>/) {
	    # print "found.\n";
	    my $oldtarget = $1;
	    my $newtarget = $oldtarget;
	    my $prefix = $self->apiUIDPrefix();

	    if (!($oldtarget =~ /\/\/$prefix/)) {
		print "link needs to be resolved.\n" if ($localDebug);
		print "target is $oldtarget\n" if ($localDebug);
		$newtarget = resolve_link($oldtarget);
		# print "new target is $newtarget\n" if ($localDebug);
	    }

	    # print "element is $element\n";
	    $element =~ s/^hd_link $oldtarget>\s//;
	    # print "link name is $element\n";
	    if ($mode) {
		$ret .= "<hd_link apple_ref=\"$newtarget\">";
		$ret .= $element;
		$ret .= "</hd_link>";
	    } else {
		# if ($newtarget eq $oldtarget) {
		    $ret .= "<!-- a logicalPath=\"$newtarget\" -->";
		    $ret .= $element;
		    $ret .= "<!-- /a -->";
		# } else {
		    # if ($toplevel) {
			# $ret .= "<a href=\"CompositePage.html#$newtarget\">";
		    # } else {
			# $ret .= "<a href=\"../CompositePage.html#$newtarget\">";
		    # }
		    # $ret .= $element;
		    # $ret .= "</a>\n";
		# }
	    }
	} else {
	    if ($element =~ s/^\/hd_link>//) {
		$ret .= $element;
	    } else {
		$ret .= "<$element";
	    }
	}
    }
    $ret =~ s/^<//;

    return $ret;
}

sub toplevel_html_fixup_links
{
    my $self = shift;
    my $string = shift;
    my $resolver_output = fixup_links($self, $string, 2);

    return $resolver_output;
}

sub html_fixup_links
{
    my $self = shift;
    my $string = shift;
    my $resolver_output = fixup_links($self, $string, 0);

    return $resolver_output;
}

sub xml_fixup_links
{
    my $self = shift;
    my $string = shift;
    my $resolver_output = fixup_links($self, $string, 1);

    return $resolver_output;
}

sub resolve_link
{
    my $symbol = shift;
    my $ret = "";
    my $filename = $HeaderDoc::headerObject->filename();

    foreach my $uid (@uid_list) {
	if ($uid =~ /\/$symbol$/) {
	    if ($ret eq "" || $ret eq $uid) {
		$ret = $uid;
	    } else {
		print "$filename:0:WARNING: multiple matches found for symbol \"$symbol\"!!!\n";
		print "$filename:0:Only the first matching symbol will be linked.\n";
		print "$filename:0:Replace the symbol with a specific api ref tag\n";
		print "$filename:0:(e.g. apple_ref) in header file to fix this conflict.\n";
	    }
	}
    }
    if ($ret eq "") {
	print "$filename:0:WARNING: no symbol matching \"$symbol\" found.  If this\n";
	print "$filename:0:symbol is not in this file or class, you need to specify it\n";
	print "$filename:0:with an api ref tag (e.g. apple_ref).\n";
    }
    return $ret;
}

sub register_uid
{
    my $self = shift;
    my $uid = shift;
    my $localDebug = 0;

    print "pushing $uid\n" if ($localDebug);;
    push(@uid_list, $uid);
}

1;

