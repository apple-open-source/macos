#! /usr/bin/perl -w
#
# Class name: Header
# Synopsis: Holds header-wide comments parsed by headerDoc
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2001/03/22 02:27:13 $
# 
# Copyright (c) 1999 Apple Computer, Inc.  All Rights Reserved.
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

sub new {
    my($param) = shift;
    my($class) = ref($param) || $param;
    my $self = {};
    
    bless($self, $class);
    $self->SUPER::_initialize();
    $self->_initialize();
    return($self);
}

sub _initialize {
    my($self) = shift;
    $self->{CLASSES} = [];
    $self->{CLASSESDIR} = undef;
    $self->{CURRENTCLASS} = undef;
}

sub outputDir {
    my $self = shift;

    if (@_) {
        my $rootOutputDir = shift;
        $self->SUPER::outputDir($rootOutputDir);
        $self->{OUTPUTDIR} = $rootOutputDir;
	    $self->classesDir("$rootOutputDir$pathSeparator"."Classes");
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
    return @{ $self->{CLASSES} };
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

sub writeHeaderElements {
    my $self = shift;
    my $classesDir = $self->classesDir();

    $self->SUPER::writeHeaderElements();
    if ($self->classes()) {
		if (! -e $classesDir) {
			unless (mkdir ("$classesDir", 0777)) {die ("Can't create output folder $classesDir. \n$!\n");};
	    }
	    $self->writeClasses();
    }
}

sub writeHeaderElementsToCompositePage {
    my $self = shift;
    my @classObjs = $self->classes();

    $self->SUPER::writeHeaderElementsToCompositePage();
    if ($self->classes()) {
	    foreach my $obj (@classObjs) {
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
        if (1 || $isMacOS) {$className = &safeName($className);};
        $obj->outputDir("$classRootDir$pathSeparator$className");
        $obj->createFramesetFile();
        $obj->createContentFile();
        $obj->createTOCFile();
        $obj->writeHeaderElements(); 
    }
}

sub createTOCFile {
    my $self = shift;
    my $rootDir = $self->outputDir();
    my $outputFileName = "toc.html";    
    my $outputFile = "$rootDir$pathSeparator$outputFileName";    
    my $fileString = $self->tocString();    
    my $filename = $self->name();    

	open(OUTFILE, ">$outputFile") || die "Can't write $outputFile.\n$!\n";
    if ($isMacOS) {MacPerl::SetFileInfo('MSIE', 'TEXT', "$outputFile");};
	print OUTFILE "<html><head><title>Draft Documentation for $filename</title></head>\n";
	print OUTFILE "<body bgcolor=\"#cccccc\">\n";
	print OUTFILE "<table border=\"0\" cellpadding=\"0\" cellspacing=\"2\" width=\"148\">\n";
	print OUTFILE "<tr><td colspan=\"2\"><font size=\"5\" color=\"#330066\"><b>Header:</b></font></td></tr>\n";
	print OUTFILE "<tr><td width=\"15\"></td><td><b><font size=\"+1\">$filename</font></b></td></tr>\n";
	print OUTFILE "</table><hr>\n";
	print OUTFILE $fileString;
	print OUTFILE "</body></html>\n";
	close OUTFILE;
}

sub tocString {
    my $self = shift;
    my @classes = $self->classes();
	my $compositePageName = HeaderDoc::APIOwner->compositePageName();
	my $defaultFrameName = HeaderDoc::APIOwner->defaultFrameName();
    
    my $tocString = $self->SUPER::tocString();

    if (@classes) {
	    $tocString .= "<h4>Classes</h4>\n";
	    foreach my $obj (sort objName @classes) {
	        my $name = $obj->name();
	        my $safeName = $name;
	        # for now, always shorten long names since some files may be moved to a Mac for browsing
            if (1 || $isMacOS) {$safeName = &safeName($name);};
	        $tocString .= "<nobr>&nbsp;<a href = \"Classes/$safeName/$defaultFrameName\" target =\"_top\">$name</a></nobr><br>\n";
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
 
    print "Header\n";
    $self->SUPER::printObject();
    print "outputDir: $self->{OUTPUTDIR}\n";
    print "constantsDir: $self->{CONSTANTSDIR}\n";
    print "datatypesDir: $self->{DATATYPESDIR}\n";
    print "functionsDir: $self->{FUNCTIONSDIR}\n";
    print "typedefsDir: $self->{TYPEDEFSDIR}\n";
    print "constants:\n";
    &printArray(@{$self->{CONSTANTS}});
    print "functions:\n";
    &printArray(@{$self->{FUNCTIONS}});
    print "typedefs:\n";
    &printArray(@{$self->{TYPEDEFS}});
    print "\n";
}

1;

