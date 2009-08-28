#! /usr/bin/perl -w
#
# Script name: gatherHeaderDoc
# Synopsis: 	Finds all HeaderDoc generated docs in an input
#		folder and creates a top-level HTML page to them
#
# Last Updated: $Date: 2009/04/17 23:21:58 $
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
# $Revision: 1.18 $
######################################################################
my $pathSeparator;
my $isMacOS;
my $uninstalledModulesPath;
my $has_resolver;

sub resolveLinks($$$);

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

    $HeaderDoc::use_styles = 0;
}

use strict;
# use Cwd;
use File::Basename;
use File::Find;
use File::Copy;
# use Carp qw(cluck);
use lib $uninstalledModulesPath;
use POSIX;

my $skipTOC = 0;
my $generateDocSet = 0;

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

########################## Get command line arguments and flags #######################
my @inputFiles;
my @contentFiles;
my $inputDir;
my $externalXRefFiles = "";

use Getopt::Std;
my %options = ();
my %letters_linked = ();
my %group_letters_linked = ();
getopts("c:dnx:",\%options);

# The options are handled after processing config file so they can
# override behavior.  However, we need to handle the options first
# before checking for input file names (which we should do first
# to avoid wasting a lot of time before telling the user he/she
# did something wrong).

my $masterTOCFileName = "";
my $bookxmlname = "";

if (($#ARGV == 0 || $#ARGV == 1 || $#ARGV == 2) && (-d $ARGV[0])) {
    $inputDir = $ARGV[0];

	if ($#ARGV) {
		$masterTOCFileName = $ARGV[1];
	}
	if ($#ARGV > 1) {
		$bookxmlname = $ARGV[2];
	}

} else {
    die "You must specify a single input directory for processing.\n";
}

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
    masterTOCName => "MasterTOC.html",
    groupHierLimit => 0,
    groupHierSubgroupLimit => 0
);

if ($options{c}) {
	@configFiles = ( $options{c} );
}

%config = &updateHashFromConfigFiles(\%config,\@configFiles);

my $framesetFileName;
my @TOCTemplateList = ();
my @TOCNames = ();
my $framework = "";
my $frameworknestlevel = -1;
my $frameworkShortName = "";
my $frameworkpath = "";
my $frameworkrelated = "";
my $frameworkUID = "";
my $frameworkCopyrightString = "";

my $landingPageUID = "";
my $landingPageFrameworkUID = "";
my $stripDotH = 0;
my $gather_functions = 0;
my $gather_types = 0;
my $gather_properties = 0;
my $gather_globals_and_constants = 0;
my $gather_man_pages = 0;
my $apiUIDPrefix = "apple_ref";
my $compositePageName = "CompositePage.html";
my $classAsComposite = 0;
my $externalAPIUIDPrefixes = "";
my %usedInTemplate = ();

if (defined $config{"dateFormat"}) {
    $HeaderDoc::datefmt = $config{"dateFormat"};
    if ($HeaderDoc::datefmt !~ /\S/) {
	$HeaderDoc::datefmt = "%B %d, %Y";
    }
} else {
    $HeaderDoc::datefmt = "%B %d, %Y";
}


use HeaderDoc::APIOwner;

HeaderDoc::APIOwner->fix_date();

my ($sec,$min,$hour,$mday,$mon,$yr,$wday,$yday,$isdst) = localtime(time());
my $yearStamp = strftime("%Y", $sec, $min, $hour,
	$mday, $mon, $yr, $wday, $yday, $isdst);
my $dateStamp = HeaderDoc::HeaderElement::strdate($mon, $mday, $yr + 1900);

if (defined $config{"styleImports"}) {
    $HeaderDoc::styleImports = $config{"styleImports"};
    $HeaderDoc::styleImports =~ s/[\n\r]/ /sgo;
    $HeaderDoc::use_styles = 1;
}

if (defined $config{"groupHierLimit"}) {
    $HeaderDoc::groupHierLimit = $config{"groupHierLimit"};
}
if (defined $config{"groupHierSubgroupLimit"}) {
    $HeaderDoc::groupHierSubgroupLimit = $config{"groupHierSubgroupLimit"};
}

if (defined $config{"tocStyleImports"}) {
    $HeaderDoc::tocStyleImports = $config{"tocStyleImports"};
    $HeaderDoc::tocStyleImports =~ s/[\n\r]/ /sgo;
    $HeaderDoc::use_styles = 1;
}

if (defined $config{"textStyle"}) {
	HeaderDoc::APIOwner->setStyle("text", $config{"textStyle"});
}

if (defined $config{"copyrightOwner"}) {
	$HeaderDoc::copyrightOwner = $config{"copyrightOwner"};
}

if (defined $config{"commentStyle"}) {
	HeaderDoc::APIOwner->setStyle("comment", $config{"commentStyle"});
}

if (defined $config{"preprocessorStyle"}) {
	HeaderDoc::APIOwner->setStyle("preprocessor", $config{"preprocessorStyle"});
}

if (defined $config{"funcNameStyle"}) {
	HeaderDoc::APIOwner->setStyle("function", $config{"funcNameStyle"});
}

if (defined $config{"stringStyle"}) {
	HeaderDoc::APIOwner->setStyle("string", $config{"stringStyle"});
}

if (defined $config{"charStyle"}) {
	HeaderDoc::APIOwner->setStyle("char", $config{"charStyle"});
}

if (defined $config{"numberStyle"}) {
	HeaderDoc::APIOwner->setStyle("number", $config{"numberStyle"});
}

if (defined $config{"keywordStyle"}) {
	HeaderDoc::APIOwner->setStyle("keyword", $config{"keywordStyle"});
}

if (defined $config{"typeStyle"}) {
	HeaderDoc::APIOwner->setStyle("type", $config{"typeStyle"});
}

if (defined $config{"paramStyle"}) {
	HeaderDoc::APIOwner->setStyle("param", $config{"paramStyle"});
}

if (defined $config{"varStyle"}) {
	HeaderDoc::APIOwner->setStyle("var", $config{"varStyle"});
}

if (defined $config{"templateStyle"}) {
	HeaderDoc::APIOwner->setStyle("template", $config{"templateStyle"});
}

if (defined $config{"externalXRefFiles"}) {
	$externalXRefFiles = $config{"externalXRefFiles"};
}

if (defined $config{"externalAPIUIDPrefixes"}) {
	$externalAPIUIDPrefixes = $config{"externalAPIUIDPrefixes"};
}

if (defined $config{"defaultFrameName"}) {
	$framesetFileName = $config{"defaultFrameName"};
} 

if (defined $config{"apiUIDPrefix"}) {
    $apiUIDPrefix = $config{"apiUIDPrefix"};
}

if (defined $config{"compositePageName"}) {
	$compositePageName = $config{"compositePageName"};
}

if (defined $config{"classAsComposite"}) {
	$classAsComposite = $config{"classAsComposite"};
	$classAsComposite =~ s/\s*//;
} else {
	$classAsComposite = 0;
}

if (defined $config{"masterTOCName"} && $masterTOCFileName eq "") {
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
		my %used = ();

		print "Searching for $file\n";
		my @templateFiles = ($systemPreferencesPath.$pathSeparator.$file, $usersPreferencesPath.$pathSeparator.$file, $Bin.$pathSeparator.$file, $file);
		my $TOCTemplate = "";
		my $found = 0;
		my $foundpath = "";

		foreach my $filename (@templateFiles) {
			if (open(TOCFILE, "<$filename")) {
				$TOCTemplate = <TOCFILE>;
				close(TOCFILE);
				$found = 1;
				$foundpath = $filename;
			}
		}
		if (!$found) {
			die("Template file $file not found.\n");
		} else {
			print "Found at $foundpath\n";
		}
		push(@TOCTemplateList, $TOCTemplate);
		push(@TOCNames, basename($file));

		if ($TOCTemplate =~ /\$\$\s*typelist/) {
			$gather_types = 1;
			$used{type} = 1;
		}
		if ($TOCTemplate =~ /\$\$\s*proplist/) {
			$gather_properties = 1;
			$used{prop} = 1;
		}
		if ($TOCTemplate =~ /\$\$\s*datalist/) {
			$gather_globals_and_constants = 1;
			$used{data} = 1;
		}
		if ($TOCTemplate =~ /\$\$\s*functionlist/) {
			$gather_functions = 1;
			$used{function} = 1;
		}
		if ($TOCTemplate =~ /\$\$\s*manpagelist/) {
			$gather_man_pages = 1;
			$used{manpage} = 1;
		}

		if ($TOCTemplate =~ /\$\$\s*headerlist/) {
			$used{header} = 1;
		}
		if ($TOCTemplate =~ /\$\$\s*macrolist/) {
			$used{macro} = 1;
		}
		if ($TOCTemplate =~ /\$\$\s*protocollist/) {
			$used{protocol} = 1;
		}
		if ($TOCTemplate =~ /\$\$\s*categorylist/) {
			$used{category} = 1;
		}
		if ($TOCTemplate =~ /\$\$\s*classlist/) {
			$used{class} = 1;
		}
		if ($TOCTemplate =~ /\$\$\s*comintlist/) {
			$used{comint} = 1;
		}
		$usedInTemplate{$TOCTemplate} = \%used;
	}
	$/ = $oldRecSep;
}

my $useBreadcrumbs = 0;

if (defined $config{"useBreadcrumbs"}) {
	$useBreadcrumbs = $config{"useBreadcrumbs"};
}


########################## Handle command line flags #######################
if ($options{d}) {
    $generateDocSet = 1;
    if ($options{n}) {
	$skipTOC = 1;
    }
}
if ($options{x}) {
    $externalXRefFiles = $options{x};
}

########################## Input Folder and Files #######################

	if ($^O =~ /MacOS/i) {
		find(\&getFiles, $inputDir);
		$inputDir =~ s/([^:]*):$/$1/;	#WD-rpw 07/01/02
	} else {
		$inputDir =~ s|(.*)/$|$1|; # get rid of trailing slash, if any
		if ($inputDir !~ /^\//) { # not absolute path -- !!! should check for ~
			my $cwd = getcwd();
			$inputDir = $cwd.$pathSeparator.$inputDir;
		}
		&find({wanted => \&getFiles, follow => 1}, $inputDir);
	}
unless (@inputFiles) { print STDERR "No valid input files specified. \n\n"; exit(-1)};


$GHD::bgcolor = "#ffffff";
if (!scalar(@TOCTemplateList)) {
	my $TOCTemplate = default_template();
	push(@TOCTemplateList, $TOCTemplate);
	push(@TOCNames, "masterTOC.html")
}

# print "GatherFunc: $gather_functions\n";
# print "TT: $TOCTemplate\n";


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
        push(@contentFiles, $filePath);
    } elsif ($dirName =~ /^(man|cat)[\w\d]+$/ && $gather_man_pages) {
	print "Man Page\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Constants\.html$/ && $gather_globals_and_constants && !$classAsComposite) {
	print "Constants\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Vars\.html$/ && $gather_globals_and_constants && !$classAsComposite) {
	print "Vars\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /DataTypes\.html$/ && $gather_types && !$classAsComposite) {
	print "DataTypes\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Structs\.html$/ && $gather_types && !$classAsComposite) {
	print "Structs\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Enums\.html$/ && $gather_types && !$classAsComposite) {
	print "Enums\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Methods\.html$/ && $gather_functions && !$classAsComposite) {
	print "Methods\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /Functions\.html$/ && $gather_functions && !$classAsComposite) {
	print "Functions\n" if ($localDebug);
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName =~ /doc\.html$/ && !$classAsComposite) {
	print "Framework Documentation\n" if ($localDebug);
	# Framework (maybe)
        push(@inputFiles, $filePath);
        push(@contentFiles, $filePath);
    } elsif ($fileName !~ /toc\.html$/) {
	print "Other Content\n" if ($localDebug);
	# Don't push the TOCs.
	if ($classAsComposite && $fileName =~ /\Q$compositePageName\E$/) {
        	push(@inputFiles, $filePath);
	}
        push(@contentFiles, $filePath);
    } else {
	print "toc.\n" if ($localDebug);
    }
}
########################## Find HeaderDoc Comments #######################
my @fileRefSets;
my @headerFramesetRefs;
my @propFramesetRefs;
my @dataFramesetRefs;
my @macroFramesetRefs;
my @typeFramesetRefs;
my @comintFramesetRefs;
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
  my @perFileDocRefs = ();
  if (-f $file) {
    open (INFILE, "<$file") || die "Can't open $file: $!\n";
    my $fileString = <INFILE>;
    close INFILE;
    my $fileStringCopy = $fileString;
    while ($fileStringCopy =~ s/<\!--\s+(headerDoc\s*=.*?)-->(.*)/$2/s) {
        my $fullComment = $1;
	my $tail = $2;
	my $inDiscussion = 0;
	my $inAbstract = 0;
	my $inPath = 0;
	my $inRelated = 0;
	my $inFWUID = 0;
	my $inFWCopyright = 0;
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
				$temp =~ s/\\$/;/s;
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
			    if (rightframework($file)) {
				$inDiscussion = 1;
			    }
			}
			if ($value =~ /frameworkabstract/) {
			    if (rightframework($file)) {
				$inAbstract = 1;
			    }
			}
			if ($value =~ /frameworkpath/) {
			    if (rightframework($file)) {
				$inPath = 1;
			    }
			}
			if ($value =~ /frameworkrelated/) {
			    if (rightframework($file)) {
				$inRelated = 1;
			    }
			}
			if ($value =~ /frameworkuid/) {
				# print "FWUID DETECTED ($value)\n";
				if (rightframework($file)) {
					$inFWUID = 1;
					# print "RIGHT FILE\n";
					# $frameworkUID = $value;
					# $frameworkUID =~ s/^\s*//sg;
					# $frameworkUID =~ s/\s*$//sg;
				}
		    	};
			if ($value =~ /frameworkcopyright/) {
				# print "FWCopyright DETECTED ($value)\n";
				if (rightframework($file)) {
					$inFWCopyright = 1;
					# print "RIGHT FILE\n";
					# $frameworkUID = $value;
					# $frameworkUID =~ s/^\s*//sg;
					# $frameworkUID =~ s/\s*$//sg;
				}
		    	};
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
			} elsif ($inDiscussion && $value =~ /end/) {
				$inDiscussion = 0;
			}
			if ($inAbstract && $value =~ /start/) {
				$frameworkabstract = $tail;
				$frameworkabstract =~ s/<!--\s+headerDoc\s*.*//sg;
				# print "Abstract: $frameworkabstract\n";
			} elsif ($inAbstract && $value =~ /end/) {
				$inAbstract = 0;
			}
			if ($inRelated && $value =~ /start/) {
				$frameworkrelated = $tail;
				$frameworkrelated =~ s/<!--\s+headerDoc\s*.*//sg;
				$frameworkrelated =~ s/^\s*//sg;
				$frameworkrelated =~ s/\s*$//sg;
				# print "Related Docs: $frameworkrelated\n";
			} elsif ($inRelated && $value =~ /end/) {
				$inRelated = 0;
			}
			if ($inFWUID && $value =~ /start/) {
				$frameworkUID = $tail;

				# Strip off the closing marker and what follows.
				$frameworkUID =~ s/<!--\s+headerDoc\s*.*//sg;
				$frameworkUID =~ s/^\s*//sg;
				$frameworkUID =~ s/\s*$//sg;
				# print "Got UID: $frameworkUID\n";

				if ($frameworkUID =~ /\S/) {
					$landingPageFrameworkUID = "//$apiUIDPrefix/doc/uid/$frameworkUID";
				}
			} elsif ($inFWUID && $value =~ /end/) {
				$inFWUID = 0;
			}
			if ($inFWCopyright && $value =~ /start/) {
				$frameworkCopyrightString = $tail;

				# Strip off the closing marker and what follows.
				$frameworkCopyrightString =~ s/<!--\s+headerDoc\s*.*//sg;
				$frameworkCopyrightString =~ s/^\s*//sg;
				$frameworkCopyrightString =~ s/\s*$//sg;
				# print "Got CopyrightString: $frameworkCopyrightString\n";
			} elsif ($inFWCopyright && $value =~ /end/) {
				$inFWCopyright = 0;
			}
			if ($inPath && $value =~ /start/) {
				$frameworkpath = $tail;
				$frameworkpath =~ s/<!--\s+headerDoc\s*.*//sg;
				$frameworkpath =~ s/^\s*//sg;
				$frameworkpath =~ s/\s*$//sg;
				# print "Abstract: $frameworkabstract\n";
			} elsif ($inPath && $value =~ /end/) {
				$inPath = 0;
			}
                        last SWITCH;
                    };
            }
        }
        my $tmpType = $docRef->type();
	my $isTitle = 0;
	if ($tmpType =~ /^title:(.*)$/s) {
		$tmpType = $1;
		$isTitle = 1; # for future use.
	}
	if (!$docRef->uid()) {
		print "REF UID BLANK FOR : ".$docRef->name()." in file $file\n";
	}
        if ($tmpType eq "Header") {
            push (@headerFramesetRefs, $docRef);
        } elsif ($tmpType eq "instp"){
            push (@propFramesetRefs, $docRef);
        } elsif ($tmpType eq "clconst"){
            push (@dataFramesetRefs, $docRef);
        } elsif ($tmpType eq "data"){
            push (@dataFramesetRefs, $docRef);
        } elsif ($tmpType eq "tag"){
            push (@typeFramesetRefs, $docRef);
        } elsif ($tmpType eq "tdef"){
            push (@typeFramesetRefs, $docRef);
        } elsif ($tmpType eq "com"){
            push (@comintFramesetRefs, $docRef);
        } elsif ($tmpType eq "cl"){
            push (@classFramesetRefs, $docRef);
        } elsif ($tmpType eq "man"){
            push (@manpageFramesetRefs, $docRef);
        } elsif ($tmpType eq "intf"){
            push (@protocolFramesetRefs, $docRef);
        } elsif ($tmpType eq "macro"){
            push (@macroFramesetRefs, $docRef);
        } elsif ($tmpType eq "cat"){
            push (@categoryFramesetRefs, $docRef);
	} elsif ($tmpType eq "func" || $tmpType eq "instm" ||
	         $tmpType eq "intfm" || $tmpType eq "clm" ||
	         $tmpType eq "ftmplt") {
		if (!$isTitle) {
			push (@functionRefs, $docRef);
		}
	} elsif ($tmpType eq "Framework" || $tmpType eq "framework") {
	    if (rightframework($file)) {
		$framework = $docRef->name();
		$frameworkShortName = $docRef->shortname();
		$landingPageUID = "//$apiUIDPrefix/doc/framework/$frameworkShortName";
		# print "FWUID IS \"$frameworkUID\"\n";
		if ($frameworkUID =~ /\S/) {
			$landingPageFrameworkUID = "//$apiUIDPrefix/doc/uid/$frameworkUID";
		}
	    }
	} elsif ($tmpType eq "frameworkdiscussion" ||
	         $tmpType eq "frameworkabstract" ||
	         $tmpType eq "frameworkuid" ||
	         $tmpType eq "frameworkcopyright" ||
	         $tmpType eq "frameworkpath" ||
		 $tmpType eq "frameworkrelated") {
	    if ($localDebug) {
		print "Discussion: $frameworkdiscussion\n";
		print "Abstract: $frameworkabstract\n";
		print "UID: $frameworkUID\n";
		print "Copyright: $frameworkCopyrightString\n";
		print "Path: $frameworkpath\n";
		print "Related: $frameworkrelated\n";
	    }
	} elsif ($tmpType eq "inheritedContent") {
		print "Inherited content: ".$docRef->name()."\n" if ($localDebug);
        } elsif ($tmpType eq "econst") {
		print "Embedded constant: ".$docRef->name()."\n" if ($localDebug);
		push (@dataFramesetRefs, $docRef);
        } else {
            my $tmpName = $docRef->name();
            my $tmpPath = $docRef->path();
            print "Unknown type '$tmpType' for document with name '$tmpName' and path '$tmpPath'\n";
        }
	if ($docRef->uid()) {
		push(@perFileDocRefs, $docRef);
	}
    }
  }
  my $docRef = HeaderDoc::DocReference->new;
  $docRef->name($file);
  $docRef->path($file);
  $docRef->group( \@perFileDocRefs );
  push(@fileRefSets, $docRef);
}
$/ = $oldRecSep;
print "\ndone.\n";
$| = 0;

# foreach my $key (sort keys (%groups)) {
	# print "GROUPNAME: $key\n";
# }

# create master TOC if we have any framesets
if (scalar(@headerFramesetRefs) + scalar(@comintFramesetRefs) + scalar(@classFramesetRefs) + scalar(@manpageFramesetRefs) + scalar(@protocolFramesetRefs) + scalar(@categoryFramesetRefs) + scalar(@functionRefs)) {
    if ($generateDocSet) {
	print STDERR "Generating DocSet files suitable for use with docsetutil.\n";
	generateDocSetFile($inputDir);
    }
    if (!$skipTOC) {
	print STDERR "Generating TOCs.\n";
        &printMasterTOC();
        &addTopLinkToFramesetTOCs();
    } else {
	print STDERR "Not generating landing pages because -n flag specified.\n";
    }
    if ($has_resolver) {
	LinkResolver::resolveLinks($inputDir); # Apple internal resolver.
    } else {
	resolveLinks($inputDir, $externalXRefFiles, $externalAPIUIDPrefixes);
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
  my %macroLinkString = ();
  my %propLinkString = ();
  my %dataLinkString = ();
  my %comintsLinkString = ();
  my %classesLinkString = ();
  my %manpagesLinkString = ();
  my %protocolsLinkString = ();
  my %categoriesLinkString = ();
  my %functionsLinkString = ();

  my $seenHeaders = 0;
  my $seenType = 0;
  my $seenProp = 0;
  my $seenData = 0;
  my $seenMacros = 0;
  my $seenClasses = 0;
  my $seenComInts = 0;
  my $seenManPages = 0;
  my $seenProtocols = 0;
  my $seenCategories = 0;
  my $seenFunctions = 0;

  my $localDebug = 0;
  my %tempgroups = %groups;
  $tempgroups{"hd_master_letters_linked"} = "";

  my $template_number = 0;
  foreach my $TOCTemplate (@TOCTemplateList) {
    %letters_linked = (); # Reset this for each output file.
    my %used = %{$usedInTemplate{$TOCTemplate}};
    my @templatefields = split(/\$\$/, $TOCTemplate);
    my $templatefilename = $TOCNames[$template_number];
    my $templatename = $templatefilename;

    print "Writing output file for template \"$templatename\"\n";
    if ($localDebug) {
	print "Contains header list:        ".($used{header} ? 1 : 0)."\n";
	print "Contains type list:          ".($used{type} ? 1 : 0)."\n";
	print "Contains property list:      ".($used{prop} ? 1 : 0)."\n";
	print "Contains data list:          ".($used{data} ? 1 : 0)."\n";
	print "Contains function list:      ".($used{function} ? 1 : 0)."\n";
	print "Contains man page list:      ".($used{manpage} ? 1 : 0)."\n";
	print "Contains macro list:         ".($used{macro} ? 1 : 0)."\n";
	print "Contains protocol list:      ".($used{protocol} ? 1 : 0)."\n";
	print "Contains category list:      ".($used{category} ? 1 : 0)."\n";
	print "Contains class list:         ".($used{class} ? 1 : 0)."\n";
	print "Contains COM Interface list: ".($used{comint} ? 1 : 0)."\n";
	print "\n";
    }

    foreach my $group (sort keys (%tempgroups)) {

	print "processing group $group\n" if ($localDebug);

      %letters_linked = (); # Reset this for each group.
      $headersLinkString{$group} = "";
      $typeLinkString{$group} = "";
      $propLinkString{$group} = "";
      $dataLinkString{$group} = "";
      $comintsLinkString{$group} = "";
      $classesLinkString{$group} = "";
      $manpagesLinkString{$group} = "";
      $protocolsLinkString{$group} = "";
      $categoriesLinkString{$group} = "";
      $functionsLinkString{$group} = "";

      # get the HTML links to each header 
      if ($used{header}) {
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

          my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name, $group, "header");
          $headersLinkString{$group} .= $tmpString; $seenHeaders = 1;
        }
        print "\$headersLinkString is '".$headersLinkString{$group}."'\n" if ($localDebug);
      } else { $seenHeaders = scalar @headerFramesetRefs; }

      my $groupns = $group;
      $groupns =~ s/\s/_/sg;

      {
      my %temp_letters_linked = %letters_linked;
      my $grouptype = $groupns."_header";
      print "inserting into \"$grouptype\"\n" if ($localDebug);
      $group_letters_linked{$grouptype} = \%temp_letters_linked;
      %letters_linked = ();
      }
    
      # get the HTML links to each macro
      if ($used{macro}) {
        foreach my $ref (sort objName @macroFramesetRefs) {
          my $name = $ref->name();
          my $path = $ref->path();        
	  my $uid = $ref->uid();
	  if ($ref->group() ne $group) { next; }
  
          my $tmpString = &getLinkToFunctionFrom($masterTOC, $path, $name, $uid, $group, "macro");
          $macroLinkString{$group} .= $tmpString; $seenMacros = 1;
        }
        if (($localDebug) && length($macroLinkString{$group})) {print "\$macroLinkString is '".$macroLinkString{$group}."'\n";};
      } else { $seenMacros = scalar @macroFramesetRefs; }
    
      {
      my %temp_letters_linked = %letters_linked;
      my $grouptype = $groupns."_macro";
      print "inserting into \"$grouptype\" count ".scalar(keys %letters_linked)."\n" if ($localDebug);
      $group_letters_linked{$grouptype} = \%temp_letters_linked;
      %letters_linked = ();
      }

      # get the HTML links to each variable/constant 
      if ($localDebug) {
	print "BLANKLIST: ";
	foreach my $name (keys %letters_linked) {
		print "$name "
	}
	print "ENDLIST\n";
      }
      if ($used{data}) {
        foreach my $ref (sort objName @dataFramesetRefs) {
          my $name = $ref->name();
          my $path = $ref->path();        
	  my $uid = $ref->uid();
	  if ($ref->group() ne $group) {
		print "Not adding \"$name\" because GROUP \"".$ref->group()."\" ne \"$group\".\n" if ($localDebug);
		next;
	  }
	  print "Adding \"$name\".\n" if ($localDebug);

          my $tmpString = &getLinkToFunctionFrom($masterTOC, $path, $name, $uid, $group, "data");
          $dataLinkString{$group} .= $tmpString; $seenData = 1;
        }
        if (($localDebug) && length($dataLinkString{$group})) {print "\$dataLinkString is '".$dataLinkString{$group}."'\n";};
      } else { $seenData = scalar @dataFramesetRefs; }

      # get the HTML links to each property
      if ($localDebug) {
	print "BLANKLIST: ";
	foreach my $name (keys %letters_linked) {
		print "$name "
	}
	print "ENDLIST\n";
      }
      if ($used{prop}) {
        foreach my $ref (sort objName @propFramesetRefs) {
          my $name = $ref->name();
          my $path = $ref->path();        
	  my $uid = $ref->uid();
	  if ($ref->group() ne $group) {
		print "Not adding \"$name\" because GROUP \"".$ref->group()."\" ne \"$group\".\n" if ($localDebug);
		next;
	  }
	  print "Adding \"$name\".\n" if ($localDebug);

          my $tmpString = &getLinkToFunctionFrom($masterTOC, $path, $name, $uid, $group, "prop");
          $propLinkString{$group} .= $tmpString; $seenProp = 1;
        }
        if (($localDebug) && length($propLinkString{$group})) {print "\$propLinkString is '".$propLinkString{$group}."'\n";};
      } else { $seenProp = scalar @propFramesetRefs; }

      # Get the list of letters to create links.
      {
      my %temp_letters_linked = %letters_linked;
      my $grouptype = $groupns."_data";
      print "inserting into \"$grouptype\"\n" if ($localDebug);
      $group_letters_linked{$grouptype} = \%temp_letters_linked;
      if ($localDebug) {
	print "LIST: ";
	# foreach my $name (keys %{$group_letters_linked{$grouptype}}) {
	foreach my $name (keys %letters_linked) {
		print "$name "
	}
	print "ENDLIST\n";
      }
      %letters_linked = ();
      }
    
      # get the HTML links to each type 
      if ($used{type}) {
        foreach my $ref (sort objName @typeFramesetRefs) {
          my $name = $ref->name();
          my $path = $ref->path();
	  my $uid = $ref->uid();
	  if ($ref->group() ne $group) { next; }

          my $tmpString = &getLinkToFunctionFrom($masterTOC, $path, $name, $uid, $group, "type");
          $typeLinkString{$group} .= $tmpString; $seenType = 1;
        }
        if (($localDebug) && length($typeLinkString{$group})) {print "\$typeLinkString is '".$typeLinkString{$group}."'\n";};
      } else { $seenType = scalar @typeFramesetRefs; }
    
      {
      my %temp_letters_linked = %letters_linked;
      my $grouptype = $groupns."_type";
      print "inserting into \"$grouptype\"\n" if ($localDebug);
      $group_letters_linked{$grouptype} = \%temp_letters_linked;
      %letters_linked = ();
      }

      # get the HTML links to each man page 
      if ($used{manpage}) {
        foreach my $ref (sort objName @manpageFramesetRefs) {
          my $name = $ref->name();
          my $path = $ref->path();        
	  if ($ref->group() ne $group && $group ne "hd_master_letters_linked") { next; }

          my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name, $group, "man");
          $manpagesLinkString{$group} .= $tmpString; $seenManPages = 1;
        }
        if (($localDebug) && length($manpagesLinkString{$group})) {print "\$manpagesLinkString is '".$manpagesLinkString{$group}."'\n";};
      } else { $seenManPages = scalar @manpageFramesetRefs; }

      {
      my %temp_letters_linked = %letters_linked;
      my $grouptype = $groupns."_man";
      print "inserting into \"$grouptype\"\n" if ($localDebug);
      $group_letters_linked{$grouptype} = \%temp_letters_linked;
      %letters_linked = ();
      }
    
      # get the HTML links to each COM Interface 
      if ($used{comint}) {
        foreach my $ref (sort objName @comintFramesetRefs) {
          my $name = $ref->name();
          my $path = $ref->path();        
	  if ($ref->group() ne $group) { next; }

          my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name, $group, "comint");
          $comintsLinkString{$group} .= $tmpString; $seenComInts = 1;
        }
        if (($localDebug) && length($comintsLinkString{$group})) {print "\$comintsLinkString is '".$comintsLinkString{$group}."'\n";};
      } else { $seenComInts = scalar @comintFramesetRefs; }
    
      {
      my %temp_letters_linked = %letters_linked;
      my $grouptype = $groupns."_comint";
      print "inserting into \"$grouptype\"\n" if ($localDebug);
      $group_letters_linked{$grouptype} = \%temp_letters_linked;
      %letters_linked = ();
      }

      # get the HTML links to each class 
      if ($used{class}) {
        foreach my $ref (sort objName @classFramesetRefs) {
          my $name = $ref->name();
          my $path = $ref->path();        
	  if ($ref->group() ne $group) { next; }

          my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name, $group, "class");
          $classesLinkString{$group} .= $tmpString; $seenClasses = 1;
        }
        if (($localDebug) && length($classesLinkString{$group})) {print "\$classesLinkString is '".$classesLinkString{$group}."'\n";};
      } else { $seenClasses = scalar @classFramesetRefs; }

      {
      my %temp_letters_linked = %letters_linked;
      my $grouptype = $groupns."_class";
      print "inserting into \"$grouptype\"\n" if ($localDebug);
      $group_letters_linked{$grouptype} = \%temp_letters_linked;
      %letters_linked = ();
      }
    
      # get the HTML links to each protocol 
      if ($used{protocol}) {
        foreach my $ref (sort objName @protocolFramesetRefs) {
          my $name = $ref->name();
          my $path = $ref->path();        
	  if ($ref->group() ne $group) { next; }

          my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name, $group, "protocol");
          $protocolsLinkString{$group} .= $tmpString; $seenProtocols = 1;
        }
        if (($localDebug) && length($protocolsLinkString{$group})) {print "\$protocolsLinkString is '".$protocolsLinkString{$group}."'\n";};
      } else { $seenProtocols = scalar @protocolFramesetRefs; }

      {
      my %temp_letters_linked = %letters_linked;
      my $grouptype = $groupns."_protocol";
      print "inserting into \"$grouptype\"\n" if ($localDebug);
      $group_letters_linked{$grouptype} = \%temp_letters_linked;
      %letters_linked = ();
      }
    
      # get the HTML links to each category 
      if ($used{category}) {
        foreach my $ref (sort objName @categoryFramesetRefs) {
          my $name = $ref->name();
          my $path = $ref->path();        
	  if ($ref->group() ne $group) { next; }

          my $tmpString = &getLinkToFramesetFrom($masterTOC, $path, $name, $group, "category");
          $categoriesLinkString{$group} .= $tmpString; $seenCategories = 1;
        }
        if (($localDebug) && length($categoriesLinkString{$group})) {print "\$categoriesLinkString is '".$categoriesLinkString{$group}."'\n";};
      } else { $seenCategories = scalar @categoryFramesetRefs; }
    
      {
      my %temp_letters_linked = %letters_linked;
      my $grouptype = $groupns."_category";
      print "inserting into \"$grouptype\"\n" if ($localDebug);
      $group_letters_linked{$grouptype} = \%temp_letters_linked;
      %letters_linked = ();
      }

      # get the HTML links to each function 
      if ($used{function}) {
        foreach my $ref (sort objName @functionRefs) {
          my $name = $ref->name();
          my $path = $ref->path();        
	  my $uid = $ref->uid();
	  if ($ref->group() ne $group) { next; }

          my $tmpString = &getLinkToFunctionFrom($masterTOC, $path, $name, $uid, $group, "function");
          $functionsLinkString{$group} .= $tmpString; $seenFunctions = 1;
        }
        if (($localDebug) && length($functionsLinkString{$group})) {print "\$functionsLinkString is '".$functionsLinkString{$group}."'\n";};
      } else { $seenFunctions = scalar @functionRefs; }

      {
      my %temp_letters_linked = %letters_linked;
      my $grouptype = $groupns."_function";
      print "inserting into \"$grouptype\"\n" if ($localDebug);
      $group_letters_linked{$grouptype} = \%temp_letters_linked;
      %letters_linked = ();
      }

      # printll(\%letters_linked, $groupns);
    }
    # foreach my $key (keys %group_letters_linked) {
      # printll($group_letters_linked{$key}, $key);
    # }

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
		($field =~ /^\s*frameworkrelatedsection/) && do {

			if (!length($frameworkrelated)) {
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/frameworkrelatedsection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*frameworkrelatedlist/) && do {
			$field =~ s/\@\@.*//s;
			# print "FIELD IS $field\n";

			if ($include_in_output) {
				$out .= relatedDocs($frameworkrelated, $field);
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*frameworkpathsection/) && do {

			if (!length($frameworkpath)) {
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/frameworkpathsection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*frameworkpath/) && do {

			if ($include_in_output) {
				$out .= "$frameworkpath";
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
				if ($landingPageFrameworkUID ne "") {
					$out .= "<a name=\"$landingPageFrameworkUID\" title=\"$framework\"></a>";
				}
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
					$out .= genTable($headersLinkString{$group}, $field, $group, "header");
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
		($field =~ /^\s*grouplist/) && do {
			$keep =~ s/.*?\@\@//s;
			if ($include_in_output) {
				$out .= groupList();
			}
			last SWITCH;
			};
		($field =~ /^\s*manpagelist/) && do {

			if ($seenManPages) {
				$field =~ s/\@\@.*//s;
				if ($field =~ /^\s*manpagelist\s+nogroups\s+/) {
					$out .= genTable($manpagesLinkString{"hd_master_letters_linked"}, $field, "hd_master_letters_linked", "man", 0);
				} else {
				    foreach my $group (sort keys(%groups)) {
					$out .= genTable($manpagesLinkString{$group}, $field, $group, "man", 1);
				    }
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*oocontainersection/) && do {

			if (!$seenComInts && !$seenClasses && !$seenCategories && !$seenProtocols) {
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/oocontainersection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*comintsection/) && do {

			if (!$seenComInts) {
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/comintsection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*comintlist/) && do {

			if ($seenComInts) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($comintsLinkString{$group}, $field, $group, "comint");
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
					$out .= genTable($classesLinkString{$group}, $field, $group, "class");
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*categorysection/) && do {

			if (!$seenCategories) { # @@@ Debug checkpoint for categories
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
					$out .= genTable($categoriesLinkString{$group}, $field, $group, "category");
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*protocolsection/) && do {

			if (!$seenProtocols) { # @@@ Debug checkpoint for protocols
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
					$out .= genTable($protocolsLinkString{$group}, $field, $group, "protocol");
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*macrosection/) && do {

			if (!$seenMacros) {
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/macrosection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*macrolist/) && do {

			if ($seenMacros) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($macroLinkString{$group}, $field, $group, "macro");
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*propsection/) && do {

			if (!$seenProp) { # @@@ Debug checkpoint for properties
				$include_in_output = 0;
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*\/propsection/) && do {

			$include_in_output = 1;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*proplist/) && do {

			if ($seenProp) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($propLinkString{$group}, $field, $group, "prop");
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*datasection/) && do {

			if (!$seenData) { # @@@ Debug checkpoint for data (const, etc.)
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
					$out .= genTable($dataLinkString{$group}, $field, $group, "data");
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*typesection/) && do {

			if (!$seenType) { # @@@ Debug checkpoint for data types
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
		($field =~ /^\s*year/) && do {
			$out .= $yearStamp;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*date/) && do {
			$out .= $dateStamp;

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*copyright/) && do {

			if (length($frameworkCopyrightString)) {
				my $temp = $frameworkCopyrightString;
				$temp =~ s/\$\$year\@\@/\Q$yearStamp\E/g;
				$temp =~ s/\$\$date\@\@/\Q$dateStamp\E/g;
				$out .= $temp;
			} else {
				if ($HeaderDoc::copyrightOwner && length($HeaderDoc::copyrightOwner)) {
					$out .= "&copy; ".$yearStamp." ".$HeaderDoc::copyrightOwner
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*typelist/) && do {

			if ($seenType) {
				$field =~ s/\@\@.*//s;
				foreach my $group (sort keys(%groups)) {
					$out .= genTable($typeLinkString{$group}, $field, $group, "type");
				}
			}

			$keep =~ s/.*?\@\@//s;
			last SWITCH;
			};
		($field =~ /^\s*functionsection/) && do {

			if (!$seenFunctions) { # @@@ Debug checkpoint for functions
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
					$out .= genTable($functionsLinkString{$group}, $field, $group, "function");
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
    my @allFramesetRefs;
    push(@allFramesetRefs, @headerFramesetRefs);
    push(@allFramesetRefs, @comintFramesetRefs);
    push(@allFramesetRefs, @classFramesetRefs);
    push(@allFramesetRefs, @protocolFramesetRefs);
    push(@allFramesetRefs, @categoryFramesetRefs);
    my $localDebug = 0;
    
    foreach my $ref (@allFramesetRefs) {
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
                    my $topLink = "\n<font size=\"-2\"><a href=\"$relPathToMasterTOC\" target=\"_top\" $uniqueMarker>[Top]</a></font><br/>\n";
                    
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
    my $group = shift;
    my $typename = shift;
    my $linkString;
    
    my $relPath = &findRelativePath($masterTOCFile, $dest);
    my $namestring = getNameStringForLink($name, $group, $typename);
    $linkString = "<a $namestring href=\"$relPath\" target =\"_top\">$name</a><br/>\n"; 
    return $linkString;
}

sub getNameStringForLink
{
    my $name = shift;
    my $group = shift;
    my $typename = shift;
    my $namestring = "";
    my $groupns = $group;
    $groupns =~ s/\s/_/sg;

    my $grouptype = $groupns."_".$typename;
    my $firsttwo = uc($name);
    $firsttwo =~ s/^(..).*$/$1/s;
# print "FIRSTTWO: $firsttwo\n";
# cluck("test\n");
    if (!$letters_linked{$firsttwo}) {
	$namestring = "name=\"group_$grouptype"."_$firsttwo\"";
	$letters_linked{$firsttwo} = 1;
	# print "SET letters_linked{$firsttwo}\n";
    } else {
	$letters_linked{$firsttwo}++;
    }
    return $namestring;
}

sub getLinkToFunctionFrom {
    my $masterTOCFile = shift;
    my $dest = shift;    
    my $name = shift;    
    my $uid = shift;
    my $group = shift;
    my $typename = shift;
    my $linkString;
    
    my $relPath = &findRelativePath($masterTOCFile, $dest);
    my $ns = getNameStringForLink($name, $group, $typename);
    my $noClassName = $name;
    $noClassName =~ s/.*\:\://s;
    my $urlname = sanitize($noClassName);
    my $lp = "";
    if ($uid && length($uid)) {
	$urlname = $uid;
	$lp = " logicalPath=\"$uid\"";
    }
	# print "UIDCHECK: $uid\n";
    if ($uid =~ /\/\/apple_ref\/occ\/(clm|instm|intfcm|intfm)\//) {
	# Format Objective-C class name
	my $type = $1;
	$name =~ s/^(.*)\:\://;
	my $class = $1;
	my $plusmin = "+";
	if ($type eq "instm") {
		$plusmin = "-";
	}
	$name = $plusmin."[ $class $name ]";
    }
    $linkString = "<a $lp $ns href=\"$relPath#$urlname\" retarget=\"yes\" target=\"_top\">$name</a><br/>\n"; 
    return $linkString;
}


sub objName { # for sorting objects by their names
    uc($a->name()) cmp uc($b->name());
}

sub default_template
{
    my $template = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\"\n    \"http://www.w3.org/TR/1998/REC-html40-19980424/loose.dtd\">\n";
    my $stylesheet = "";
    # my $he = HeaderElement::new;
    # my $stylesheet = $he->styleSheet(0);

    $template .= "<html>\n<head>\n    <title>\$\$title\@\@</title>\n	<meta name=\"generator\" content=\"HeaderDoc\" />\n<meta name=\"xcode-display\" content=\"render\" />\n$stylesheet</head>\n<body bgcolor=\"$GHD::bgcolor\"><h1>\$\$framework\@\@ Documentation</h1><hr/><br/>\n";
    $template .= "<p>\$\$frameworkdiscussion\@\@</p>";
    $template .= "\$\$headersection\@\@<h2>Headers</h2>\n<blockquote>\n\$\$headerlist cols=2 order=down atts=border=\"0\" width=\"80%\"\@\@\n</blockquote>\$\$/headersection\@\@\n";
    $template .= "\$\$classsection\@\@<h2>Classes</h2>\n<blockquote>\n\$\$classlist cols=2 order=down atts=border=\"0\" width=\"80%\"\@\@\n</blockquote>\$\$/classsection\@\@\n";
    $template .= "\$\$categorysection\@\@<h2>Categories</h2>\n<blockquote>\n\$\$categorylist cols=2 order=down atts=border=\"0\" width=\"80%\"\@\@\n</blockquote>\$\$/categorysection\@\@\n";
    $template .= "\$\$protocolsection\@\@<h2>Protocols</h2>\n<blockquote>\n\$\$protocollist cols=2 order=down atts=border=\"0\" width=\"80%\"\@\@\n</blockquote>\$\$/protocolsection\@\@\n";
    $template .= "\$\$functionsection\@\@<h2>Functions</h2>\n<blockquote>\n\$\$functionlist cols=2 order=down atts=border=\"0\" width=\"80%\"\@\@\n</blockquote>\$\$/functionsection\@\@\n";
    $template .= "\$\$typesection\@\@<h2>Data Types</h2>\n<blockquote>\n\$\$typelist cols=2 order=down atts=border=\"0\" width=\"80%\"\@\@\n</blockquote>\$\$/typesection\@\@\n";
    $template .= "\$\$datasection\@\@<h2>Globals and Constants</h2>\n<blockquote>\n\$\$datalist cols=2 order=down atts=border=\"0\" width=\"80%\"\@\@\n</blockquote>\$\$/datasection\@\@\n";
    $template .= "\$\$propsection\@\@<h2>Globals and Constants</h2>\n<blockquote>\n\$\$proplist cols=2 order=down atts=border=\"0\" width=\"80%\"\@\@\n</blockquote>\$\$/propsection\@\@\n";
    $template .= "\$\$copyright\@\@\n";
    $template .= "</body>\n</html>\n";

    $gather_globals_and_constants = 1;
    $gather_types = 1;
    $gather_functions = 1;
			
    return $template;
}

sub gethierlinkstring
{
    my $group = shift;
    my $linkletter = shift;
    my $letter = shift;
    my $typename = shift;
    my $optional_last = "";
    if (@_) {
	$optional_last = shift;
	$optional_last = "-$optional_last";
    } elsif ($letter =~ /../) {
	$optional_last = $letter;
	$optional_last =~ s/(.)./$1Z/s;
	$optional_last = "-$optional_last";
    }
    my $groupns = $group;
    $groupns =~ s/\s/_/sg;
    my $grouptype = $groupns."_".$typename;

    return "<a href=\"#group_$grouptype"."_$linkletter\">$letter$optional_last</a>";
}

sub genTable
{
    my $inputstring = shift;
    my $settings = shift;
    my $groupname = shift;
    my $typename = shift;
    my $isman = 0;
    if (@_) {
	$isman = shift;
    }
    my $ncols = 0;
    my $order = "down";
    my $attributes = "border=\"0\" width=\"100%\"";
    my $tdclass = "";
    my $trclass = "";
    my $localDebug = 0;
    my $addempty = 0;
    my $notable = 0;

    print "genTable(IS: [omitted], SET: $settings, GN: $groupname, TN: $typename, IM: $isman)\n" if ($localDebug);

    my $mansectiontext = "";
    if ($isman) {
	my $mansectionname = $groupname;
	$mansectionname =~ s/^\s*Section\s+//s;

	my $filename="sectiondesc/man$mansectionname".".html";
	if (open(SECTIONTEXT, "<$filename")) {
		my $lastrs = $/;
		$/ = undef;
		$mansectiontext = <SECTIONTEXT>;
		$/ = $lastrs;
		close(SECTIONTEXT);
	} else {
		warn "No file for man section $mansectionname\n";
	}
    }

    if (!defined($inputstring)) { return ""; }

    my @lines = split(/\n/, $inputstring);
    my $nlines = scalar(@lines);

    my $addHierarchicalLinks = 0;
    my $hierstring = "";

    if ($HeaderDoc::groupHierLimit && ($nlines > $HeaderDoc::groupHierLimit)) {
	$addHierarchicalLinks = 1;

	my $splitparts = 0;
	my $attempts = 0;
	my $subgroupLimit = $HeaderDoc::groupHierSubgroupLimit;
	my $minsplit = 5;

	while ($splitparts < $minsplit && $attempts < 5) {
		my $linkletter = "";
		my $prevletter = "";
		my $prevtwoletter = "";
		$splitparts = 0; # Count the number of entries and reduce the limit as needed to ensure no singleton lists.
		$hierstring = "<blockquote class='letterlist'><table width='80%'><tr><td>\n";

		print "GROUPNAME: $groupname\n" if ($localDebug);
		my $groupns = $groupname;
		$groupns =~ s/\s/_/sg;
		my $grouptype = $groupns."_".$typename;
		print "GROUPTYPE: \"$grouptype\"\n" if ($localDebug);

		my %twoletterlinkcounts = %{$group_letters_linked{$grouptype}};

		print "GLLCHECK: ".scalar(keys %{$group_letters_linked{$grouptype}})."\n" if ($localDebug);

		my %oneletterlinkcounts = ();
		foreach my $twoletter (sort keys %twoletterlinkcounts) {
			# print "TL: $twoletter\n";
			my $firstletter = $twoletter;
			$firstletter =~ s/^(.).*$/$1/s;
			if (!$oneletterlinkcounts{$firstletter}) {
				# print "FIRST $firstletter; linkletter -> $twoletter\n";
				$oneletterlinkcounts{$firstletter} = $twoletterlinkcounts{$twoletter};
				if ($prevletter ne "") {
					$hierstring .= gethierlinkstring($groupname, $linkletter, $prevletter, $typename)."&nbsp;<span class='hierpipe'>|</span> \n";
				}
				$prevletter = $firstletter;
				$linkletter = $twoletter;
				$splitparts++;
			} elsif ($oneletterlinkcounts{$firstletter} + $twoletterlinkcounts{$twoletter} > $subgroupLimit) {
				# print "LIMIT $firstletter; linkletter -> $twoletter\n";
				$hierstring .= gethierlinkstring($groupname, $linkletter, $prevletter, $typename, $prevtwoletter)."&nbsp;<span class='hierpipe'>|</span> \n";
				$prevletter = $twoletter;
				$linkletter = $twoletter;
				$splitparts++;
			}
			$prevtwoletter = $twoletter;
		}
		if ($prevletter ne "") {
			$hierstring .= gethierlinkstring($groupname, $linkletter, $prevletter, $typename);
		}
		$hierstring .= "</td></tr></table></blockquote>\n";

		# Reduce the subgroup limit and increase the attempt count so that if
		# we execute this code again, we will probably get more subgroups.
		# Use the attempts count to ensure that this loop isn't infinite if
		# all entries have the same first letter.

		if ($splitparts < $minsplit) {
			print "Minimum split count $minsplit not reached.  Split count was $splitparts.  Reducing split count.\n" if ($localDebug);
			$subgroupLimit = $subgroupLimit / $minsplit;
		}
		$attempts++;
	}
	print "SPLITPARTS: $splitparts\n" if ($localDebug);
	if ($splitparts <= 1) {
		print "Could not split list at all.  Dropping singleton.\n" if ($localDebug);
		$hierstring = ""; # eliminate singleton lists.
	}
    # } else {
	# print "Not over limit: $groupname\n";
    }
    # print "HIERSTRING: $hierstring\n";

    my $ngroups = scalar(keys(%groups));

    my $groupnamestring = "";
    if ($groupname =~ /\S/) {
	my $groupnospc = $groupname;
	$groupnospc =~ s/\s/_/sg;
	$groupnamestring = "<p class='groupname'><a name='group_$groupnospc'></a><i>$groupname</i></p>\n";
    }
    if ($groupname eq "hd_master_letters_linked") { $groupnamestring = ""; }

    my $groupheadstring = "<blockquote class='groupindent'>\n";
    my $grouptailstring = "</blockquote>\n";
    if (!$ngroups) {
	$groupheadstring = "";
	$grouptailstring = "";
    }

    $settings =~ s/^\s*(\w+list)\s*//;
    my $name = $1;

    if ($settings =~ s/^nogroups\s+//) {
	$ngroups = 0;
    }
    if ($settings =~ s/^cols=(\d+)\s+//) {
	$ncols = $1;
    }
    if ($settings =~ s/^order=(\w+)\s+//) {
	$order = $1;
	if (!$ncols) { $ncols = 1; }
    }
    if ($settings =~ s/^trclass=(\w+)\s+//) {
	$trclass = " class=\"$1\"";
	if (!$ncols) { $ncols = 1; }
    }

    if ($settings =~ s/^tdclass=(\w+)\s+//) {
	$tdclass = " class=\"$1\"";
	if (!$ncols) { $ncols = 1; }
    }
    if ($settings =~ s/^notable//) {
	$notable = 1; $ncols = 1;
    }

    if ($settings =~ s/^addempty=(\d+)//) {
	$addempty = $1;
    }

    if ($settings =~ s/^atts=//) {
	$attributes = $settings;
	$settings = "";
	if (!$ncols) { $ncols = 1; }
    }

    if ($ncols) {
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

# warn("TABLE $attributes\n");
	my $outstring = "";
	if (!$notable) { $outstring .= "<table $attributes>"; }

	$curline = 0;
	$curcolumn = 0;
	my $currow = 0;
	my $first = 1;

	while ($curline < $nlines) {
		if (!$curcolumn) {
			if ($first) {
				$first = 0;
				if (!$notable) { $outstring .= "<tr$trclass>"; }
			} else {
				if (!$notable) { $outstring .= "</tr><tr>\n"; }
				$currow++;
			}
		} else {
			if ($addempty) {
				if (!$notable) { $outstring .= "<td width=\"$addempty\"$tdclass>&nbsp;</td>\n"; }
			}
		}

		my $line = ${$columns[$curcolumn]}[$currow];
		my $val = floor(100/$ncols);
		if ($notable) {
			$outstring .= "$line<br>\n";
		} else {
			$outstring .= "<td$tdclass width=\"" . $val . "\%\">$line</td>\n";
		}

		$curline++;
		$curcolumn = ($curcolumn + 1) % $ncols;
	}
	if (!$notable) { $outstring .= "</tr></table>\n"; }

	return $groupnamestring.$mansectiontext.$hierstring.$groupheadstring.$outstring.$grouptailstring;
    } else {
	return $groupnamestring.$mansectiontext.$hierstring.$groupheadstring.$inputstring.$grouptailstring;
    }
}

sub pathparts
{
    my $string = shift;
    my $count = 0;
    while ($string =~ s/\///) { $count++; }

	# print "PATHPARTS FOR $string: $count\n";

    return $count;
}

sub rightframework
{
    my $filename = shift;
    my $count = pathparts($filename);
    if ($frameworknestlevel == -1) {
	$frameworknestlevel = $count;
	return 1;
    }
    if ($frameworknestlevel < $count) {
	return 0;
    }
    return 1;
}

sub docListFromString
{
    my $string = shift;

    my @parts = split(/<!--/, $string);
    my @list = ();

    my $lastpath = "";
    foreach my $part (@parts) {
	if ($part =~ s/^\s*a logicalPath="//s) {
		# print "PART $part\n";
		my @subparts = split(/\"/, $part, 2);
		my $uid = $subparts[0];
		my $name = $subparts[1];
		$name =~ s/^\s*-->//s;
		my $string = "<!-- a logicalPath=\"$uid\" -->$name<!-- /a -->\n";
		push(@list, $string);
	}
    }
    return @list;
}

sub relatedDocs
{
    my $inputstring = shift;
    my $field = shift;
    my $retstring = "";
    my $tmpstring = "";

    if (length($inputstring)) {
	my @lines = docListFromString($inputstring);
	foreach my $line (@lines) {
		# print "LINE IS \"$line\"\n";
		if (length($line)) {
			$tmpstring .= $line . "\n";
		}
	}
	$tmpstring =~ s/\n$//s;

	$retstring .= genTable($tmpstring, $field, "", "");
    }
    return $retstring;
}

sub groupList
{
    my $string = "";
    my $first = 1;
    foreach my $group (sort keys (%groups)) {
	if ($group !~ /\S/) { next; }

	if ($first) { $first = 0; }
	else { $string .= "&nbsp;&nbsp;<span class='sectpipe'>|</span>&nbsp; \n"; }

	my $groupnospc = $group;
	$groupnospc =~ s/\s/_/sg;

	my $groupnobr = $group;
	$groupnobr =~ s/\s/&nbsp;/sg;

	$string .= "<a href=\"#group_$groupnospc\">$groupnobr</a>";
    }
    # $string .= "<br>\n";
    return $string;
}

sub printll
{
    my $arrayref = shift;
    my $group = shift;

    my %arr = %{$arrayref};
    print "FOR GROUP \"$group\"\n";
    foreach my $key (sort keys %arr) {
	print "$key\n";
    }
    print "\n";
}

# sub writeAPIOwner
# {
    # my $apioRef = shift;
    # my $file = shift;
# 
    # my $name = $apioRef->name();
    # my $type = $apioRef->type();
    # my $path = $apioRef->path();
    # my $uid = $apioRef->uid();
# 
    # print $file "<Node type="file">\n";
    # print $file "    <Name>$name</name>\n";
    # print $file "    <Path>$path</Path>\n";
    # print $file "    <Anchor>$uid</Anchor>\n";
# }

sub generateDocSetFile
{
    my $outputDir = shift;
    my $masterTOCFile = $outputDir.$pathSeparator.$masterTOCFileName;

# my @allFramesetRefs;
# my @headerFramesetRefs;
# my @dataFramesetRefs;
# my @macroFramesetRefs;
# my @typeFramesetRefs;
# my @comintFramesetRefs;
# my @classFramesetRefs;
# my @manpageFramesetRefs;
# my @categoryFramesetRefs;
# my @protocolFramesetRefs;
# my @functionRefs;
    # foreach my $header (@headerFramesetRefs) {
	# writeAPIOwner($header, OUTFILE);
    # }

    open(OUTFILE, ">$inputDir/Nodes.xml") || die("Could not write Nodes.xml file.\n");

    print OUTFILE "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    print OUTFILE "<DocSetNodes version=\"1.0\">\n";
    print OUTFILE "    <TOC>\n";
    print OUTFILE "        <Node type=\"file\">\n";
    print OUTFILE "            <Name>$framework</Name>\n";
    print OUTFILE "            <Path>$masterTOCFileName</Path>\n";
    print OUTFILE "        </Node>\n";
    print OUTFILE "    </TOC>\n";
    print OUTFILE "</DocSetNodes>\n";
    close(OUTFILE);

    open(OUTFILE, ">$inputDir/Tokens.xml") || die("Could not write Tokens.xml file.\n");
    print OUTFILE "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    print OUTFILE "<Tokens version=\"1.0\">\n";

    foreach my $header (@fileRefSets) {
	my $path = $header->path();
	my $relPath = &findRelativePath($masterTOCFile, $path);

	my $arrayRef = $header->group();
	my @refs = @{$arrayRef};

	print OUTFILE "    <File path=\"$relPath\">\n";
	foreach my $ref (@refs) {
		my $uid = $ref->uid();
		print OUTFILE "        <Token>\n";
		print OUTFILE "            <TokenIdentifier>$uid</TokenIdentifier>\n";
		# TODO: Availability
		print OUTFILE "        </Token>\n";
	}
	print OUTFILE "    </File>\n";
    }
    print OUTFILE "</Tokens>\n";

    close(OUTFILE);
}

