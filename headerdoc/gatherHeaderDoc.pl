#! /usr/bin/perl -w
#
# Script name: gatherHeaderDoc
# Synopsis: 	Finds all HeaderDoc generated docs in an input
#		folder and creates a top-level HTML page to them
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2004/11/29 23:40:28 $
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
# $Revision: 1.8.4.39 $
######################################################################
my $pathSeparator;
my $isMacOS;
my $uninstalledModulesPath;
my $has_resolver;

sub resolveLinks($);

BEGIN {
	use FindBin qw ($Bin);

    if ($^O =~ /MacOS/i) {
		$pathSeparator = ":";
		$isMacOS = 1;
		#$Bin seems to return a colon after the path on certain versions of MacPerl
		#if it's there we take it out. If not, leave it be
		#WD-rpw 05/09/02
		($uninstalledModulesPath = $FindBin::Bin) =~ s/([^:]*):$/$1/;
    } else {
		$pathSeparator = "/";
		$isMacOS = 0;
    }
    $uninstalledModulesPath = "$FindBin::Bin"."$pathSeparator"."Modules";
	
}

use strict;
use Cwd;
use File::Basename;
use File::Find;
use File::Copy;
use lib $uninstalledModulesPath;

$has_resolver = 1;
eval "use HeaderDoc::LinkResolver qw (resolveLinks); 1" || do { $has_resolver = 0; };
# print "HR: $has_resolver\n";
if ($has_resolver) {
	print "LinkResolver will be used to resolve cross-references.\n";
}

# Modules specific to gatherHeaderDoc
use HeaderDoc::DocReference;
use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash updateHashFromConfigFiles getHashFromConfigFile quote resolveLinks sanitize);
$HeaderDoc::modulesPath = $INC{'HeaderDoc/Utilities.pm'};
$HeaderDoc::modulesPath =~ s/Utilities.pm$//s;
# print "MP: ".$HeaderDoc::modulesPath."\n";

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
my $systemPreferencesPath;
#added WD-rpw 07/30/01 to support running on MacPerl
#modified WD-rpw 07/01/02 to support the MacPerl 5.8.0
if ($^O =~ /MacOS/i) {
	eval {
		require "FindFolder.pl";
		$homeDir = MacPerl::FindFolder("D");	#D = Desktop. Arbitrary place to put things
		$usersPreferencesPath = MacPerl::FindFolder("P");	#P = Preferences
	};
	if ($@) {
		import Mac::Files;
		$homeDir = Mac::Files::FindFolder(kOnSystemDisk(), kDesktopFolderType());
		$usersPreferencesPath = Mac::Files::FindFolder(kOnSystemDisk(), kPreferencesFolderType());
	}
	$systemPreferencesPath = $usersPreferencesPath;
} else {
	$homeDir = (getpwuid($<))[7];
	$usersPreferencesPath = $homeDir.$pathSeparator."Library".$pathSeparator."Preferences";
	$systemPreferencesPath = "/Library/Preferences";
}

my $CWD = getcwd();
my @configFiles = ($systemPreferencesPath.$pathSeparator.$preferencesConfigFileName, $usersPreferencesPath.$pathSeparator.$preferencesConfigFileName, $Bin.$pathSeparator.$localConfigFileName, $CWD.$pathSeparator.$localConfigFileName);

# ($Bin.$pathSeparator.$localConfigFileName, $usersPreferencesPath.$pathSeparator.$preferencesConfigFileName);

# default configuration, which will be modified by assignments found in config files.
# The default values listed in this hash must be the same as those in the identical 
# hash in headerDoc2HTML--so that links between the frameset and the masterTOC work.
my %config = (
    defaultFrameName => "index.html", 
    masterTOCName => "MasterTOC.html"
);

%config = &updateHashFromConfigFiles(\%config,\@configFiles);

my $framesetFileName;
my $masterTOCFileName;
my $bookxmlname = "";
my @TOCTemplateList = ();
my @TOCNames = ();
my $framework = "";
my $frameworkShortName = "";
my $landingPageUID = "";
my $stripDotH = 0;
my $gather_functions = 0;
my $gather_types = 0;
my $gather_globals_and_constants = 0;
my $gather_man_pages = 0;
my $apiUIDPrefix = "apple_ref";

if (defined $config{"defaultFrameName"}) {
	$framesetFileName = $config{"defaultFrameName"};
} 

if (defined $config{"apiUIDPrefix"}) {
    $apiUIDPrefix = $config{"apiUIDPrefix"};
}

if (defined $config{"masterTOCName"}) {
	$masterTOCFileName = $config{"masterTOCName"};
} 
if (defined $config{"stripDotH"}) {
	$stripDotH = $config{"stripDotH"};
} 
if (defined $config{"TOCTemplateFile"}) {
	my $TOCTemplateFile = $config{"TOCTemplateFile"};

	my $oldRecSep = $/;
	undef $/; # read in files as strings

	my @filelist = split(/\s/, $TOCTemplateFile);
	foreach my $file (@filelist) {
		print "Searching for $file\n";
		my @templateFiles = ($Bin.$pathSeparator.$file, $usersPreferencesPath.$pathSeparator.$file, $file);
		my $TOCTemplate = "";
		my $found = 0;

		foreach my $filename (@templateFiles) {
			if (open(TOCFILE, "<$filename")) {
				$TOCTemplate = <TOCFILE>;
				close(TOCFILE);
				$found = 1;
			}
		}
		if (!$found) {
			die("Template file $file not found.\n");
		}
		push(@TOCTemplateList, $TOCTemplate);
		push(@TOCNames, basename($file));

		if (!$gather_types && $TOCTemplate =~ /\$\$\s*typelist/) {
			$gather_types = 1;
		}
		if (!$gather_globals_and_constants && $TOCTemplate =~ /\$\$\s*datalist/) {
			$gather_globals_and_constants = 1;
		}
		if (!$gather_functions && $TOCTemplate =~ /\$\$\s*functionlist/) {
			$gather_functions = 1;
		}
		if (!$gather_man_pages && $TOCTemplate =~ /\$\$\s*manpagelist/) {
			$gather_man_pages = 1;
		}

	}
	$/ = $oldRecSep;
}

my $useBreadcrumbs = 0;

if (defined $config{"useBreadcrumbs"}) {
	$useBreadcrumbs = $config{"useBreadcrumbs"};
}

$GHD::bgcolor = "#ffffff";
if (!scalar(@TOCTemplateList)) {
	my $TOCTemplate = default_template();
	push(@TOCTemplateList, $TOCTemplate);
	push(@TOCNames, "masterTOC.html")
}

# print "GatherFunc: $gather_functions\n";
# print "TT: $TOCTemplate\n";

########################## Input Folder and Files #######################
my @inputFiles;
my @contentFiles;
my $inputDir;

if (($#ARGV == 0 || $#ARGV == 1 || $#ARGV == 2) && (-d $ARGV[0])) {
    $inputDir = $ARGV[0];

	if ($#ARGV) {
		$masterTOCFileName = $ARGV[1];
	}
	if ($#ARGV > 1) {
		$bookxmlname = $ARGV[2];
	}

	if ($^O =~ /MacOS/i) {
		find(\&getFiles, $inputDir);
		$inputDir =~ s/([^:]*):$/$1/;	#WD-rpw 07/01/02
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
    my $localDebug = 0;
    my $basePath = dirname($filePath);
    my $dirName = basename($basePath);
    
    print "$fileName ($filePath): " if ($localDebug);
    if ($fileName =~ /$framesetFileName/) {
	print "HTML frameset\n" if ($localDebug);
        push(@inputFiles, $filePath);
    } elsif ($dirName =~ /^(man|cat)[\w\d]+$/ && $gather_man_pages) {
	print "Man Page\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Constants\.html$/ && $gather_globals_and_constants) {
	print "Constants\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Vars\.html$/ && $gather_globals_and_constants) {
	print "Vars\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /DataTypes\.html$/ && $gather_types) {
	print "DataTypes\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Structs\.html$/ && $gather_types) {
	print "Structs\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Enums\.html$/ && $gather_types) {
	print "Enums\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Methods\.html$/ && $gather_functions) {
	print "Methods\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Functions\.html$/ && $gather_functions) {
	print "Functions\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /doc\.html$/) {
	print "Framework Documentation\n" if ($localDebug);
	# Framework (maybe)
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName !~ /toc\.html$/) {
	print "Other Content\n" if ($localDebug);
	# Don't push the TOCs.
        push(@contentFiles, $filePath);
    } else {
	print "toc.\n" if ($localDebug);
    }
}
########################## Find HeaderDoc Comments #######################
my @headerFramesetRefs;
my @dataFramesetRefs;
my @typeFramesetRefs;
my @classFramesetRefs;
my @manpageFramesetRefs;
my @categoryFramesetRefs;
my @protocolFramesetRefs;
my @functionRefs;

my $frameworkabstract = "";
my $frameworkdiscussion = "";

my $oldRecSep = $/;
undef $/; # read in files as strings

my $localDebug = 0;

my %groups = ();

$groups{" "}="";

$| = 1;
print "Processing...";
foreach my $file (@inputFiles) {
    open (INFILE, "<$file") || die "Can't open $file: $!\n";
    my $fileString = <INFILE>;
    close INFILE;
    my $fileStringCopy = $fileString;
    while ($fileStringCopy =~ s/<\!--\s+(headerDoc\s*=.*?)-->(.*)/$2/s) {
        my $fullComment = $1;
	my $tail = $2;
	my $inDiscussion = 0;
	my $inAbstract = 0;
        my @stockpairs = split(/;/, $fullComment);
	my @pairs = ();

	my $temp = "";
	foreach my $stockpair (@stockpairs) {
		if (length($temp)) {
			$temp .= $stockpair;
			if ($temp !~ /\\$/) {
				push(@pairs, $temp);
				$temp = "";
			}
		} else {
			if ($stockpair =~ /\\$/) {
				$temp = $stockpair;
			} else {
				push(@pairs, $stockpair);
				$temp = "";
			}
		}
	}

        my $docRef = HeaderDoc::DocReference->new;
        $docRef->path($file);
	# print "PATH: $file\n";
	print ".";
        foreach my $pair (@pairs) {
            my ($key, $value) = split(/=/, $pair, 2);
            $key =~ s/^\s+|\s+$//;
            $value =~ s/^\s+|\s+$//;
            SWITCH: {
		($key =~ /indexgroup/) && do
		    {
			my $group = $value;
			$group =~ s/^\s*//sg;
			$group =~ s/\s*$//sg;
			$group =~ s/\\;/;/sg;
			$docRef->group($group);
			$groups{$group}=1;
			# print "SAW $group\n";
		    };
                ($key =~ /headerDoc/) && 
                    do {
                        $docRef->type($value);
			if ($value =~ /frameworkdiscussion/) {
				$inDiscussion = 1;
			}
			if ($value =~ /frameworkabstract/) {
				$inAbstract = 1;
			}
                        last SWITCH;
                    };
		($key =~ /shortname/) &&
		    do {
			$docRef->shortname($value);
			last SWITCH;
		    };
                ($key =~ /uid/) &&
		    do {
			$docRef->uid($value);
		    };
                ($key =~ /name/) && 
                    do {
                        $docRef->name($value);
			if ($inDiscussion && $value =~ /start/) {
				$frameworkdiscussion = $tail;
				$frameworkdiscussion =~ s/<!--\s+headerDoc\s*.*//sg;
				# print "Discussion: $frameworkdiscussion\n";
			}
			if ($inAbstract && $value =~ /start/) {
				$frameworkabstract = $tail;
				$frameworkabstract =~ s/<!--\s+headerDoc\s*.*//sg;
				# print "Abstract: $frameworkabstract\n";
			}
                        last SWITCH;
                    };
            }
        }
        my $tmpType = $docRef->type();
        if ($tmpType eq "Header") {
            push (@headerFramesetRefs, $docRef);
        } elsif ($tmpType eq "data"){
            push (@dataFramesetRefs, $docRef);
        } elsif ($tmpType eq "tag"){
            push (@typeFramesetRefs, $docRef);
        } elsif ($tmpType eq "tdef"){
            push (@typeFramesetRefs, $docRef);
        } elsif ($tmpType eq "cl"){
            push (@classFramesetRefs, $docRef);
        } elsif ($tmpType eq "man"){
            push (@manpageFramesetRefs, $docRef);
        } elsif ($tmpType eq "intf"){
            push (@protocolFramesetRefs, $docRef);
        } elsif ($tmpType eq "cat"){
            push (@categoryFramesetRefs, $docRef);
	} elsif ($tmpType eq "func" || $tmpType eq "instm" ||
	         $tmpType eq "intfm" || $tmpType eq "clm" ||
	         $tmpType eq "ftmplt") {
	    push (@functionRefs, $docRef);
	} elsif ($tmpType eq "Framework") {
	    $framework = $docRef->name();
	    $frameworkShortName = $docRef->shortname();
	    $landingPageUID = "//$apiUIDPrefix/doc/framework/$frameworkShortName";
	} elsif ($tmpType eq "frameworkdiscussion" ||
	         $tmpType eq "frameworkabstract") {
	    if ($localDebug) {
		print "Discussion: $frameworkdiscussion\n";
		print "Abstract: $frameworkabstract\n";
	    }
        } else {
            my $tmpName = $docRef->name();
            my $tmpPath = $docRef->path();
            print "Unknown type '$tmpType' for document with name '$tmpName' and path '$tmpPath'\n";
        }
    }
}
$/ = $oldRecSep;
print "\ndone.\n";
$| = 0;

# foreach my $key (sort keys (%groups)) {
	# print "GROUPNAME: $key\n";
# }

# create master TOC if we have any framesets
if (scalar(@headerFramesetRefs) + scalar(@classFramesetRefs) + scalar(@manpageFramesetRefs) + scalar(@protocolFramesetRefs) + scalar(@categoryFramesetRefs) + scalar(@functionRefs)) {
    &printMasterTOC();
    &addTopLinkToFramesetTOCs();
    if ($has_resolver) {
	LinkResolver::resolveLinks($inputDir);
    } else {
	resolveLinks($inputDir);
    }
    if (length($bookxmlname)) {
	generate_book_xml("$inputDir/$bookxmlname");
    }
} else {
    print "gatherHeaderDoc.pl: No HeaderDoc framesets found--returning\n" if ($debugging); 
}
exit 0;

sub generate_book_xml
{
    my $filename = shift;

    my $text = "";
    $text .= "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    $text .= "<plist version=\"1.0\">\n";
    $text .= "<dict>\n";

    my $title = "$framework Documentation";
    my $landingPageUID = "//$apiUIDPrefix/doc/framework/$frameworkShortName";

    $text .= "<key>BookTitle</key>\n"; 
    $text .= "<string>$title</string>\n";
    $text .= "<key>AppleRefBookID</key>\n";
    $text .= "<string>$landingPageUID</string>\n";
    $text .= "<key>WriterEmail</key>\n";
    $text .= "<string>techpubs\@group.apple.com</string>\n";
    $text .= "<key>EDD_Name</key>\n";
    $text .= "<string>ProceduralC.EDD</string>\n";
    $text .= "<key>EDD_Version</key>\n";
    $text .= "<string>3.31</string>\n";
    $text .= "<key>ReleaseDateFooter</key>\n";
    my $date = `date +"%B %Y"`;
    $date =~ s/\n//smg;
    $text .= "<string>$date</string>\n";
    $text .= "</dict>\n";
    $text .= "</plist>\n";

    open(OUTFILE, ">$filename") || die("Could not write book.xml file.\n");
    print OUTFILE $text;
    close OUTFILE;

    $text = "";
    $text .= "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    $text .= "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    $text .= "<plist version=\"1.0\">\n";
    $text .= "<dict>\n";
    $text .= "<key>outputStyle</key>\n";
    $text .= "<string>default</string>\n";
    $text .= "</dict>\n";
    $text .= "</plist>\n";

    my $gfilename = dirname($filename)."/$frameworkShortName.gutenberg";

# print "FILENAME WAS $gfilename\n";

    open(OUTFILE, ">$gfilename") || die("Could not write gutenberg file.\n");
    print OUTFILE $text;
    close OUTFILE;

}

################### Print Navigation Page #######################
sub printMasterTOC {
    my $outputDir = $inputDir;
    my $masterTOC = $outputDir.$pathSeparator.$masterTOCFileName;
    my %headersLinkString= ();
    my %typeLinkString = ();
    my %dataLinkString = ();
    my %classesLinkString = ();
    my %manpagesLinkString = ();
    my %protocolsLinkString = ();
    my %categoriesLinkString = ();
    my %functionsLinkString = ();

    my $seenHeaders = 0;
    my $seenType = 0;
    my $seenData = 0;
    my $seenClasses = 0;
    my $seenManPages = 0;
    my $seenProtocols = 0;
    my $seenCategories = 0;
    my $seenFunctions = 0;

    my $localDebug = 0;

    foreach my $group (sort keys (%groups)) {
	$headersLinkString{$group} = "";
	$typeLinkString{$group} = "";
	$dataLinkString{$group} = "";
	$classesLinkString{$group} = "";
	$manpagesLinkString{$group} = "";
	$protocolsLinkString{$group} = "";
	$categoriesLinkString{$group} = "";
	$functionsLinkString{$group} = "";

    # get the HTML links to each header 
    foreach my $ref (sort objName @headerFramesetRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
	if ($ref->group() ne $group) {
		# print "GROUP \"".$ref->group()."\" ne \"$group\".\n";
		next;
	}

	if ($stripDotH) {
		$name =~ s/\.h$//;
	}

        my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name);
        $headersLinkString{$group} .= $tmpString; $seenHeaders = 1;
    }
    print "\$headersLinkString is '".$headersLinkString{$group}."'\n" if ($localDebug);
    
    # get the HTML links to each variable/constant 
    foreach my $ref (sort objName @dataFramesetRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
	if ($ref->group() ne $group) { next; }

        my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name);
        $dataLinkString{$group} .= $tmpString; $seenData = 1;
    }
    if (($localDebug) && length($dataLinkString{$group})) {print "\$dataLinkString is '".$dataLinkString{$group}."'\n";};
    
    # get the HTML links to each type 
    foreach my $ref (sort objName @typeFramesetRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
	if ($ref->group() ne $group) { next; }

        my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name);
        $typeLinkString{$group} .= $tmpString; $seenType = 1;
    }
    if (($localDebug) && length($typeLinkString{$group})) {print "\$typeLinkString is '".$typeLinkString{$group}."'\n";};
    
    # get the HTML links to each man page 
    foreach my $ref (sort objName @manpageFramesetRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
	if ($ref->group() ne $group) { next; }

        my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name);
        $manpagesLinkString{$group} .= $tmpString; $seenManPages = 1;
    }
    if (($localDebug) && length($manpagesLinkString{$group})) {print "\$manpagesLinkString is '".$manpagesLinkString{$group}."'\n";};
    
    # get the HTML links to each class 
    foreach my $ref (sort objName @classFramesetRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
	if ($ref->group() ne $group) { next; }

        my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name);
        $classesLinkString{$group} .= $tmpString; $seenClasses = 1;
    }
    if (($localDebug) && length($classesLinkString{$group})) {print "\$classesLinkString is '".$classesLinkString{$group}."'\n";};
    
    # get the HTML links to each protocol 
    foreach my $ref (sort objName @protocolFramesetRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
	if ($ref->group() ne $group) { next; }

        my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name);
        $protocolsLinkString{$group} .= $tmpString; $seenProtocols = 1;
    }
    if (($localDebug) && length($protocolsLinkString{$group})) {print "\$protocolsLinkString is '".$protocolsLinkString{$group}."'\n";};
    
    # get the HTML links to each category 
    foreach my $ref (sort objName @categoryFramesetRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
	if ($ref->group() ne $group) { next; }

        my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name);
        $categoriesLinkString{$group} .= $tmpString; $seenCategories = 1;
    }
    if (($localDebug) && length($categoriesLinkString{$group})) {print "\$categoriesLinkString is '".$categoriesLinkString{$group}."'\n";};
    
    # get the HTML links to each function 
    foreach my $ref (sort objName @functionRefs) {
        my $name = $ref->name();
        my $path = $ref->path();        
	my $uid = $ref->uid();
	if ($ref->group() ne $group) { next; }

        my $tmpString = &getLinkToFunctionFrom($masterTOC, $path, $name, $uid);
        $functionsLinkString{$group} .= $tmpString; $seenFunctions = 1;
    }
    if (($localDebug) && length($functionsLinkString{$group})) {print "\$functionsLinkString is '".$functionsLinkString{$group}."'\n";};
  }

    my $template_number = 0;
    foreach my $TOCTemplate (@TOCTemplateList) {
      my @templatefields = split(/\$\$/, $TOCTemplate);
      my $templatefilename = $TOCNames[$template_number];
      my $templatename = $templatefilename;

      my $localDebug = 0;
      if ($localDebug) {
	print "processing # $template_number\n";
        print "NAME WAS $templatefilename\n";
      }

      $templatename =~ s/^\s*//s;
      $templatename =~ s/\s*$//s;
      $templatename =~ s/\.html$//s;
      $templatename =~ s/\.tmpl$//s;

      my $title = "$framework Documentation";

      my $include_in_output = 1; my $first = 1; my $out = "";
      foreach my $field (@templatefields) {
	my $keep = $field;
	SWITCH: {
		($field =~ /^\s*title/) && do {

			if ($include_in_output) {
				$out .= $title;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*frameworkdiscussion/) && do {

			if ($include_in_output) {
				$out .= $frameworkdiscussion;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*frameworkabstract/) && do {

			if ($include_in_output) {
				$out .= $frameworkabstract;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*tocname/) && do {
			my $tn = basename($masterTOC);

			if ($include_in_output) {
				$out .= "$tn";
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*frameworkdir/) && do {

			if ($include_in_output) {
				$out .= "$frameworkShortName";
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*frameworkuid/) && do {

			if ($include_in_output) {
				$out .= "<a name=\"$landingPageUID\" title=\"$framework\"></a>";
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*framework/) && do {

			if ($include_in_output) {
				$out .= "$framework";
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*headersection/) && do {

			if (!$seenHeaders) {
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/headersection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*headerlist/) && do {

			if ($seenHeaders) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($headersLinkString{$group}, $field, $group);
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*manpagesection/) && do {

			if (!$seenManPages) {
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/manpagesection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*manpagelist/) && do {

			if ($seenManPages) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($manpagesLinkString{$group}, $field, $group);
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*classsection/) && do {

			if (!$seenClasses) {
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/classsection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*classlist/) && do {

			if ($seenClasses) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($classesLinkString{$group}, $field, $group);
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*categorysection/) && do {

			if (!$seenCategories) { # @@@
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/categorysection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*categorylist/) && do {

			if ($seenCategories) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($categoriesLinkString{$group}, $field, $group);
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*protocolsection/) && do {

			if (!$seenProtocols) { # @@@
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/protocolsection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*protocollist/) && do {

			if ($seenProtocols) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($protocolsLinkString{$group}, $field, $group);
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*datasection/) && do {

			if (!$seenData) { # @@@
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/datasection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*datalist/) && do {

			if ($seenData) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($dataLinkString{$group}, $field, $group);
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*typesection/) && do {

			if (!$seenType) { # @@@
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/typesection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*typelist/) && do {

			if ($seenType) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($typeLinkString{$group}, $field, $group);
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*functionsection/) && do {

			if (!$seenFunctions) { # @@@
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/functionsection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*functionlist/) && do {

			if ($seenFunctions) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($functionsLinkString{$group}, $field, $group);
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		{
			if ($first) { $first = 0; }
			else { warn "Unknown field: \$\$$field\n"; $out .= "\$\$"; }
		}
	}
	if ($include_in_output) { $out .= $keep; }
      }

      # print "HTML: $out\n";

      # write out page
      print "gatherHeaderDoc.pl: writing master TOC to $masterTOC\n" if ($localDebug);
      if (!$template_number) {
	open(OUTFILE, ">$masterTOC") || die "Can't write $masterTOC.\n";
      } else {
	open(OUTFILE, ">$outputDir$pathSeparator$frameworkShortName-$templatename.html") || die "Can't write $outputDir$pathSeparator$frameworkShortName-$templatename.html.\n";
      }
      print OUTFILE $out;
      close OUTFILE;
      ++$template_number;
    }
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
            

	    if (!$useBreadcrumbs) {
                my $uniqueMarker = "headerDoc=\"topLink\"";
                if ($fileString !~ /$uniqueMarker/) { # we haven't been here before
                    my $relPathToMasterTOC = &findRelativePath($tocFile, $masterTOC);
                    my $topLink = "\n<font size=\"-2\"><a href=\"$relPathToMasterTOC\" target=\"_top\" $uniqueMarker>[Top]</a></font><br>\n";
                    
                    $fileString =~ s/(<body[^>]*>)/$1$topLink/i;
                
                    open (OUTFILE, ">$tocFile") || die "Can't write file $tocFile.\n";
                    print OUTFILE $fileString;
                    close (OUTFILE);
                }
	    }
        } elsif ($debugging) {
            print "--> '$tocFile' doesn't exist!\n";
            print "Cannot add [top] link for frameset doc reference:\n";
            print "   name: $name\n";
            print "   type: $type\n";
            print "   path: $path\n";
        }
    }
    if ($useBreadcrumbs) {
	foreach my $file (@contentFiles) {
	    # print "FILE: $file\n";
            if (-e "$file" && ! -d "$file" ) {
                my $oldRecSep = $/;
                undef $/; # read in file as string
		open(INFILE, "<$file") || die "Can't read file $file.\n";
		my $fileString = <INFILE>;
		close INFILE;
                $/ = $oldRecSep;

		my $uniqueMarker = "headerDoc=\"topLink\"";

		# if ($fileString !~ /$uniqueMarker/) { # we haven't been here before
		if (length($framework)) {
                    my $relPathToMasterTOC = &findRelativePath($file, $masterTOC);
                    my $breadcrumb = "<a href=\"$relPathToMasterTOC\" target=\"_top\" $uniqueMarker>$framework</a>";

                    $fileString =~ s/<!-- begin breadcrumb -->.*?<!-- end breadcrumb -->/<!-- begin breadcrumb -->$breadcrumb<!-- end breadcrumb -->/i;
                
                    open (OUTFILE, ">$file") || die "Can't write file $file.\n";
                    print OUTFILE $fileString;
                    close (OUTFILE);
		} else {
			warn "No framework (.hdoc) file found and breadcrumb specified.  Breadcrumbs will\nnot be inserted.\n";
		}
		# }
	    }
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

sub getLinkToFunctionFrom {
    my $masterTOCFile = shift;
    my $dest = shift;    
    my $name = shift;    
    my $uid = shift;
    my $linkString;
    
    my $relPath = &findRelativePath($masterTOCFile, $dest);
    my $noClassName = $name;
    $noClassName =~ s/.*\:\://s;
    my $urlname = sanitize($noClassName);
    my $lp = "";
    if ($uid && length($uid)) {
	$urlname = $uid;
	$lp = " logicalPath=\"$uid\"";
    }
    $linkString = "<a $lp href=\"$relPath#$urlname\" retarget=\"yes\" target =\"_top\">$name</a><br>\n"; 
    return $linkString;
}


sub objName { # for sorting objects by their names
    uc($a->name()) cmp uc($b->name());
}

sub default_template
{
    my $template = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n    \"http://www.w3.org/TR/1998/REC-html40-19980424/loose.dtd\">\n";
    $template .= "<html>\n<head>\n    <title>\$\$title\@\@</title>\n	<meta name=\"generator\" content=\"HeaderDoc\">\n</head>\n<body bgcolor=\"$GHD::bgcolor\"><h1>\$\$framework\@\@ Documentation</h1><hr><br>\n";
    $template .= "<p>\$\$frameworkdiscussion\@\@</p>";
    $template .= "\$\$headersection\@\@<h2>Headers</h2>\n<blockquote>\n\$\$headerlist cols=2 order=down atts=border=0 width=\"80%\"\@\@\n</blockquote>\$\$/headersection\@\@\n";
    $template .= "\$\$classsection\@\@<h2>Classes</h2>\n<blockquote>\n\$\$classlist cols=2 order=down atts=border=0 width=\"80%\"\@\@\n</blockquote>\$\$/classsection\@\@\n";
    $template .= "\$\$categorysection\@\@<h2>Categories</h2>\n<blockquote>\n\$\$categorylist cols=2 order=down atts=border=0 width=\"80%\"\@\@\n</blockquote>\$\$/categorysection\@\@\n";
    $template .= "\$\$protocolsection\@\@<h2>Protocols</h2>\n<blockquote>\n\$\$protocollist cols=2 order=down atts=border=0 width=\"80%\"\@\@\n</blockquote>\$\$/protocolsection\@\@\n";
    $template .= "\$\$functionsection\@\@<h2>Functions</h2>\n<blockquote>\n\$\$functionlist cols=2 order=down atts=border=0 width=\"80%\"\@\@\n</blockquote>\$\$/functionsection\@\@\n";
    $template .= "\$\$typesection\@\@<h2>Data Types</h2>\n<blockquote>\n\$\$typelist cols=2 order=down atts=border=0 width=\"80%\"\@\@\n</blockquote>\$\$/typesection\@\@\n";
    $template .= "\$\$datasection\@\@<h2>Globals and Constants</h2>\n<blockquote>\n\$\$datalist cols=2 order=down atts=border=0 width=\"80%\"\@\@\n</blockquote>\$\$/datasection\@\@\n";
    $template .= "</body>\n</html>\n";

    return $template;
}

sub genTable
{
    my $inputstring = shift;
    my $settings = shift;
    my $groupname = shift;
    my $ncols = 0;
    my $order = "down";
    my $attributes = "border=0 width=\"100%\"";
    my $localDebug = 0;

    my $ngroups = scalar(keys(%groups));

    my $groupnamestring = "";
    if ($groupname =~ /\S/) {
	$groupnamestring = "<p><i>$groupname</i></p>\n";
    }
    my $groupheadstring = "<blockquote>\n";
    my $grouptailstring = "</blockquote>\n";
    if (!$ngroups) {
	$groupheadstring = "";
	$grouptailstring = "";
    }

    $settings =~ s/^\s*(\w+list)\s*//;
    my $name = $1;

    if ($settings =~ s/^cols=(\d+)\s+//) {
	$ncols = $1;
    }
    if ($settings =~ s/^order=(\w+)\s+//) {
	$order = $1;
	if (!$ncols) { $ncols = 1; }
    }
    if ($settings =~ s/^atts=(\w+)\s+//) {
	$attributes = $1;
	if (!$ncols) { $ncols = 1; }
    }

    if ($ncols) {
	my @lines = split(/\n/, $inputstring);
	my $nlines = scalar(@lines);

	if (!$nlines) { return ""; }

	my @columns = ();
	my $loopindex = $ncols;
	while ($loopindex--) {
		my @column = ();
		push(@columns, \@column);
	}

	my $curcolumn = 0; my $curline = 0;

	my $lines_per_column = int(($nlines + $ncols - 1) / $ncols);
			# ceil(nlines/ncols)
	my $blanks = ($lines_per_column * $ncols) - $nlines;
	$nlines += $blanks;
	while ($blanks) {
		push(@lines, "");
		$blanks--;
	}

	warn "NLINES: $nlines\n" if ($localDebug);
	warn "Lines per column: $lines_per_column\n" if ($localDebug);

	foreach my $line (@lines) {
		warn "columns[$curcolumn] : adding line\n" if ($localDebug);
		my $columnref = $columns[$curcolumn];

		push(@{$columnref}, $line);

		$curline++;

		if ($order eq "across") {
			$curcolumn = ($curcolumn + 1) % $ncols;
		} elsif ($curline >= $lines_per_column) {
			$curline = 0; $curcolumn++;
		}
	}

	if ($localDebug) {
	    $loopindex = 0;
	    while ($loopindex < $ncols) {
		warn "Column ".$loopindex.":\n";
		foreach my $line (@{$columns[$loopindex]}) {
			warn "$line\n";
		}
		$loopindex++;
	    }
	}

	my $outstring = "<table $attributes>";

	$curline = 0;
	$curcolumn = 0;
	my $currow = 0;
	my $first = 1;

	while ($curline < $nlines) {
		if (!$curcolumn) {
			if ($first) { $first = 0; $outstring .= "<tr>"; }
			else { $outstring .= "</tr><tr>\n"; $currow++; }
		}

		my $line = ${$columns[$curcolumn]}[$currow];
		$outstring .= "<td>$line</td>\n";

		$curline++;
		$curcolumn = ($curcolumn + 1) % $ncols;
	}
	$outstring .= "</tr></table>\n";

	return $groupnamestring.$groupheadstring.$outstring.$grouptailstring;
    } else {
	return $groupnamestring.$groupheadstring.$inputstring.$grouptailstring;
    }
}

