#! /usr/bin/perl -w
#
# Class name: Header
# Synopsis: Holds header-wide comments parsed by headerDoc
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
package HeaderDoc::Header;

BEGIN {
	foreach (qw(Mac::Files)) {
	    $MOD_AVAIL{$_} = eval "use $_; 1";
    }
}

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc convertCharsForFileMaker printArray printHash);
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
    $self->{CLASSESDIR} = undef;
    $self->{CATEGORIES}= ();
    $self->{CATEGORIESDIR} = undef;
    $self->{PROTOCOLS}= ();
    $self->{PROTOCOLSDIR} = undef;
    $self->{CURRENTCLASS} = undef;
    
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

sub classesDir {
    my $self = shift;

    if (@_) {
        $self->{CLASSESDIR} = shift;
    }
    return $self->{CLASSESDIR};
}

sub classes {
    my $self = shift;

    if (@_) {
        @{ $self->{CLASSES} } = @_;
    }
    ($self->{CLASSES}) ? return @{ $self->{CLASSES} } : return ();
}

sub currentClass {
    my $self = shift;

    if (@_) {
        @{ $self->{CURRENTCLASS} } = @_;
    }
    return @{ $self->{CURRENTCLASS} };
}

sub addToClasses {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            $self->currentClass($item);
            push (@{ $self->{CLASSES} }, $item);
        }
    }
    return @{ $self->{CLASSES} };
}

sub protocolsDir {
    my $self = shift;

    if (@_) {
        $self->{PROTOCOLSDIR} = shift;
    }
    return $self->{PROTOCOLSDIR};
}

sub protocols {
    my $self = shift;
    
    if (@_) {
        @{ $self->{PROTOCOLS} } = @_;
    }
    ($self->{PROTOCOLS}) ? return @{ $self->{PROTOCOLS} } : return ();
}

sub addToProtocols {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{PROTOCOLS} }, $item);
        }
    }
    return @{ $self->{PROTOCOLS} };
}

sub categoriesDir {
    my $self = shift;

    if (@_) {
        $self->{CATEGORIESDIR} = shift;
    }
    return $self->{CATEGORIESDIR};
}

sub categories {
    my $self = shift;

    if (@_) {
        @{ $self->{CATEGORIES} } = @_;
    }
    ($self->{CATEGORIES}) ? return @{ $self->{CATEGORIES} } : return ();
}

sub addToCategories {
    my $self = shift;

    if (@_) {
        foreach my $item (@_) {
            push (@{ $self->{CATEGORIES} }, $item);
        }
    }
    return @{ $self->{CATEGORIES} };
}

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
				print "Removing $fullName from Header object.\n" if ($localDebug);
			}
		}
	}
	# we set it directly since the accessor will not allow us to set an empty array
	@{ $self->{CATEGORIES} } = @tempArray;
}

sub writeHeaderElements {
    my $self = shift;
    my $classesDir = $self->classesDir();
    my $protocolsDir = $self->protocolsDir();
    my $categoriesDir = $self->categoriesDir();

    $self->SUPER::writeHeaderElements();
    if ($self->classes()) {
		if (! -e $classesDir) {
			unless (mkdir ("$classesDir", 0777)) {die ("Can't create output folder $classesDir. \n$!\n");};
	    }
	    $self->writeClasses();
    }
    if ($self->protocols()) {
		if (! -e $protocolsDir) {
			unless (mkdir ("$protocolsDir", 0777)) {die ("Can't create output folder $protocolsDir. \n$!\n");};
	    }
	    $self->writeProtocols();
    }
    if ($self->categories()) {
		if (! -e $categoriesDir) {
			unless (mkdir ("$categoriesDir", 0777)) {die ("Can't create output folder $categoriesDir. \n$!\n");};
	    }
	    $self->writeCategories();
    }
}

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

sub writeClasses {
    my $self = shift;
    my @classObjs = $self->classes();
    my $classRootDir = $self->classesDir();
        
    foreach my $obj (sort objName @classObjs) {
        my $className = $obj->name();
        # for now, always shorten long names since some files may be moved to a Mac for browsing
        if (1 || $isMacOS) {$className = &safeName(filename => $className);};
        $obj->outputDir("$classRootDir$pathSeparator$className");
        $obj->createFramesetFile();
        $obj->createContentFile();
        $obj->createTOCFile();
        $obj->writeHeaderElements(); 
    }
}

sub writeProtocols {
    my $self = shift;
    my @protocolObjs = $self->protocols();
    my $protocolsRootDir = $self->protocolsDir();
        
    foreach my $obj (sort objName @protocolObjs) {
        my $protocolName = $obj->name();
        # for now, always shorten long names since some files may be moved to a Mac for browsing
        if (1 || $isMacOS) {$protocolName = &safeName(filename => $protocolName);};
        $obj->outputDir("$protocolsRootDir$pathSeparator$protocolName");
        $obj->createFramesetFile();
        $obj->createContentFile();
        $obj->createTOCFile();
        $obj->writeHeaderElements(); 
    }
}

sub writeCategories {
    my $self = shift;
    my @categoryObjs = $self->categories();
    my $categoriesRootDir = $self->categoriesDir();
        
    foreach my $obj (sort objName @categoryObjs) {
        my $categoryName = $obj->name();
        # for now, always shorten long names since some files may be moved to a Mac for browsing
        if (1 || $isMacOS) {$categoryName = &safeName(filename => $categoryName);};
        $obj->outputDir("$categoriesRootDir$pathSeparator$categoryName");
        $obj->createFramesetFile();
        $obj->createContentFile();
        $obj->createTOCFile();
        $obj->writeHeaderElements(); 
    }
}

sub createTOCFile {
    my $self = shift;
    my $rootDir = $self->outputDir();
    my $tocTitlePrefix = $self->tocTitlePrefix();
    my $outputFileName = "toc.html";    
    my $outputFile = "$rootDir$pathSeparator$outputFileName";    
    my $fileString = $self->tocString();    
    my $filename = $self->name();    

	open(OUTFILE, ">$outputFile") || die "Can't write $outputFile.\n$!\n";
    if ($isMacOS) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};
	print OUTFILE "<html><head><title>Documentation for $filename</title></head>\n";
	print OUTFILE "<body bgcolor=\"#cccccc\">\n";
	print OUTFILE "<table border=\"0\" cellpadding=\"0\" cellspacing=\"2\" width=\"148\">\n";
	print OUTFILE "<tr><td colspan=\"2\"><font size=\"5\" color=\"#330066\"><b>$tocTitlePrefix</b></font></td></tr>\n";
	print OUTFILE "<tr><td width=\"15\"></td><td><b><font size=\"+1\">$filename</font></b></td></tr>\n";
	print OUTFILE "</table><hr>\n";
	print OUTFILE $fileString;
	print OUTFILE "</body></html>\n";
	close OUTFILE;
}

sub tocString {
    my $self = shift;
    my @classes = $self->classes();
    my @protocols = $self->protocols();
    my @categories = $self->categories();
	my $compositePageName = HeaderDoc::APIOwner->compositePageName();
	my $defaultFrameName = HeaderDoc::APIOwner->defaultFrameName();
    
    my $tocString = $self->SUPER::tocString();

    if (@classes) {
	    $tocString .= "<h4>Classes</h4>\n";
	    foreach my $obj (sort objName @classes) {
	        my $name = $obj->name();
	        my $safeName = $name;
	        # for now, always shorten long names since some files may be moved to a Mac for browsing
            if (1 || $isMacOS) {$safeName = &safeName(filename => $name);};
	        $tocString .= "<nobr>&nbsp;<a href = \"Classes/$safeName/$defaultFrameName\" target =\"_top\">$name</a></nobr><br>\n";
	    }
    }
    if (@protocols) {
	    $tocString .= "<h4>Protocols</h4>\n";
	    foreach my $obj (sort objName @protocols) {
	        my $name = $obj->name();
	        my $safeName = $name;
	        # for now, always shorten long names since some files may be moved to a Mac for browsing
            if (1 || $isMacOS) {$safeName = &safeName(filename => $name);};
	        $tocString .= "<nobr>&nbsp;<a href = \"Protocols/$safeName/$defaultFrameName\" target =\"_top\">$name</a></nobr><br>\n";
	    }
    }
    if (@categories) {
	    $tocString .= "<h4>Categories</h4>\n";
	    foreach my $obj (sort objName @categories) {
	        my $name = $obj->name();
	        my $safeName = $name;
	        # for now, always shorten long names since some files may be moved to a Mac for browsing
            if (1 || $isMacOS) {$safeName = &safeName(filename => $name);};
	        $tocString .= "<nobr>&nbsp;<a href = \"Categories/$safeName/$defaultFrameName\" target =\"_top\">$name</a></nobr><br>\n";
	    }
    }
    $tocString .= "<br><hr><a href=\"$compositePageName\" target =\"_blank\">[Printable HTML Page]</a>\n";
    return $tocString;
}

sub docNavigatorComment {
    my $self = shift;
    my $name = $self->name();
    
    return "<-- headerDoc=Header; name=$name-->";
}

################## Misc Functions ###################################


sub objName { # used for sorting
   my $obj1 = $a;
   my $obj2 = $b;
   return ($obj1->name() cmp $obj2->name());
}

##################### Debugging ####################################

sub printObject {
    my $self = shift;
    my $classesDir = $self->{CLASSESDIR};
    my $categoriesDir = $self->{CATEGORIESDIR};
    my $protocolsDir = $self->{PROTOCOLSDIR};
    my $currentClass = $self->{CURRENTCLASS};
 
    print "Header\n";
    print " classes dir:    $classesDir\n";
    print " categories dir: $categoriesDir\n";
    print " protocols dir:  $protocolsDir\n";
    print " current class:  $currentClass\n";
    $self->SUPER::printObject();
    print "  Classes:\n";
    &printArray(@{$self->{CLASSES}});
    print "  Categories:\n";
    &printArray(@{$self->{CATEGORIES}});
    print "  Protocols:\n";
    &printArray(@{$self->{PROTOCOLS}});
    
    print "\n";
}

1;

