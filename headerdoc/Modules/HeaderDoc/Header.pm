#! /usr/bin/perl -w
#
# Class name: Header
# Synopsis: Holds header-wide comments parsed by headerDoc
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
package HeaderDoc::Header;

BEGIN {
	foreach (qw(Mac::Files)) {
	    $MOD_AVAIL{$_} = eval "use $_; 1";
    }
}

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash sanitize);
use HeaderDoc::APIOwner;

use strict;
use vars qw($VERSION @ISA);
$HeaderDoc::Header::VERSION = '$Revision: 1.15 $';

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
######################################################################

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

    $self->{CLASSES} = ();
    # $self->{CLASSESDIR} = undef;
    # $self->{UPDATED}= undef;
    $self->{COPYRIGHT}= "";
    $self->{HTMLMETA}= "";
    $self->{CATEGORIES}= ();
    # $self->{CATEGORIESDIR} = undef;
    $self->{PROTOCOLS}= ();
    # $self->{PROTOCOLSDIR} = undef;
    # $self->{CURRENTCLASS} = undef;
    $self->{CLASS} = "HeaderDoc::Header";
    
    $self->tocTitlePrefix('Header:');
}

sub outputDir {
    my $self = shift;

    if (@_) {
        my $rootOutputDir = shift;
        $self->SUPER::outputDir($rootOutputDir);
        $self->{OUTPUTDIR} = $rootOutputDir;
	    $self->classesDir("$rootOutputDir$pathSeparator"."Classes");
	    $self->protocolsDir("$rootOutputDir$pathSeparator"."Protocols");
	    $self->categoriesDir("$rootOutputDir$pathSeparator"."Categories");
    }
    return $self->{OUTPUTDIR};
}

sub fullpath {
    my $self = shift;

    if (@_) {
        my $fullpath = shift;
        $self->{FULLPATH} = $fullpath;
    }
    return $self->{FULLPATH};
}

### sub classesDir {
    ### my $self = shift;
### 
    ### if (@_) {
        ### $self->{CLASSESDIR} = shift;
    ### }
    ### return $self->{CLASSESDIR};
### }
### 
### sub classes {
    ### my $self = shift;
### 
    ### if (@_) {
        ### @{ $self->{CLASSES} } = @_;
    ### }
    ### ($self->{CLASSES}) ? return @{ $self->{CLASSES} } : return ();
### }
### 
### sub protocolsDir {
    ### my $self = shift;
### 
    ### if (@_) {
        ### $self->{PROTOCOLSDIR} = shift;
    ### }
    ### return $self->{PROTOCOLSDIR};
### }
### 
### sub protocols {
    ### my $self = shift;
    ### 
    ### if (@_) {
        ### @{ $self->{PROTOCOLS} } = @_;
    ### }
    ### ($self->{PROTOCOLS}) ? return @{ $self->{PROTOCOLS} } : return ();
### }
### 
### sub addToProtocols {
    ### my $self = shift;
### 
    ### if (@_) {
        ### foreach my $item (@_) {
            ### push (@{ $self->{PROTOCOLS} }, $item);
        ### }
    ### }
    ### return @{ $self->{PROTOCOLS} };
### }
### 
### sub categoriesDir {
    ### my $self = shift;
### 
    ### if (@_) {
        ### $self->{CATEGORIESDIR} = shift;
    ### }
    ### return $self->{CATEGORIESDIR};
### }

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

	print STDERR "updated is $updated\n" if ($localDebug);
	if (!($updated =~ /\d\d\d\d-\d\d-\d\d/o )) {
	    if (!($updated =~ /\d\d-\d\d-\d\d\d\d/o )) {
		if (!($updated =~ /\d\d-\d\d-\d\d/o )) {
		    # my $fullpath = $HeaderDoc::headerObject->fullpath();
		    my $fullpath = $self->fullpath();
		    my $linenum = $self->linenum();
		    print STDERR "$fullpath:$linenum: warning: Bogus date format: $updated. Valid formats are MM-DD-YYYY, MM-DD-YY, and YYYY-MM-DD\n";
		    return $self->{UPDATED};
		} else {
		    $month =~ s/(\d\d)-\d\d-\d\d/$1/smog;
		    $day =~ s/\d\d-(\d\d)-\d\d/$1/smog;
		    $year =~ s/\d\d-\d\d-(\d\d)/$1/smog;

		    my $century;
		    $century = `date +%C`;
		    $century *= 100;
		    $year += $century;
		    # $year += 2000;
		    print STDERR "YEAR: $year" if ($localDebug);
		}
	    } else {
		print STDERR "03-25-2003 case.\n" if ($localDebug);
		    $month =~ s/(\d\d)-\d\d-\d\d\d\d/$1/smog;
		    $day =~ s/\d\d-(\d\d)-\d\d\d\d/$1/smog;
		    $year =~ s/\d\d-\d\d-(\d\d\d\d)/$1/smog;
	    }
	} else {
		    $year =~ s/(\d\d\d\d)-\d\d-\d\d/$1/smog;
		    $month =~ s/\d\d\d\d-(\d\d)-\d\d/$1/smog;
		    $day =~ s/\d\d\d\d-\d\d-(\d\d)/$1/smog;
	}
	$month =~ s/\n*//smog;
	$day =~ s/\n*//smog;
	$year =~ s/\n*//smog;
	$month =~ s/\s*//smog;
	$day =~ s/\s*//smog;
	$year =~ s/\s*//smog;

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
		# my $fullpath = $HeaderDoc::headerObject->fullpath();
		my $fullpath = $self->fullpath();
		my $linenum = $self->linenum();
		print STDERR "$fullpath:$linenum: warning: Invalid date (year = $year, month = $month, day = $day). Valid formats are MM-DD-YYYY, MM-DD-YY, and YYYY-MM-DD\n";
		return $self->{UPDATED};
	} else {
		$self->{UPDATED} = HeaderDoc::HeaderElement::strdate($month-1, $day, $year);
		print STDERR "date set to ".$self->{UPDATED}."\n" if ($localDebug);
	}
    }
    return $self->{UPDATED};
}

### sub categories {
    ### my $self = shift;
### 
    ### if (@_) {
        ### @{ $self->{CATEGORIES} } = @_;
    ### }
    ### ($self->{CATEGORIES}) ? return @{ $self->{CATEGORIES} } : return ();
### }
### 
### sub addToCategories {
    ### my $self = shift;
### 
    ### if (@_) {
        ### foreach my $item (@_) {
            ### push (@{ $self->{CATEGORIES} }, $item);
        ### }
    ### }
    ### return @{ $self->{CATEGORIES} };
### }

# removes a maximum of one object per invocation
# we remove a catagory if we've been successful finding 
# the associated class and adding the category methods to it.
sub removeFromCategories {
    my $self = shift;
    my $objToRemove = shift;
    my $nameOfObjToRemove = $objToRemove->name();
    my @tempArray;
    my @categories = $self->categories();
    my $localDebug = 0;
    
    if (!@categories) {return;};

	foreach my $obj (@categories) {
	    if (ref($obj) eq "HeaderDoc::ObjCCategory") { 
			my $fullName = $obj->name();
			if ($fullName ne $nameOfObjToRemove) {
				push (@tempArray, $obj);
			} else {
				print STDERR "Removing $fullName from Header object.\n" if ($localDebug);
			}
		}
	}
	# we set it directly since the accessor will not allow us to set an empty array
	@{ $self->{CATEGORIES} } = @tempArray;
}

sub headerCopyrightOwner {
    my $self = shift;

    if (@_) {     
	my $test = shift;
	$self->{COPYRIGHT} = $test;
    }
    return $self->{COPYRIGHT};
}

sub HTMLmeta {
    my $self = shift;

    if (@_) {
	my $text = shift;

	if ($text =~ /=/o) {
		# @meta blah="blah" this="that"
		#    becomes
		# <meta blah="blah" this="that">
		$text =~ s/\n.*//smog;
		$self->{HTMLMETA} .= "<meta $text />\n";
	} else {
		# @meta nameparm contentparm
		#    becomes
		# <meta name="nameparm" content="contentparm">
		$text =~ /^(.*?)\s/o;
		my $name = $1;
		$text =~ s/^$name\s+//;
		$text =~ s/\n.*//smog;

		$self->{HTMLMETA} .= "<meta name=\"$name\" content=\"$text\" />\n";
	}
    }

    my $extendedmeta = $self->{HTMLMETA};
    my $encoding = $self->encoding();

    $extendedmeta .= "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=$encoding\" />\n";

    return $extendedmeta;
}

sub metaFileText {
    my $self = shift;
    my $encoding = $self->encoding();
    my $text = "<?xml version=\"1.0\" encoding=\"$encoding\"?>\n";

    $text .= "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    $text .= "<plist version=\"1.0\">\n";
    $text .= "<dict>\n";

    my $title = $self->name();

    if (!("$title" eq "")) {
	$text .= "<key>BookTitle</key>\n";
	$text .= "<string>$title HeaderDoc Reference</string>\n";
    }
    $text .= "<key>WriterEmail</key>\n";
    $text .= "<string>techpubs\@group.apple.com</string>\n";
    $text .= "<key>EDD_Name</key>\n";
    $text .= "<string>ProceduralC.EDD</string>\n";
    $text .= "<key>EDD_Version</key>\n";
    $text .= "<string>3.31</string>\n";
    $text .= "<key>ReleaseDateFooter</key>\n";
    my $date = `date +"%B %Y"`;
    $date =~ s/\n//smog;
    $text .= "<string>$date</string>\n";
    $text .= "</dict>\n";
    $text .= "</plist>\n";

    return $text;
}

### sub writeHeaderElements {
    ### my $self = shift;
    ### my $classesDir = $self->classesDir();
    ### my $protocolsDir = $self->protocolsDir();
    ### my $categoriesDir = $self->categoriesDir();
### 
    ### $self->SUPER::writeHeaderElements();
    ### # if ($self->classes()) {
		### # if (! -e $classesDir) {
			### # unless (mkdir ("$classesDir", 0777)) {die ("Can't create output folder $classesDir. \n$!\n");};
	    ### # }
	    ### # $self->writeClasses();
    ### # }
    ### # if ($self->protocols()) {
		### # if (! -e $protocolsDir) {
			### # unless (mkdir ("$protocolsDir", 0777)) {die ("Can't create output folder $protocolsDir. \n$!\n");};
	    ### # }
	    ### # $self->writeProtocols();
    ### # }
    ### # if ($self->categories()) {
		### # if (! -e $categoriesDir) {
			### # unless (mkdir ("$categoriesDir", 0777)) {die ("Can't create output folder $categoriesDir. \n$!\n");};
	    ### # }
	    ### # $self->writeCategories();
    ### # }
### }

sub writeHeaderElementsToCompositePage {
    my $self = shift;
    my @classObjs = $self->classes();
    my @protocolObjs = $self->protocols();
    my @categoryObjs = $self->categories();

    $self->SUPER::writeHeaderElementsToCompositePage();
    if ($self->classes()) {
	    foreach my $obj (@classObjs) {
		    $obj->writeHeaderElementsToCompositePage(); 
	    }
    }
    if ($self->protocols()) {
	    foreach my $obj (@protocolObjs) {
		    $obj->writeHeaderElementsToCompositePage(); 
	    }
    }
    if ($self->categories()) {
	    foreach my $obj (@categoryObjs) {
		    $obj->writeHeaderElementsToCompositePage(); 
	    }
    }
}

sub writeProtocols {
    my $self = shift;
    my @protocolObjs = $self->protocols();
    my $protocolsRootDir = $self->protocolsDir();
        
    my @tempobjs = ();
    if ($HeaderDoc::sort_entries) {
	@tempobjs = sort objName @protocolObjs;
    } else {
	@tempobjs = @protocolObjs;
    }
    foreach my $obj (@tempobjs) {
        my $protocolName = $obj->name();
        # for now, always shorten long names since some files may be moved to a Mac for browsing
        if (1 || $isMacOS) {$protocolName = &safeName(filename => $protocolName);};
        $obj->outputDir("$protocolsRootDir$pathSeparator$protocolName");
        $obj->createFramesetFile();
        $obj->createContentFile() if (!$HeaderDoc::ClassAsComposite);
        $obj->createTOCFile();
        $obj->writeHeaderElements(); 
    }
}

sub writeCategories {
    my $self = shift;
    my @categoryObjs = $self->categories();
    my $categoriesRootDir = $self->categoriesDir();
        
    my @tempobjs = ();
    if ($HeaderDoc::sort_entries) {
	@tempobjs = sort objName @categoryObjs;
    } else {
	@tempobjs = @categoryObjs;
    }
    foreach my $obj (@tempobjs) {
        my $categoryName = $obj->name();
        # for now, always shorten long names since some files may be moved to a Mac for browsing
        if (1 || $isMacOS) {$categoryName = &safeName(filename => $categoryName);};
        $obj->outputDir("$categoriesRootDir$pathSeparator$categoryName");
        $obj->createFramesetFile();
        $obj->createContentFile() if (!$HeaderDoc::ClassAsComposite);
        $obj->createTOCFile();
        $obj->writeHeaderElements(); 
    }
}

# use Devel::Peek;

sub docNavigatorComment {
    my $self = shift;
    # print STDERR "IX0\n"; Dump($self);
    my $name = $self->name();
    my $procname = $name;
    $procname =~ s/;//sgo;
    $name =~ s/;/\\;/sgo;
    # my $shortname = $self->filename();
    # $shortname =~ s/\.hdoc$//so;
    # $shortname = sanitize($shortname, 1);
    # print STDERR "IX1\n"; Dump($self);
    my $uid = $self->apiuid();
    
    if ($self->isFramework()) {
	# Don't insert a UID.  It will go on the landing page.
	return $self->apiref(0, "framework"); # "<!-- headerDoc=Framework; shortname=$shortname; uid=".$uid."; name=$name-->";
    } else {
	# return "<!-- headerDoc=Header; name=$procname-->";
	return $self->apiref(0, "Header");
    }
}

################## Misc Functions ###################################


sub objName { # used for sorting
    my $obj1 = $a;
    my $obj2 = $b;
    return (lc($obj1->name()) cmp lc($obj2->name()));
}

##################### Debugging ####################################

sub printObject {
    my $self = shift;
    my $classesDir = $self->{CLASSESDIR};
    my $categoriesDir = $self->{CATEGORIESDIR};
    my $protocolsDir = $self->{PROTOCOLSDIR};
    my $currentClass = $self->{CURRENTCLASS};
 
    print STDERR "Header\n";
    print STDERR " classes dir:    $classesDir\n";
    print STDERR " categories dir: $categoriesDir\n";
    print STDERR " protocols dir:  $protocolsDir\n";
    print STDERR " current class:  $currentClass\n";
    $self->SUPER::printObject();
    print STDERR "  Classes:\n";
    &printArray(@{$self->{CLASSES}});
    print STDERR "  Categories:\n";
    &printArray(@{$self->{CATEGORIES}});
    print STDERR "  Protocols:\n";
    &printArray(@{$self->{PROTOCOLS}});
    
    print STDERR "\n";
}

1;

