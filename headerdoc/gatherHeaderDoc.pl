#! /usr/bin/perl -w
#
# Script name: gatherHeaderDoc
# Synopsis: 	Finds all HeaderDoc generated docs in an input
#		folder and creates a top-level HTML page to them
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2001/11/30 22:43:15 $
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
# $Revision: 1.5 $
######################################################################
use Cwd;
use File::Find;
use File::Copy;

my $pathSeparator;
my $isMacOS;
my $scriptDir;
my $framesetFileName;
my $masterTOCFileName;
BEGIN {
    if ($^O =~ /MacOS/i) {
            $pathSeparator = ":";
            $isMacOS = 1;
    } else {
            $pathSeparator = "/";
            $isMacOS = 0;
    }
}

use strict;
use FindBin qw ($Bin);
use lib "$Bin"."$pathSeparator"."Modules";

# Modules specific to gatherHeaderDoc
use HeaderDoc::DocReference;
use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash updateHashFromConfigFiles getHashFromConfigFile);

my $debugging = 1;
######################################## Design Overview ###################################
# - We scan input directory for frameset files (index.html, by default).
# - For each frameset file, we look for a special HTML comment (left by HeaderDoc)
#   that tell us the name of the header/class and the type (header or cppclass). 
# - We create a DocReference object to store this info, and also the path to the
#   frameset file.
# - We run through array of DocRef objs and create a master TOC based on the info
# - Finally, we visit each TOC file in each frameset and add a "[Top]" link
#   back to the master TOC.  [This is fragile in the current implementation, since
#   we find TOCs based on searching for a file called "toc.html" in the frameset dir.]
# 
########################## Setup from Configuration File #######################
my $localConfigFileName = "headerDoc2HTML.config";
my $preferencesConfigFileName = "com.apple.headerDoc2HTML.config";
my $homeDir;
my $usersPreferencesPath;
#added WD-rpw 07/30/01 to support running on MacPerl
if ($^O =~ /MacOS/i)  {
	require "FindFolder.pl";
	$homeDir = MacPerl::FindFolder("D");	#D = Desktop. Arbitrary place to put things
	$usersPreferencesPath = MacPerl::FindFolder("P");	#P = Preferences
} else {
	$homeDir = (getpwuid($<))[7];
	$usersPreferencesPath = $homeDir.$pathSeparator."Library".$pathSeparator."Preferences";
}

my @configFiles = ($Bin.$pathSeparator.$localConfigFileName, $usersPreferencesPath.$pathSeparator.$preferencesConfigFileName);

# default configuration, which will be modified by assignments found in config files.
# The default values listed in this hash must be the same as those in the identical 
# hash in headerDoc2HTML--so that links between the frameset and the masterTOC work.
my %config = (
    defaultFrameName => "index.html", 
    masterTOCName => "MasterTOC.html"
);

%config = &updateHashFromConfigFiles(\%config,\@configFiles);

if (defined $config{"defaultFrameName"}) {
	$framesetFileName = $config{"defaultFrameName"};
} 
if (defined $config{"masterTOCName"}) {
	$masterTOCFileName = $config{"masterTOCName"};
} 

########################## Input Folder and Files #######################
my @inputFiles;
my $inputDir;

if (($#ARGV == 0) && (-d $ARGV[0])) {
    $inputDir = $ARGV[0];

	if ($^O =~ /MacOS/i) {
		find(\&getFiles, $inputDir);
	} else {
		$inputDir =~ s|(.*)/$|$1|; # get rid of trailing slash, if any
		if ($inputDir !~ /^\//) { # not absolute path -- !!! should check for ~
			my $cwd = cwd();
			$inputDir = $cwd.$pathSeparator.$inputDir;
		}
		&find({wanted => \&getFiles, follow => 1}, $inputDir);
	}
} else {
    die "You must specify a single input directory for processing.\n";
}
unless (@inputFiles) { print "No valid input files specified. \n\n";};

sub getFiles {
    my $filePath = $File::Find::name;
    my $fileName = $_;
    
    if ($fileName =~ /$framesetFileName/) {
        push(@inputFiles, $filePath);
    }
}
########################## Find HeaderDoc Comments #######################
my @headerFramesetRefs;
my @classFramesetRefs;
my @categoryFramesetRefs;
my @protocolFramesetRefs;

my $oldRecSep = $/;
undef $/; # read in files as strings

foreach my $file (@inputFiles) {
    open (INFILE, "<$file") || die "Can't open $file: $!\n";
    my $fileString = <INFILE>;
    close INFILE;
    if ($fileString =~ /<--\s+(headerDoc\s*=.*?)-->/) {
        my $fullComment = $1;
        my @pairs = split(/;/, $fullComment);
        my $docRef = HeaderDoc::DocReference->new;
        $docRef->path($file);
        foreach my $pair (@pairs) {
            my ($key, $value) = split(/=/, $pair);
            $key =~ s/^\s+|\s+$//;
            $value =~ s/^\s+|\s+$//;
            SWITCH: {
                ($key =~ /headerDoc/) && 
                    do {
                        $docRef->type($value);
                        last SWITCH;
                    };
                ($key =~ /name/) && 
                    do {
                        $docRef->name($value);
                        last SWITCH;
                    };
            }
        }
        my $tmpType = $docRef->type();
        if ($tmpType eq "Header") {
            push (@headerFramesetRefs, $docRef);
        } elsif ($tmpType eq "cl"){
            push (@classFramesetRefs, $docRef);
        } elsif ($tmpType eq "intf"){
            push (@protocolFramesetRefs, $docRef);
        } elsif ($tmpType eq "ObjCCategory"){
            push (@categoryFramesetRefs, $docRef);
        } else {
            my $tmpName = $docRef->name();
            my $tmpPath = $docRef->path();
            print "Unknown type '$tmpType' for document with name '$tmpName' and path '$tmpPath'\n";
        }
    }
}
$/ = $oldRecSep;

# create master TOC if we have any framesets
if (scalar(@headerFramesetRefs) + scalar(@classFramesetRefs) + scalar(@protocolFramesetRefs) + scalar(@categoryFramesetRefs)) {
    &printMasterTOC();
    &addTopLinkToFramesetTOCs();
} else {
    print "gatherHeaderDoc.pl: No HeaderDoc framesets found--returning\n" if ($debugging); 
}
exit 0;

################### Print Navigation Page #######################
sub printMasterTOC {
    my $outputDir = $inputDir;
    my $masterTOC = $outputDir.$pathSeparator. $masterTOCFileName;
    my $headersLinkString= '';
    my $classesLinkString = '';
    my $protocolsLinkString = '';
    my $categoriesLinkString = '';
    my $fileString;
    my $localDebug = 0;
    
    # get the HTML links to each header 
    foreach my $ref (sort objName @headerFramesetRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
        my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name);
        $headersLinkString .= $tmpString;
    }
    print "\$headersLinkString is '$headersLinkString'\n" if ($localDebug);
    
    # get the HTML links to each class 
    foreach my $ref (sort objName @classFramesetRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
        my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name);
        $classesLinkString .= $tmpString;
    }
    if (($localDebug) && length($classesLinkString)) {print "\$classesLinkString is '$classesLinkString'\n";};
    
    # get the HTML links to each protocol 
    foreach my $ref (sort objName @protocolFramesetRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
        my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name);
        $protocolsLinkString .= $tmpString;
    }
    if (($localDebug) && length($protocolsLinkString)) {print "\$protocolsLinkString is '$protocolsLinkString'\n";};
    
    # get the HTML links to each category 
    foreach my $ref (sort objName @categoryFramesetRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
        my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name);
        $categoriesLinkString .= $tmpString;
    }
    if (($localDebug) && length($categoriesLinkString)) {print "\$categoriesLinkString is '$categoriesLinkString'\n";};
    
    # put together header/footer with linkString--could use template
    my $htmlHeader = "<html><head><title>Header Documentation</title></head><body bgcolor=\"#cccccc\"><h1>Header Documentation</h1><hr><br>\n";
    my $headerSection = "<h2>Headers</h2>\n<blockquote>\n".$headersLinkString."\n</blockquote>\n";
    my $classesSection = '';
    if (length($classesLinkString)) {
    	$classesSection = "<h2>Classes</h2>\n<blockquote>\n".$classesLinkString."\n</blockquote>\n";
    }
    my $categoriesSection = '';
    if (length($categoriesLinkString)) {
    	$categoriesSection = "<h2>Categories</h2>\n<blockquote>\n".$categoriesLinkString."\n</blockquote>\n";
    }
    my $protocolsSection = '';
    if (length($protocolsLinkString)) {
    	$protocolsSection = "<h2>Protocols</h2>\n<blockquote>\n".$protocolsLinkString."\n</blockquote>\n";
    }
    my $htmlFooter = "</body>\n</html>\n";
    $fileString = $htmlHeader.$headerSection.$classesSection.$categoriesSection.$protocolsSection.$htmlFooter;
    
    # write out page
    print "gatherHeaderDoc.pl: writing master TOC to $masterTOC\n" if ($localDebug);
    open(OUTFILE, ">$masterTOC") || die "Can't write $masterTOC.\n";
    print OUTFILE $fileString;
    close OUTFILE;
}

sub addTopLinkToFramesetTOCs {
    my $masterTOC = $inputDir.$pathSeparator. $masterTOCFileName;
    my $tocFileName = "toc.html";
    my @allDocRefs;
    push(@allDocRefs, @headerFramesetRefs);
    push(@allDocRefs, @classFramesetRefs);
    push(@allDocRefs, @protocolFramesetRefs);
    push(@allDocRefs, @categoryFramesetRefs);
    my $localDebug = 0;
    
    foreach my $ref (@allDocRefs) {
        my $name = $ref->name();
        my $type = $ref->type();
        my $path = $ref->path();
        my $tocFile = $path;   				# path to index.html
        my $fsName = quotemeta($framesetFileName);
        $tocFile =~ s/$fsName$/toc.html/; 		# path to toc.html
        
        if (-e "$tocFile" ) {
            my $oldRecSep = $/;
            undef $/; # read in file as string
            open(INFILE, "<$tocFile") || die "Can't read file $tocFile.\n";
            my $fileString = <INFILE>;
            close INFILE;
            $/ = $oldRecSep;
            
            my $uniqueMarker = "headerDoc=\"topLink\"";
            if ($fileString !~ /$uniqueMarker/) { # we haven't been here before
                my $relPathToMasterTOC = &findRelativePath($tocFile, $masterTOC);
                my $topLink = "\n<font size=\"-2\"><a href=\"$relPathToMasterTOC\" target=\"_top\" $uniqueMarker>[Top]</a></font><br>\n";
                
                $fileString =~ s/(<body[^>]*>)/$1$topLink/i;
            
                open (OUTFILE, ">$tocFile") || die "Can't write file $tocFile.\n";
                print OUTFILE $fileString;
                close (OUTFILE);
            }
        } elsif ($debugging) {
            print "--> '$tocFile' doesn't exist!\n";
            print "Cannot add [top] link for frameset doc reference:\n";
            print "   name: $name\n";
            print "   type: $type\n";
            print "   path: $path\n";
        }
    }
}

sub getLinkToFramesetFrom {
    my $masterTOCFile = shift;
    my $dest = shift;    
    my $name = shift;    
    my $linkString;
    
    my $relPath = &findRelativePath($masterTOCFile, $dest);
    $linkString = "<a href=\"$relPath\" target =\"_top\">$name</a><br>\n"; 
    return $linkString;
}


sub objName { # for sorting objects by their names
    uc($a->name()) cmp uc($b->name());
}
