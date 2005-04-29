#!/usr/bin/perl
#
# Script name: headerDoc2HTML
# Synopsis: Scans a file for headerDoc comments and generates an HTML
#           file from the comments it finds.
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2004/05/13 17:40:07 $
#
# ObjC additions by SKoT McDonald <skot@tomandandy.com> Aug 2001 
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
# $Revision: 1.1.2.1 $
#####################################################################

my $VERSION = 2.1;
################ General Constants ###################################
my $isMacOS;
my $pathSeparator;
my $specifiedOutputDir;
my $export;
my $debugging;
my $testingExport = 0;
my $printVersion;
my $quietLevel;
my $xml_output;
my $headerdoc_strip;
my $regenerate_headers;
my $write_control_file;
#################### Locations #####################################
# Look-up tables are used when exporting API and doc to tab-delimited
# data files, which can be used for import to a database.  
# The look-up tables supply uniqueID-to-APIName mappings.

my $lang = "C";
my $scriptDir;
my $lookupTableDirName;
my $lookupTableDir;
my $dbLookupTables;
my $functionFilename;
my $typesFilename;
my $enumsFilename;
my $masterTOCName;
my @inputFiles;
my @ignorePrefixes = ();
my @perHeaderIgnorePrefixes = ();
my $reprocess_input = 0;
my $functionGroup = "";
$HeaderDoc::outerNamesOnly = 0;
$HeaderDoc::globalGroup = "";
my @headerObjects;	# holds finished objects, ready for printing
					# we defer printing until all header objects are ready
					# so that we can merge ObjC category methods into the 
					# headerObject that holds the class, if it exists.
my @categoryObjects;	    # holds finished objects that represent ObjC categories
my %objCClassNameToObject;	# makes it easy to find the class object to add category methods to

my @classObjects;
					
# Turn on autoflushing of 'print' output.  This is useful
# when HeaderDoc is operating in support of a GUI front-end
# which needs to get each line of log output as it is printed. 
$| = 1;

# Check options in BEGIN block to avoid overhead of loading supporting 
# modules in error cases.
my $modulesPath;
BEGIN {
    use FindBin qw ($Bin);
    use Cwd;
    use Getopt::Std;
    use File::Find;

    use lib '/Library/Perl/TechPubs'; # Apple configuration workaround
    use lib '/AppleInternal/Library/Perl'; # Apple configuration workaround

    my %options = ();
    $lookupTableDirName = "LookupTables";
    $functionFilename = "functions.tab";;
    $typesFilename = "types.tab";
    $enumsFilename = "enumConstants.tab";

    $scriptDir = cwd();
    $HeaderDoc::force_parameter_tagging = 0;
    $HeaderDoc::use_styles = 0;
    $HeaderDoc::ignore_apiuid_errors = 0;
    $HeaderDoc::maxDecLen = 60; # Wrap functions, etc. if declaration longer than this length

    if ($^O =~ /MacOS/i) {
		$pathSeparator = ":";
		$isMacOS = 1;
		#$Bin seems to return a colon after the path on certain versions of MacPerl
		#if it's there we take it out. If not, leave it be
		#WD-rpw 05/09/02
		($modulesPath = $FindBin::Bin) =~ s/([^:]*):$/$1/;
    } else {
		$pathSeparator = "/";
		$isMacOS = 0;
    }
	$modulesPath = "$FindBin::Bin"."$pathSeparator"."Modules";
	
	foreach (qw(Mac::Files)) {
	    $MOD_AVAIL{$_} = eval "use $_; 1";
    }

    $HeaderDoc::twig_available = 0;
    foreach my $path (@INC) {
	# print "$path\n";
	my $name = $path.$pathSeparator."XML".$pathSeparator."Twig.pm";
	# print "NAME: $name\n";
	if (-f $name) {
		$HeaderDoc::twig_available = 1;
	}
    }
    if ($HeaderDoc::twig_available) {
	# This doesn't work!  Need alternate solution.
	# use HeaderDoc::Regen;
    }

    &getopts("CHOSXdho:qrstuvx", \%options);
    if ($options{v}) {
    	print "Getting version information for all modules.  Please wait...\n";
		$printVersion = 1;
		return;
    }

    if ($options{r}) {
# print "TWIG? $HeaderDoc::twig_available\n";
	if ($HeaderDoc::twig_available) {
		print "Regenerating headers.\n";
	} else {
		warn "***********************************************************************\n";
		warn "*    Headerdoc comment regeneration from XML requires XML::Parser     *\n";
		warn "*             and XML::Twig, available from CPAN.  Visit              *\n";
		warn "*                                                                     *\n";
		warn "*                         http://www.cpan.org                         *\n";
		warn "*                                                                     *\n";
		warn "*                        for more information.                        *\n";
		warn "***********************************************************************\n";
		exit -1;
	}
	$regenerate_headers = 1;
    } else {
	$regenerate_headers = 0;
    }

    if ($options{S}) {
	$HeaderDoc::IncludeSuper = 1;
    } else {
	$HeaderDoc::IncludeSuper = 0;
    }
    if ($options{C}) {
	$HeaderDoc::ClassAsComposite = 1;
    } else {
	$HeaderDoc::ClassAsComposite = 0;
    }

    if ($options{s}) {
	$headerdoc_strip = 1;
    } else {
	$headerdoc_strip = 0;
    }

    if ($options{h}) {
	$write_control_file = "1";
    } else {
	$write_control_file = "0";
    }
    if ($options{u}) {
	$HeaderDoc::sort_entries = 0;
    } else {
	$HeaderDoc::sort_entries = 1;
    }
    if ($options{H}) {
	$HeaderDoc::insert_header = 1;
    } else {
	$HeaderDoc::insert_header = 0;
    }
    if ($options{q}) {
	$quietLevel = "1";
    } else {
	$quietLevel = "0";
    }
    if ($options{t}) {
	if (!$quietLevel) {
		print "Forcing strict parameter tagging.\n";
	}
	$HeaderDoc::force_parameter_tagging = 1;
    }
    if ($options{O}) {
	$HeaderDoc::outerNamesOnly = 1;
    } else {
	$HeaderDoc::outerNamesOnly = 0;
    }
    if ($options{d}) {
            print "\tDebugging on...\n\n";
            $debugging = 1;
    }

    if ($options{o}) {
        $specifiedOutputDir = $options{o};
        if (! -e $specifiedOutputDir)  {
            unless (mkdir ("$specifiedOutputDir", 0777)) {
                die "Error: $specifiedOutputDir does not exist. Exiting. \n$!\n";
            }
        } elsif (! -d $specifiedOutputDir) {
            die "Error: $specifiedOutputDir is not a directory. Exiting.\n$!\n";
        } elsif (! -w $specifiedOutputDir) {
            die "Error: Output directory $specifiedOutputDir is not writable. Exiting.\n$!\n";
        }
		if ($quietLevel eq "0") {
			print "\nDocumentation will be written to $specifiedOutputDir\n";
		}
    }
    $lookupTableDir = "$scriptDir$pathSeparator$lookupTableDirName";
    if (($options{x}) || ($testingExport)) {
        if ((-e "$lookupTableDir$pathSeparator$functionFilename") && (-e "$lookupTableDir$pathSeparator$typesFilename")) {
                print "\nWill write database files to an Export directory within each top-level HTML directory.\n\n";
                $export = 1;
        } else {
                print "\nLookup table files not available. Cannot export data.\n";
            $export = 0;
                $testingExport = 0;
        }
    }
    if ($quietLevel eq "0") {
      if ($options{X}) {
	print "XML output mode.\n";
	$xml_output = 1;
      } else {
	print "HTML output mode.\n";
	$xml_output = 0;
      }
    }
# print "output mode is $xml_output\n";
    
    if (($#ARGV == 0) && (-d $ARGV[0])) {
        my $inputDir = $ARGV[0];
        if ($inputDir =~ /$pathSeparator$/) {
			$inputDir =~ s|(.*)$pathSeparator$|$1|; # get rid of trailing slash, if any
		}		
		if ($^O =~ /MacOS/i) {
			find(\&getHeaders, $inputDir);
		} else {
			&find({wanted => \&getHeaders, follow => 1}, $inputDir);
		}
    } else {
        print "Will process one or more individual files.\n" if ($debugging);
        foreach my $singleFile (@ARGV) {
            if (-f $singleFile) {
                    push(@inputFiles, $singleFile);
                }
        }
    }
    unless (@inputFiles) {
        print "No valid input files specified. \n\n";
        if ($isMacOS) {
            die "\tTo use HeaderDoc, drop a header file or folder of header files on this application.\n\n";
            } else {
                    die "\tUsage: headerdoc2html [-dq] [-o <output directory>] <input file(s) or directory>.\n\n";
            }
    }
    
# /*! @function getHeaders
#   */
    sub getHeaders {
        my $filePath = $File::Find::name;
        my $fileName = $_;

        
        if ($fileName =~ /\.(c|h|i|hdoc|php|php\d|class|pas|p|java|jsp|js|jscript|html|shtml|pl|csh|sh|defs)$/) {
            push(@inputFiles, $filePath);
        }
    }
}


use strict;
use File::Copy;
use lib $modulesPath;

# Classes and other modules specific to HeaderDoc
use HeaderDoc::DBLookup;
use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray linesFromFile
                            printHash updateHashFromConfigFiles getHashFromConfigFile quote parseTokens);
use HeaderDoc::BlockParse qw(blockParse);
use HeaderDoc::Header;
use HeaderDoc::ClassArray;
use HeaderDoc::CPPClass;
use HeaderDoc::ObjCClass;
use HeaderDoc::ObjCProtocol;
use HeaderDoc::ObjCCategory;
use HeaderDoc::Function;
use HeaderDoc::Method;
use HeaderDoc::Typedef;
use HeaderDoc::Struct;
use HeaderDoc::Constant;
use HeaderDoc::Var;
use HeaderDoc::PDefine;
use HeaderDoc::Enum;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::ParseTree;

################ Setup from Configuration File #######################
my $localConfigFileName = "headerDoc2HTML.config";
my $preferencesConfigFileName = "com.apple.headerDoc2HTML.config";
my $homeDir;
my $usersPreferencesPath;
#added WD-rpw 07/30/01 to support running on MacPerl
#modified WD-rpw 07/01/02 to support the MacPerl 5.8.0
if ($^O =~ /MacOS/i) {
	eval 
	{
		require "FindFolder.pl";
		$homeDir = MacPerl::FindFolder("D");	#D = Desktop. Arbitrary place to put things
		$usersPreferencesPath = MacPerl::FindFolder("P");	#P = Preferences
	};
	if ($@) {
		import Mac::Files;
		$homeDir = Mac::Files::FindFolder(kOnSystemDisk(), kDesktopFolderType());
		$usersPreferencesPath = Mac::Files::FindFolder(kOnSystemDisk(), kPreferencesFolderType());
	}
} else {
	$homeDir = (getpwuid($<))[7];
	$usersPreferencesPath = $homeDir.$pathSeparator."Library".$pathSeparator."Preferences";
}

# The order of files in this array determines the order that the config files will be read
# If there are multiple config files that declare a value for the same key, the last one read wins
my @configFiles = ($usersPreferencesPath.$pathSeparator.$preferencesConfigFileName, $Bin.$pathSeparator.$localConfigFileName);

# default configuration, which will be modified by assignments found in config files.
my %config = (
    copyrightOwner => "",
    defaultFrameName => "index.html",
    compositePageName => "CompositePage.html",
    masterTOCName => "MasterTOC.html",
    apiUIDPrefix => "apple_ref",
    ignorePrefixes => "",
    htmlHeader => "",
    dateFormat => "",
    textStyle => "",
    commentStyle => "",
    preprocessorStyle => "",
    funcNameStyle => "",
    stringStyle => "",
    charStyle => "",
    numberStyle => "",
    keywordStyle => "",
    typeStyle => "",
    paramStyle => "",
    varStyle => ""
);

%config = &updateHashFromConfigFiles(\%config,\@configFiles);

if (defined $config{"ignorePrefixes"}) {
    my $localDebug = 0;
    my @prefixlist = split(/\|/, $config{"ignorePrefixes"});
    foreach my $prefix (@prefixlist) {
	print "ignoring $prefix\n" if ($localDebug);
	push(@ignorePrefixes, $prefix);
    }
}

if (defined $config{"copyrightOwner"}) {
    HeaderDoc::APIOwner->copyrightOwner($config{"copyrightOwner"});
}
if (defined $config{"defaultFrameName"}) {
    HeaderDoc::APIOwner->defaultFrameName($config{"defaultFrameName"});
}
if (defined $config{"compositePageName"}) {
    HeaderDoc::APIOwner->compositePageName($config{"compositePageName"});
}
if (defined $config{"apiUIDPrefix"}) {
    HeaderDoc::APIOwner->apiUIDPrefix($config{"apiUIDPrefix"});
}
if (defined $config{"htmlHeader"}) {
    HeaderDoc::APIOwner->htmlHeader($config{"htmlHeader"});
}
my $oldRecSep = $/;
undef $/;

if (defined $config{"htmlHeaderFile"}) {
    my $basename = $config{"htmlHeaderFile"};
    my @htmlHeaderFiles = ($Bin.$pathSeparator.$basename, $usersPreferencesPath.$pathSeparator.$basename, $basename);
    foreach my $filename (@htmlHeaderFiles) {
	if (open(HTMLHEADERFILE, "<$filename")) {
	    my $headerString = <HTMLHEADERFILE>;
	    close(HTMLHEADERFILE);
	    # print "HEADER: $headerString";
	    HeaderDoc::APIOwner->htmlHeader($headerString);
	}
    }
}
$/ = $oldRecSep;

if (defined $config{"dateFormat"}) {
    $HeaderDoc::datefmt = $config{"dateFormat"};
}
HeaderDoc::APIOwner->fix_date();

if (defined $config{"textStyle"}) {
	HeaderDoc::APIOwner->setStyle("text", $config{"textStyle"});
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


################ Version Info ##############################
if ($printVersion) {
    &printVersionInfo();
    exit;
} 

################ Exporting ##############################
if ($export || $testingExport) {
	HeaderDoc::DBLookup->loadUsingFolderAndFiles($lookupTableDir, $functionFilename, $typesFilename, $enumsFilename);
}

################### States ###########################################
my $inHeader        = 0;
my $inJavaSource    = 0;
my $inShellScript   = 0;
my $inPerlScript    = 0;
my $inPHPScript     = 0;
my $inCPPHeader     = 0;
my $inOCCHeader     = 0;
my $inClass         = 0; #includes CPPClass, ObjCClass ObjCProtocol
my $inInterface     = 0;
my $inFunction      = 0;
my $inFunctionGroup = 0;
my $inGroup         = 0;
my $inTypedef       = 0;
my $inUnknown       = 0;
my $inStruct        = 0;
my $inUnion         = 0;
my $inConstant      = 0;
my $inVar           = 0;
my $inPDefine       = 0;
my $inEnum          = 0;
my $inMethod        = 0;

################ Processing starts here ##############################
my $headerObject;  # this is the Header object that will own the HeaderElement objects for this file.
my $rootFileName;

if (!$quietLevel) {
    print "======= Parsing Input Files =======\n";
}

my $methods_with_new_parser = 1;

foreach my $inputFile (@inputFiles) {
	my $constantObj;
	my $enumObj;
	my $funcObj;
        my $methObj;
	my $pDefineObj;
	my $structObj;
	my $curObj;
	my $varObj;
	my $cppAccessControlState = "protected:"; # the default in C++
	my $objcAccessControlState = "private:"; # the default in Objective C
	
    my @path = split (/$pathSeparator/, $inputFile);
    my $filename = pop (@path);
    my $sublang = "";
    if ($quietLevel eq "0") {
	if ($headerdoc_strip) {
		print "\nStripping $filename\n";
	} elsif ($regenerate_headers) {
		print "\nRegenerating $filename\n";
	} else {
		print "\nProcessing $filename\n";
	}
    }
    @perHeaderIgnorePrefixes = ();
    $reprocess_input = 0;
    
    my $headerDir = join("$pathSeparator", @path);
    ($rootFileName = $filename) =~ s/\.(c|h|i|hdoc|php|php\d|class|pas|p|java|jsp|js|jscript|html|shtml|pl|csh|sh|defs)$//;
    if ($filename =~ /\.(php|php\d|class)$/) {
	$lang = "php";
	$sublang = "php";
    } elsif ($filename =~ /\.(c|C|cpp)$/) {
	# treat a C program similar to PHP, since it could contain k&r-style declarations
	$lang = "Csource";
	$sublang = "Csource";
    } elsif ($filename =~ /\.(s|d|)html$/) {
	$lang = "java";
	$sublang = "java";
    } elsif ($filename =~ /\.j(ava|s|sp|script)$/) {
	$lang = "java";
	$sublang = "javascript";
    } elsif ($filename =~ /\.p(as|)$/) {
	$lang = "pascal";
	$sublang = "pascal";
    } elsif ($filename =~ /\.pl$/) {
	$lang = "perl";
	$sublang = "perl";
    } elsif ($filename =~ /\.(c|)sh$/) {
	$lang = "shell";
	$sublang = "shell";
    } else {
	$lang = "C";
	$sublang = "C";
    }

    $HeaderDoc::lang = $lang;
    $HeaderDoc::sublang = $sublang;

    if ($filename =~ /\.defs/) { 
	$HeaderDoc::sublang = "MIG";
    }

    my $rootOutputDir;
    if (length ($specifiedOutputDir)) {
    	$rootOutputDir ="$specifiedOutputDir$pathSeparator$rootFileName";
    } elsif (@path) {
    	$rootOutputDir ="$headerDir$pathSeparator$rootFileName";
    } else {
    	$rootOutputDir = $rootFileName;
    }
    
	my @rawInputLines = &linesFromFile($inputFile);

    my @cookedInputLines;
    my $localDebug = 0;

    foreach my $line (@rawInputLines) {
	foreach my $prefix (@ignorePrefixes) {
	    if ($line =~ s/^\s*$prefix\s*//g) {
		print "ignored $prefix\n" if ($localDebug);
	    }
	}
	push(@cookedInputLines, $line);
    }
    @rawInputLines = @cookedInputLines;
	
REDO:
print "REDO" if ($debugging);
    # check for HeaderDoc comments -- if none, move to next file
    my @headerDocCommentLines = grep(/^\s*\/\*\!/, @rawInputLines);
    if ((!@headerDocCommentLines) && ($lang eq "java")) {
	@headerDocCommentLines = grep(/^\s*\/\*\*/, @rawInputLines);
    }
    if ((!@headerDocCommentLines) && ($lang eq "perl" || $lang eq "shell")) {
	@headerDocCommentLines = grep(/^\s*\#\s*\/\*\!/, @rawInputLines);
    }
    if ((!@headerDocCommentLines) && ($lang eq "pascal")) {
	@headerDocCommentLines = grep(/^\s*\{\!/, @rawInputLines);
    }
    if (!@headerDocCommentLines) {
	if ($quietLevel eq "0") {
            print "    Skipping. No HeaderDoc comments found.\n";
	}
        next;
    }
    
    $headerObject = HeaderDoc::Header->new();
    $headerObject->linenum(0);
    $headerObject->apiOwner($headerObject);
    $HeaderDoc::headerObject = $headerObject;

    # print "output mode is $xml_output\n";

    if ($quietLevel eq "0") {
      if ($xml_output) {
	$headerObject->outputformat("hdxml");
      } else { 
	$headerObject->outputformat("html");
      }
    }
	$headerObject->outputDir($rootOutputDir);
	$headerObject->name($filename);
	$headerObject->filename($filename);
	my $fullpath=cwd()."/$inputFile";
	$headerObject->fullpath($fullpath);
	
    # scan input lines for class declarations
    # return an array of array refs, the first array being the header-wide lines
    # the others (if any) being the class-specific lines
	my @lineArrays = &getLineArrays(\@rawInputLines);

# print "NLA: " . scalar(@lineArrays) . "\n";
    
    my $localDebug = 0;
    my $linenumdebug = 0;

    if ($headerdoc_strip) {
	# print "input file is $filename, output dir is $rootOutputDir\n";
	strip($filename, $rootOutputDir, \@rawInputLines);
	print "done.\n" if ($quietLevel eq "0");
	next;
    }
    if ($regenerate_headers) {
	HeaderDoc::Regen->regenerate($inputFile, $rootOutputDir);
	print "done.\n" if ($quietLevel eq "0");
	next;
    }

    foreach my $arrayRef (@lineArrays) {
        my $blockOffset = 0;
        my @inputLines = @$arrayRef;
	    # look for /*! comments and collect all comment fields into the appropriate objects
        my $apiOwner = $headerObject;  # switches to a class/protocol/category object, when within a those declarations
	$HeaderDoc::currentClass = $apiOwner;
	    print "inHeader\n" if ($localDebug);
	    my $inputCounter = 0;
	    my $ctdebug = 0;
	    my $classType = "unknown";
	    print "CLASS TYPE CHANGED TO $classType\n" if ($ctdebug);
	    while ($inputCounter <= $#inputLines) {
			my $line = "";           
	        
	        	print "Input line number: $inputCounter\n" if ($localDebug);
			print "last line ".$inputLines[$inputCounter-1]."\n" if ($localDebug);
			print "next line ".$inputLines[$inputCounter]."\n" if ($localDebug);
	        	if ($inputLines[$inputCounter] =~ /^\s*(public|private|protected)/) {
				$cppAccessControlState = $&;
	        		if ($inputLines[$inputCounter] =~ /^\s*(public|private|protected)\s*:/) {
					# trim leading whitespace and tabulation
					$cppAccessControlState =~ s/^\s+//;
					# trim ending ':' and whitespace
					$cppAccessControlState =~ s/\s*:\s*$/$1/s;
					# set back the ':'
					$cppAccessControlState = "$cppAccessControlState:";
				}
			}
	        	if ($inputLines[$inputCounter] =~ /^\s*(\@public|\@private|\@protected)/) {
				$objcAccessControlState = $&;
	        		if ($inputLines[$inputCounter] =~ /^\s*(\@public|\@private|\@protected)\s+/) {
					# trim leading whitespace and tabulation
					$objcAccessControlState =~ s/^\s+//;
					# trim ending ':' and whitespace
					$objcAccessControlState =~ s/\s*:\s*$/$1/s;
					# set back the ':'
					$objcAccessControlState = "$objcAccessControlState:";
				}
			}


	        	if (($lang ne "pascal" && (
			     ($inputLines[$inputCounter] =~ /^\s*\/\*\!/) ||
			     (($lang eq "perl" || $lang eq "shell") && ($inputLines[$inputCounter] =~ /^\s*\#\s*\/\*\!/)) ||
			     (($lang eq "java") && ($inputLines[$inputCounter] =~ /^\s*\/\*\*/)))) ||
			    (($lang eq "pascal") && ($inputLines[$inputCounter] =~ s/^\s*\{!/\/\*!/s))) {  # entering headerDoc comment
				my $newlinecount = 0;
				# slurp up comment as line
				if (($lang ne "pascal" && ($inputLines[$inputCounter] =~ /\s*\*\//)) ||
				    ($lang eq "pascal" && ($inputLines[$inputCounter] =~ s/\s*\}/\*\//s))) { # closing comment marker on same line

					my $linecopy = $inputLines[$inputCounter];
					# print "LINE IS \"$linecopy\".\n" if ($linenumdebug);
					$newlinecount = ($linecopy =~ tr/\n//);
					$blockOffset += $newlinecount - 1;
					print "NEWLINECOUNT: $newlinecount\n" if ($linenumdebug);
					print "BLOCKOFFSET: $blockOffset\n" if ($linenumdebug);

					$line .= $inputLines[$inputCounter++];
					# This is perfectly legal.  Don't warn
					# necessarily.
					if (!emptyHDok($line)) {
						warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "HeaderDoc comment", "1");
					}
	        			print "Input line number: $inputCounter\n" if ($localDebug);
					print "next line ".$inputLines[$inputCounter]."\n" if ($localDebug);
				} else {                                       # multi-line comment
					my $in_textblock = 0; my $in_pre = 0;
					do {
						my $templine = $inputLines[$inputCounter];
						while ($templine =~ s/\@textblock//i) { $in_textblock++; }  
						while ($templine =~ s/\@\/textblock//i) { $in_textblock--; }
						while ($templine =~ s/<pre>//i) { $in_pre++; print "IN PRE\n" if ($localDebug);}
						while ($templine =~ s/<\/pre>//i) { $in_pre--; print "OUT OF PRE\n" if ($localDebug);}
						if (!$in_textblock && !$in_pre) {
							$inputLines[$inputCounter] =~ s/^[\t ]*[*]?[\t ]+(.*)$/$1/; # remove leading whitespace, and any leading asterisks
						}
						my $newline = $inputLines[$inputCounter++];
						warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "HeaderDoc comment", "2");
						$newline =~ s/^ \*//;
						if ($lang eq "perl" || $lang eq "shell") {
						    $newline =~ s/^\s*\#\s*//;
						}
						$line .= $newline;
	        				print "Input line number: $inputCounter\n" if ($localDebug);
						print "next line ".$inputLines[$inputCounter]."\n" if ($localDebug);
					} while ((($lang eq "pascal" && ($inputLines[$inputCounter] !~ /\}/)) ||($lang ne "pascal" && ($inputLines[$inputCounter] !~ s/\*\//\*\//s))) && ($inputCounter <= $#inputLines));
					my $newline = $inputLines[$inputCounter++];
					# This is not inherently wrong.
					if (!emptyHDok($line)) {
				 		warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "HeaderDoc comment", "3");
					}
					if ($lang eq "perl" || $lang eq "shell") {
					    $newline =~ s/^\s*\#\s*//;
					}
					if ($newline !~ /^ \*\//) {
						$newline =~ s/^ \*//;
					}
					$line .= $newline;              # get the closing comment marker
	        		print "Input line number: $inputCounter\n" if ($localDebug);
				print "last line ".$inputLines[$inputCounter-1]."\n" if ($localDebug);
				print "next line ".$inputLines[$inputCounter]."\n" if ($localDebug);
			    }
				# print "ic=$inputCounter\n" if ($localDebug);
			    # HeaderDoc-ize JavaDoc/PerlDoc comments
			    if (($lang eq "perl" || $lang eq "shell") && ($line =~ /^\s*\#\s*\/\*\!/)) {
				$line =~ s/^\s*\#\s*\/\*\!/\/\*\!/;
			    }
			    if (($lang eq "java") && ($line =~ /^\s*\/\*\*/)) {
				$line =~ s/^\s*\/\*\*/\/\*\!/;
			    }
			    $line =~ s/^\s+//;              # trim leading whitespace
			    $line =~ s/^(.*)\*\/\s*$/$1/s;  # remove closing comment marker

			    # print "line \"$line\"\n" if ($localDebug);
	           
				SWITCH: { # determine which type of comment we're in
					($line =~ /^\/\*!\s+\@header\s*/i) && do {$inHeader = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@framework\s*/i) && do {$inHeader = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@template\s*/i) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@interface\s*/i) && do {$inClass = 1; $inInterface = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@class\s*/i) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@protocol\s*/i) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@category\s*/i) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*c\+\+\s*/i) && do {$inCPPHeader = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*objc\s*/i) && do {$inOCCHeader = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*perl\s*/i) && do {$inPerlScript = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*shell\s*/i) && do {$inShellScript = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*php\s*/i) && do {$inPHPScript = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*java\s*/i) && do {$inJavaSource = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*javascript\s*/i) && do {$inJavaSource = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@functiongroup\s*/i) && do {$inFunctionGroup = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@group\s*/i) && do {$inGroup = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@function\s*/i) && do {$inFunction = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@methodgroup\s*/i) && do {$inFunctionGroup = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@method\s*/i) && do {
						    if ($classType eq "occ" ||
							$classType eq "intf" ||
							$classType eq "occCat") {
							    $inMethod = 1;last SWITCH;
						    } else {
							    $inFunction = 1;last SWITCH;
						    }
					};
					($line =~ /^\/\*!\s+\@typedef\s*/i) && do {$inTypedef = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@union\s*/i) && do {$inUnion = 1;$inStruct = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@struct\s*/i) && do {$inStruct = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@const(ant)?\s*/i) && do {$inConstant = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@var\s*/i) && do {$inVar = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@define(d)?block\s*/i) && do {$inPDefine = 2;last SWITCH;};
					($line =~ /^\/\*!\s+\@\/define(d)?block\s*/i) && do {$inPDefine = 0;last SWITCH;};
					($line =~ /^\/\*!\s+\@define(d)?\s*/i) && do {$inPDefine = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@lineref\s+(\d+)/) && do {$blockOffset = $1 - $inputCounter; $inputCounter--; print "BLOCKOFFSET SET TO $blockOffset\n" if ($linenumdebug); last SWITCH;};
					($line =~ /^\/\*!\s+\@enum\s*/i) && do {$inEnum = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@serial(Data|Field|)\s+/i) && do {$inUnknown = 2;last SWITCH;};
					($line =~ /^\/\*!\s*\w/i) && do {$inUnknown = 1;last SWITCH;};
					my $linenum = $inputCounter - 1;
					print "$filename:$linenum:HeaderDoc comment is not of known type. Comment text is:\n";
					print "    $line\n";
				}
				# $inputCounter--; # inputCounter is current line.
				my $linenum = $inputCounter - 1;
				my $preAtPart = "";
				$line =~ s/\n\n/\n<br><br>\n/g; # change newline pairs into HTML breaks, for para formatting
				if ($inUnknown == 1) {
					if ($line =~ s/^\s*\/\*!\s*(\w.*?)\@/\/\*! \@/si) {
						$preAtPart = $1;
					} elsif ($line !~ /^\s*\/\*!\s*.*\@/) {
						$preAtPart = $line;
						$preAtPart =~ s/^\s*\/\*!\s*//si;
						$preAtPart =~ s/\s*\*\/\s*$//si;
						$line = "/*! */";
					}
					print "preAtPart: \"$preAtPart\"\n" if ($localDebug);
					print "line: \"$line\"\n" if ($localDebug);
				}
				my @fields = split(/\@/, $line);
				my @newfields = ();
				my $lastappend = "";
				my $in_textblock = 0;
				my $in_link = 0;
				foreach my $field (@fields) {
				  print "processing $field\n" if ($localDebug);
				  if ($in_textblock) {
				    if ($field =~ /^\/textblock/) {
					print "out of textblock\n" if ($localDebug);
					if ($in_textblock == 1) {
					    my $cleanfield = $field;
					    $cleanfield =~ s/^\/textblock//i;
					    $lastappend .= $cleanfield;
					    push(@newfields, $lastappend);
					    print "pushed \"$lastappend\"\n" if ($localDebug);
					    $lastappend = "";
					}
					$in_textblock = 0;
				    } else {
					# clean up text block
					$field =~ s/\</\&lt\;/g;
					$field =~ s/\>/\&gt\;/g;
					$lastappend .= "\@$field";
					print "new field is \"$lastappend\"\n" if ($localDebug);
				    }
				  } else {
				    # if ($field =~ /value/) { warn "field was $field\n"; }
				    if ($field =~ s/^value/<hd_value\/>/si) {
					$lastappend = pop(@newfields);
				    }
				    if ($field =~ s/^inheritDoc/<hd_ihd\/>/si) {
					$lastappend = pop(@newfields);
				    }
				    # if ($field =~ /value/) { warn "field now $field\n"; }
				    if ($field =~ s/^\/link/<\/hd_link>/i) {
					$in_link = 0;
				    }
				    if ($field =~ s/^link\s+//i) {
					$in_link = 1;
					my $target = "";
					my $lastfield;

					if ($lastappend eq "") {
					    $lastfield = pop(@newfields);
					} else {
					    $lastfield = "";
					}
					# print "lastfield is $lastfield";
					$lastappend .= $lastfield; 
					if ($field =~ /^(\S*?)\s/) {
					    $target = $1;
					} else {
					    # print "$filename:$linenum:MISSING TARGET FOR LINK!\n";
					    $target = $field;
					}
					my $localDebug = 0;
					print "target $target\n" if ($localDebug);
					my $qtarget = quote($target);
					$field =~ s/^$qtarget//g;
					$field =~ s/\\$/\@/;
					print "name $field\n" if ($localDebug);
					$lastappend .= "<hd_link $target>";
					$lastappend .= "$field";
				    } elsif ($field =~ /^textblock\s/i) {
					if ($lastappend eq "") {
					    $in_textblock = 1;
					    print "in textblock\n" if ($localDebug);
					    $lastappend = pop(@newfields);
					} else {
					    $in_textblock = 2;
					    print "in textblock (continuation)\n" if ($localDebug);
					}
					$field =~ s/^textblock\s+//i;
					# clean up text block
					$field =~ s/\</\&lt\;/g;
					$field =~ s/\>/\&gt\;/g;
					$lastappend .= "$field";
					print "in textblock.\n" if ($localDebug);
				    } elsif ($field =~ s/\\$/\@/) {
					$lastappend .= $field;
				    } elsif ($lastappend eq "") {
					push(@newfields, $field);
				    } else {
					$lastappend .= $field;
					push(@newfields, $lastappend);	
					$lastappend = "";
				    }
				  }
				}
				if (!($lastappend eq "")) {
				    push(@newfields, $lastappend);
				}
				if ($in_link) {
					warn "$filename:$linenum:Unterminated \@link tag\n";
				}
				if ($in_textblock) {
					warn "$filename:$linenum:Unterminated \@textblock tag\n";
				}
				@fields = @newfields;
				if ($inCPPHeader) {print "inCPPHeader\n" if ($debugging); $HeaderDoc::sublang="cpp"; &processCPPHeaderComment();};
				if ($inOCCHeader) {print "inCPPHeader\n" if ($debugging); $HeaderDoc::sublang="occ"; &processCPPHeaderComment();};
				if ($inPerlScript) {print "inPerlScript\n" if ($debugging); &processCPPHeaderComment(); $lang="php";};
				if ($inPHPScript) {print "inPHPScript\n" if ($debugging); &processCPPHeaderComment(); $lang="php";};
				if ($inJavaSource) {print "inJavaSource\n" if ($debugging); &processCPPHeaderComment(); $lang="java";};
				if ($inClass) {
					$functionGroup = "";
					$HeaderDoc::globalGroup = "";
					my $classdec = "";
					my $pos=$inputCounter;
					do {
						$classdec .= $inputLines[$pos];
					} while (($pos <= $#inputLines) && ($inputLines[$pos++] !~ /(\{|\@interface|\@class)/));
					$classType = &determineClassType($inputCounter, $apiOwner, \@inputLines);
					print "CLASS TYPE CHANGED TO $classType\n" if ($ctdebug);
					if ($classType eq "C") {
					    # $cppAccessControlState = "public:";
					    $cppAccessControlState = "";
					}
					print "inClass 1 - $classType \n" if ($debugging); 
					$apiOwner = &processClassComment($apiOwner, $rootOutputDir, \@fields, $classType, $inputCounter + $blockOffset);
					if ($inInterface) {
						$inInterface = 0;
						$apiOwner->isCOMInterface(1);
						$apiOwner->tocTitlePrefix('COM Interface:');
					}
					my $superclass = &get_super($classType, $classdec);
					if (length($superclass) && (!($apiOwner->checkShortLongAttributes("Superclass")))) {
					    $apiOwner->attribute("Superclass", $superclass, 0);
					}
					print "inClass 2\n" if ($debugging); 
				};
				if ($inHeader) {
					print "inHeader\n" if ($debugging); 
					$functionGroup = "";
					$HeaderDoc::globalGroup = "";
					$apiOwner = &processHeaderComment($apiOwner, $rootOutputDir, \@fields);
					$headerDoc::currentClass = $apiOwner;
					if ($reprocess_input == 1) {
					    my @cookedInputLines;
					    my $localDebug = 0;

					    foreach my $line (@rawInputLines) {
						foreach my $prefix (@perHeaderIgnorePrefixes) {
						    if ($line =~ s/^\s*$prefix\s*//g) {
							print "ignored $prefix\n" if ($localDebug);
						    }
						}
						push(@cookedInputLines, $line);
					    }
					    @rawInputLines = @cookedInputLines;
					    $reprocess_input = 2;
					    goto REDO;
					}
				};
				if ($inGroup) {
					print "inGroup\n" if ($debugging); 
					my $name = $line;
					$name =~ s/.*\/\*!\s+\@group\s*//i;
					$name =~ s/\s*\*\/.*//;
					$name =~ s/\n//smg;
					$name =~ s/^\s+//smg;
					$name =~ s/\s+$//smg;
					print "group name is $name\n" if ($debugging);
					$HeaderDoc::globalGroup = $name;
					$inputCounter--;
				};
				if ($inFunctionGroup) {
					print "inFunctionGroup\n" if ($debugging); 
					my $name = $line;
					if (!($name =~ s/.*\/\*!\s+\@functiongroup\s*//i)) {
						$name =~ s/.*\/\*!\s+\@methodgroup\s*//i;
						print "inMethodGroup\n" if ($debugging);
					}
					$name =~ s/\s*\*\/.*//;
					$name =~ s/\n//smg;
					$name =~ s/^\s+//smg;
					$name =~ s/\s+$//smg;
					print "group name is $name\n" if ($debugging);
					$functionGroup = $name;
					$inputCounter--;
				};

				if ($inMethod && !$methods_with_new_parser) {
				    my $methodDebug = 0;
					print "inMethod $line\n" if ($methodDebug);
					$methObj = HeaderDoc::Method->new;
					$methObj->linenum($inputCounter+$blockOffset);
					$methObj->apiOwner($apiOwner);
					if ($xml_output) {
					    $methObj->outputformat("hdxml");
					} else { 
					    $methObj->outputformat("html");
					}
					if (length($functionGroup)) {
						$methObj->group($functionGroup);
					} else {
						$methObj->group($HeaderDoc::globalGroup);
					}
					$methObj->processMethodComment(\@fields);
					$methObj->filename($filename);
	 				while (($inputLines[$inputCounter] !~ /\w/)  && ($inputCounter <= $#inputLines)){ 
	 					$inputCounter++;
						warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "method", "9");
	 					print "Input line number: $inputCounter\n" if ($localDebug);
					}; # move beyond blank lines

					my $declaration = $inputLines[$inputCounter];
					if ($declaration !~ /;[^;]*$/) { # search for semicolon end, even with trailing comment
						do { 
							warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "method", "10");
							$inputCounter++;
							print "Input line number: $inputCounter\n" if ($localDebug);
							$declaration .= $inputLines[$inputCounter];
						} while (($declaration !~ /;[^;]*$/)  && ($inputCounter <= $#inputLines))
					}
					$declaration =~ s/^\s+//g;				# trim leading spaces.
					$declaration =~ s/([^;]*;).*$/$1/s;		# trim anything following the final semicolon, 
															# including comments.
					$declaration =~ s/\s+;/;/;		        # trim spaces before semicolon.
					
					print " --> setting method declaration: $declaration\n" if ($methodDebug);
					$methObj->setMethodDeclaration($declaration, $classType);

					if (length($methObj->name())) {
						if (ref($apiOwner) ne "HeaderDoc::Header") {
						    if ($classType eq "occ" ||
							$classType eq "intf" ||
							$classType eq "occCat") {
							    $methObj->accessControl($objcAccessControlState);
						    } else {
							    $methObj->accessControl($cppAccessControlState);
						    }
						}
						$apiOwner->addToMethods($methObj);
						$methObj->apiOwner($apiOwner); # methods need to know the class/protocol they belong to
						print "added method $declaration\n" if ($localDebug);
					}
				}

				if ($inUnknown || $inTypedef || $inStruct || $inEnum || $inUnion || $inConstant || $inVar || $inFunction || ($inMethod && $methods_with_new_parser) || $inPDefine) {
				    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
					$soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
					$typedefname, $varname, $constname, $structisbrace, $macronameref)
					= parseTokens($lang, $HeaderDoc::sublang);
				    my $varIsConstant = 0;
				    my $blockmode = 0;
				    my $curtype = "";
				    my $blockDebug = 0;
				    my $parmDebug = 0;
				    # my $localDebug = 1;

				    if ($inPDefine == 2) { $blockmode = 1; }
				    if ($inFunction || $inMethod) {
					print "INFUNCTION\n" if ($blockDebug);
					# @@@ FIXME DAG (OBJC)
					my $method = 0;
					if ($classType eq "occ" ||
						$classType eq "intf" ||
						$classType eq "occCat") {
							$method = 1;
					}
					if ($method) {
						$curObj = HeaderDoc::Method->new;
						$curtype = "method";
					} else {
						$curObj = HeaderDoc::Function->new;
						$curtype = "function";
					}
					$curObj->apiOwner($apiOwner);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
					if (length($functionGroup)) {
						$curObj->group($functionGroup);
					} else {
						$curObj->group($HeaderDoc::globalGroup);
					}
					if ($method) {
						$curObj->processMethodComment(\@fields);
					} else {
						$curObj->processFunctionComment(\@fields);
					}
					$curObj->filename($filename);
				    } elsif ($inPDefine) {
					$curtype = "#define";
					$curObj = HeaderDoc::PDefine->new;
					$curObj->group($HeaderDoc::globalGroup);
					$curObj->apiOwner($apiOwner);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
					$curObj->processPDefineComment(\@fields);
					$curObj->filename($filename);
				    } elsif ($inVar) {
# print "inVar!!\n";
					$curtype = "constant";
					$varIsConstant = 0;
					$curObj = HeaderDoc::Var->new;
					$curObj->group($HeaderDoc::globalGroup);
					$curObj->apiOwner($apiOwner);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
					$curObj->processVarComment(\@fields);
					$curObj->filename($filename);
				    } elsif ($inConstant) {
					$curtype = "constant";
					$varIsConstant = 1;
					$curObj = HeaderDoc::Constant->new;
					$curObj->group($HeaderDoc::globalGroup);
					$curObj->apiOwner($apiOwner);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
					$curObj->processConstantComment(\@fields);
					$curObj->filename($filename);
				    } elsif ($inUnknown) {
					$curtype = "UNKNOWN";
					$curObj = HeaderDoc::HeaderElement->new;
					$curObj->group($HeaderDoc::globalGroup);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
					warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "typedef", "11");
				    } elsif ($inTypedef) {
# print "inTypedef\n"; $localDebug = 1;
					$curtype = $typedefname;
					# if ($lang eq "pascal") {
						# $curtype = "type";
					# } else {
						# $curtype = "typedef";
					# }
					$curObj = HeaderDoc::Typedef->new;
					$curObj->group($HeaderDoc::globalGroup);
					$curObj->apiOwner($apiOwner);
					if ($xml_output) {
					    $curObj->outputformat("hdxml");
					} else { 
					    $curObj->outputformat("html");
					}
					$curObj->processTypedefComment(\@fields);
					$curObj->masterEnum(0);
					$curObj->filename($filename);
					
					warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "typedef", "11a");
					# if a struct declaration precedes the typedef, suck it up
				} elsif ($inStruct || $inUnion) {
					if ($inUnion) {
						$curtype = "union";
					} else {
						$curtype = "struct";
					}
                                        $curObj = HeaderDoc::Struct->new;
					$curObj->group($HeaderDoc::globalGroup);
                                        $curObj->apiOwner($apiOwner);
                                        if ($inUnion) {     
                                            $curObj->isUnion(1);
                                        } else {
                                            $curObj->isUnion(0);
                                        }
                                        if ($xml_output) {
                                            $curObj->outputformat("hdxml");
                                        } else {
                                            $curObj->outputformat("html");
                                        }
                                        $curObj->processStructComment(\@fields);
                                        $curObj->filename($filename);
					warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "typedef", "11b");
				} elsif ($inEnum) {
					$curtype = "enum";
                                        $curObj = HeaderDoc::Enum->new;
					$curObj->masterEnum(1);
					$curObj->group($HeaderDoc::globalGroup);
                                        $curObj->apiOwner($apiOwner);
                                        if ($xml_output) {
                                            $curObj->outputformat("hdxml");
                                        } else {
                                            $curObj->outputformat("html");
                                        }
                                        $curObj->processEnumComment(\@fields);
                                        $curObj->filename($filename);
					warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "typedef", "11c");
				}
				if ($curObj) {
					$curObj->linenum($inputCounter+$blockOffset);
				}
                                while (($inputLines[$inputCounter] !~ /\w/)  && ($inputCounter <= $#inputLines)){
                                	# print "BLANKLINE IS $inputLines[$inputCounter]\n";
                                	$inputCounter++;
                                	warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "typedef", "12");
                                	print "Input line number: $inputCounter\n" if ($localDebug);
                                };
                                # my  $declaration = $inputLines[$inputCounter];

print "NEXT LINE is ".$inputLines[$inputCounter].".\n" if ($localDebug);

	my $outertype = ""; my $newcount = 0; my $declaration = ""; my $namelist = "";

	my $typelist = ""; my $innertype = ""; my $posstypes = "";
	print "PTCT: $posstypes =? $curtype\n" if ($localDebug || $blockDebug);
	my $blockDec = "";
	my $hangDebug = 0;
	while (($blockmode || ($outertype ne $curtype && $innertype ne $curtype && $posstypes !~ /$curtype/)) && ($inputCounter <= $#inputLines)) { # ($typestring !~ /$typedefname/)

		if ($hangDebug) { print "In Block Loop\n"; }

		# while ($inputLines[$inputCounter] !~ /\S/ && ($inputCounter <= $#inputLines)) { $inputCounter++; }
		# if (warnHDComment($inputLines[$inputCounter], $inputCounter, "blockParse:$outertype", "18b")) {
			# last;
		# } else { print "OK\n"; }
		print "DOING SOMETHING\n" if ($localDebug);
		$HeaderDoc::ignore_apiuid_errors = 1;
		my $junk = $curObj->documentationBlock();
		$HeaderDoc::ignore_apiuid_errors = 0;
		# the value of a constant
		my $value = "";
		my $pplref = undef;
		my $returntype = undef;
		my $pridec = "";
		my $parseTree = undef;
		print "Entering blockParse\n" if ($hangDebug);
		($newcount, $declaration, $typelist, $namelist, $posstypes, $value, $pplref, $returntype, $pridec, $parseTree) = &blockParse($filename, $blockOffset, \@inputLines, $inputCounter, 0, \@ignorePrefixes, \@perHeaderIgnorePrefixes);
		print "Left blockParse\n" if ($hangDebug);

		if ($outertype eq $curtype || $innertype eq $curtype || $posstypes =~ /$curtype/) {
			# Make sure we have the right UID for methods
			$curObj->declaration($declaration);
			$HeaderDoc::ignore_apiuid_errors = 1;
			my $junk = $curObj->documentationBlock();
			$HeaderDoc::ignore_apiuid_errors = 0;
		}
		$curObj->privateDeclaration($pridec);
		$parseTree->apiowner($curObj);
		$curObj->parseTree($parseTree);
		# print "PT IS A $parseTree\n";
		# $parseTree->htmlTree();

		my @parsedParamList = @{$pplref};
		print "VALUE IS $value\n" if ($localDebug);
		# warn("nc: $newcount.  ts: $typestring.  nl: $namelist\nBEGIN:\n$declaration\nEND.\n");

		my $method = 0;
		if ($classType eq "occ" ||
			$classType eq "intf" ||
			$classType eq "occCat") {
				$method = 1;
		}

		$declaration =~ s/^\s*//s;
		# if (!length($declaration)) { next; }

	print "obtained declaration\n" if ($localDebug);

		$inputCounter = $newcount;

		my @oldnames = split(/[,;]/, $namelist);
		my @oldtypes = split(/ /, $typelist);

		$outertype = $oldtypes[0];
		if ($outertype eq "") {
			$outertype = $curtype;
			my $linenum = $inputCounter + $blockOffset;
			warn("$filename:$linenum:Parser bug: empty outer type\n");
			warn("IC: $inputCounter\n");
			warn("DC: \"$declaration\"\n");
			warn("TL: \"$typelist\"\n");
			warn("DC: \"$namelist\"\n");
			warn("DC: \"$posstypes\"\n");
		}
		$innertype = $oldtypes[scalar(@oldtypes)-1];

		if ($localDebug) {
			foreach my $ot (@oldtypes) {
				print "TYPE: \"$ot\"\n";
			}
		}

		my $explicit_name = 1;
		my $curname = $curObj->name();
		$curname =~ s/^\s*//;
		$curname =~ s/\s*$//;
		my @names = ( ); #$curname
		my @types = ( ); #$outertype
		if (length($curname) && length($curtype)) {
			push(@names, $curname);
			push(@types, $outertype);
		}
		my $count = 0;
		my $operator = 0;
		if ($typelist eq "operator") {
			$operator = 1;
		}
		foreach my $name (@oldnames) {
			if ($operator) {
				$name =~ s/^\s*operator\s*//s;
				$name = "operator $name";
				$curname =~ s/^operator(\W)/operator $1/s;
			}
			if (($name eq $curname) && ($oldtypes[$count] eq $outertype)) {
				$explicit_name = 0;
				$count++;
			} else {
				push(@names, $name);
				push(@types, $oldtypes[$count++]);
			}
		}
		if ($hangDebug) { print "Point A\n"; }
		# $explicit_name = 0;
		my $count = 0;
		foreach my $name (@names) {
		    my $localDebug = 0;
		    my $typestring = $types[$count++];

		    print "NAME IS $name\n" if ($localDebug);
		    print "CURNAME IS $curname\n" if ($localDebug);
		    print "TYPESTRING IS $typestring\n" if ($localDebug);
		    print "CURTYPE IS $curtype\n" if ($localDebug);
			print "MATCH: $name IS A $typestring.\n" if ($localDebug);

print "DEC ($name / $typestring): $declaration\n" if ($localDebug);

		    $name =~ s/\s*$//g;
		    $name =~ s/^\s*//g;
		    my $cmpname = $name;
		    my $cmpcurname = $curname;
		    $cmpname =~ s/:$//s;
		    $cmpcurname =~ s/:$//s;
		    if (!length($name)) { next; }
			print "Got $name ($curname)\n" if ($localDebug);

			my $extra = undef;

			if ($typestring eq $curtype && ($cmpname eq $cmpcurname || !length($curname))) {
				print "$curtype = $typestring\n" if ($localDebug);
				$extra = $curObj;
# print "E=C\n$extra\n$curObj\n";
				if ($blockmode) {
					$blockDec .= $declaration;
					print "SPDF[1]\n" if ($hangDebug);
					$curObj->isBlock(1);
					$curObj->setPDefineDeclaration($blockDec);
					print "END SPDF[1]\n" if ($hangDebug);
					# $declaration = $curObj->declaration() . $declaration;
				}
			} else {
				print "NAME IS $name\n" if ($localDebug);
			    if ($curtype eq "function" && $posstypes =~ /function/) {
				$curtype = "UNKNOWN";
			    }
			    if ($typestring eq $outertype || !$HeaderDoc::outerNamesOnly) {
				if ($typestring =~ /^$typedefname/ && length($typedefname)) {
					print "blockParse returned $typedefname\n" if ($localDebug);
					$extra = HeaderDoc::Typedef->new;
					$extra->group($HeaderDoc::globalGroup);
					$curObj->masterEnum(1);
					if ($curtype eq "UNKNOWN") {
						$extra->processTypedefComment(\@fields);
					}
				} elsif ($typestring =~ /^struct/ || $typestring =~ /^union/ || ($lang eq "pascal" && $typestring =~ /^record/)) {
					print "blockParse returned struct or union ($typestring)\n" if ($localDebug);
					$extra = HeaderDoc::Struct->new;
					$extra->group($HeaderDoc::globalGroup);
					if ($typestring =~ /union/) {
						$extra->isUnion(1);
					}
					if ($curtype eq "UNKNOWN") {
						$extra->processStructComment(\@fields);
					}
				} elsif ($typestring =~ /^enum/) {
					print "blockParse returned enum\n" if ($localDebug);
					$extra = HeaderDoc::Enum->new;
					$extra->group($HeaderDoc::globalGroup);
					if ($curtype eq "UNKNOWN") {
						$extra->processEnumComment(\@fields);
					}
					if ($curtype eq "enum" || $curtype eq "typedef") {
						$extra->masterEnum(0);
					} else {
						$extra->masterEnum(1);
					}
				} elsif ($typestring =~ /^MACRO/) {
					print "blockParse returned MACRO\n" if ($localDebug);
					# silently ignore this noise.
				} elsif ($typestring =~ /^\#define/) {
					print "blockParse returned #define\n" if ($localDebug);
					$extra = HeaderDoc::PDefine->new;
					$extra->group($HeaderDoc::globalGroup);
					if ($curtype eq "UNKNOWN") {
						$extra->processPDefineComment(\@fields);
					}
				} elsif ($typestring =~ /^constant/) {
					if ($declaration =~ /\s+const\s+/) {
						$varIsConstant = 1;
						print "blockParse returned constant\n" if ($localDebug);
						$extra = HeaderDoc::Constant->new;
						$extra->group($HeaderDoc::globalGroup);
						if ($curtype eq "UNKNOWN") {
							$extra->processConstantComment(\@fields);
						}
					} else {
						$varIsConstant = 0;
						print "blockParse returned variable\n" if ($localDebug);
						$extra = HeaderDoc::Var->new;
						$extra->group($HeaderDoc::globalGroup);
						if ($curtype eq "UNKNOWN") {
							$extra->processVarComment(\@fields);
						}
					}
				} elsif ($typestring =~ /^(function|method|operator|ftmplt)/) {
					print "blockParse returned function or method\n" if ($localDebug);
					if ($method) {
						$extra = HeaderDoc::Method->new;
						if ($curtype eq "UNKNOWN") {
							$extra->processMethodComment(\@fields);
						}
						if (length($functionGroup)) {
							$extra->group($functionGroup);
						} else {
							$extra->group($HeaderDoc::globalGroup);
						}
					} else {
						$extra = HeaderDoc::Function->new;
						if (length($functionGroup)) {
							$extra->group($functionGroup);
						} else {
							$extra->group($HeaderDoc::globalGroup);
						}
						if ($curtype eq "UNKNOWN") {
							$extra->processFunctionComment(\@fields);
						}
					}
					if ($typestring eq "ftmplt") {
						$extra->isTemplate(1);
					}
				} else {
					my $linenum = $inputCounter + $blockOffset;
					warn("$filename:$linenum:Unknown keyword $typestring in block-parsed declaration\n");
				}
				if ($extra) {
					$extra->linenum($inputCounter+$blockOffset);
				}
			    }
			}
			if ($hangDebug) { print "Point B\n"; }
			if ($curtype eq "UNKNOWN") {
				$curObj = $extra;
			}
			if ($extra) {
				my $extraclass = ref($extra) || $extra;
				my $abstract = $curObj->abstract();
				my $discussion = $curObj->discussion();
				my $pridec = $curObj->privateDeclaration();
				$extra->privateDeclaration($pridec);

				my $orig_parsetree = $curObj->parseTree($parseTree);
				bless($orig_parsetree, "HeaderDoc::ParseTree");
				$extra->parseTree($orig_parsetree->clone());

				if ($blockmode) {
					my $discussionParam = $curObj->taggedParamMatching($name);
					# print "got $discussionParam\n";
					if ($discussionParam) {
						$discussion = $discussionParam->discussion;
					}
					if ($curObj != $extra) {
						# we use the parsed parms to
						# hold subdefines.
						$curObj->addParsedParameter($extra);
					}
				}

				print "Point B1\n" if ($hangDebug);
				if ($extraclass !~ /HeaderDoc::Method/) {
					print "Point B2\n" if ($hangDebug);
					my $paramName = "";
					my $position = 0;
					my $type = "";
					$extra->returntype($returntype);
					my @tempPPL = @parsedParamList;
					foreach my $parsedParam (@tempPPL) {
					    if (0) {
						# temp code
						print "PARSED PARAM: \"$parsedParam\"\n" if ($parmDebug);
						if ($parsedParam =~ s/(\w+\)*)$//s) {
							$paramName = $1;
						} else {
							$paramName = "";
						}

						$parsedParam =~ s/\s*$//s;
						if (!length($parsedParam)) {
							$type = $paramName;
							$paramName = "";
						} else {
							$type = $parsedParam;
						}
						print "NAME: $paramName\nType: $type\n" if ($parmDebug);

						my $param = HeaderDoc::MinorAPIElement->new();
						$param->linenum($inputCounter+$blockOffset);
						$param->outputformat($extra->outputformat);
						$param->name($paramName);
						$param->position($position++);
						$param->type($type);
						$extra->addParsedParameter($param);
					    } else {
						# the real code
						my $ppDebug = 0 || $parmDebug;

						print "PARSED PARAM: \"$parsedParam\"\n" if ($ppDebug);

						my $ppstring = $parsedParam;
						$ppstring =~ s/^\s*//sg;
						$ppstring =~ s/\s*$//sg;

						my $foo;
						my $dec;
						my $pridec;
						my $type;
						my $name;
						my $pt;
						my $value;
						my $pplref;
						my $returntype;
						if ($ppstring eq "...") {
							$name = $ppstring;
							$type = "";
							$pt = "";
						} else {
							$ppstring .= ";";
							my @array = ( $ppstring );

							my $parseTree = undef;
							($foo, $dec, $type, $name, $pt, $value, $pplref, $returntype, $pridec, $parseTree) = &blockParse($filename, $extra->linenum(), \@array, 0, 1, \@ignorePrefixes, \@perHeaderIgnorePrefixes);
						}
						if ($ppDebug) {
							print "NAME: $name\n";
							print "TYPE: $type\n";
							print "PT:   $pt\n";
						}
						
						my $param = HeaderDoc::MinorAPIElement->new();
						$param->linenum($inputCounter+$blockOffset);
						$param->outputformat($extra->outputformat);
						$param->name($name);
						$param->position($position++);
						$param->type($returntype);
						$extra->addParsedParameter($param);
					    }
					}
				} else {
					# we're a method
					my @newpps = $parseTree->objCparsedParams();
					foreach my $newpp (@newpps) {
						$extra->addParsedParameter($newpp);
					}
				}
				print "Point B3\n" if ($hangDebug);
				if (length($preAtPart)) {
					print "preAtPart: $preAtPart\n" if ($localDebug);
					$extra->discussion($preAtPart);
				} elsif ($extra != $curObj) {
					# Otherwise this would be bad....
					$extra->discussion($discussion);

				}
				print "Point B4\n" if ($hangDebug);
				$extra->abstract($abstract);
				if (length($value)) { $extra->value($value); }
				if ($extra != $curObj || !length($curObj->name())) {
					$name =~ s/^(\s|\*)*//sg;
				}
				print "NAME IS $name\n" if ($localDebug);
				$extra->rawname($name);
				# my $namestring = $curObj->name();
				# if ($explicit_name && 0) {
					# $extra->name("$name ($namestring)");
				# } else {
					# $extra->name($name);
				# }
				print "Point B5\n" if ($hangDebug);
				$HeaderDoc::ignore_apiuid_errors = 1;
				$extra->name($name);
				my $junk = $extra->documentationBlock();
				$HeaderDoc::ignore_apiuid_errors = 0;


				# print "NAMES: \"".$curObj->name()."\" & \"".$extra->name()."\"\n";
				# print "ADDYS: ".$curObj." & ".$extra."\n";

				if ($extra != $curObj) {
				    my @constants = $curObj->constants();
				    foreach my $constant (@constants) {
					# print "CONSTANT $constant\n";
					if ($extra->can("addToConstants")) {
					    $extra->addToConstants($constant->clone());
					    # print "ATC\n";
					} elsif ($extra->can("addConstant")) {
					    $extra->addConstant($constant->clone());
					    # print "AC\n";
					}
				    }

				    print "Point B6\n" if ($hangDebug);
				    if (length($curObj->name())) {
	# my $a = $extra->rawname(); my $b = $curObj->rawname(); my $c = $curObj->name();
	# print "EXTRA RAWNAME: $a\nCUROBJ RAWNAME: $b\nCUROBJ NAME: $c\n";
					# change whitespace to ctrl-d to
					# allow multi-word names.
					my $ern = $extra->rawname();
					if ($ern =~ /\s/ && $localDebug) {
						print "changed space to ctrl-d\n";
						print "ref is ".$extra->apiuid()."\n";
					}
					$ern =~ s/\s/\cD/sg;
					my $crn = $curObj->rawname();
					if ($crn =~ /\s/ && $localDebug) {
						print "changed space to ctrl-d\n";
						print "ref is ".$curObj->apiuid()."\n";
					}
					$crn =~ s/\s/\cD/sg;
					$curObj->attributelist("See Also", $ern." ".$extra->apiuid());
					$extra->attributelist("See Also", $crn." ".$curObj->apiuid());
				    }
				}
				print "Point B7 TS = $typestring\n" if ($hangDebug);
				if (ref($apiOwner) ne "HeaderDoc::Header") {
					$extra->accessControl($cppAccessControlState); # @@@ FIXME DAG CHECK FOR OBJC
				}
				if ($curObj->can("fields") && $extra->can("fields")) {
					my @fields = $curObj->fields();

					foreach my $field (@fields) {
						bless($field, "HeaderDoc::MinorAPIElement");
						$extra->addField($field->clone());
					}
				}
				$extra->apiOwner($apiOwner);
				if ($xml_output) {
				    $extra->outputformat("hdxml");
				} else { 
				    $extra->outputformat("html");
				}
				$extra->filename($filename);

		# warn("Added ".$extra->name()." ".$extra->apiuid().".\n");

				if ($typestring =~ /$typedefname/ && length($typedefname)) {
	                		if (length($declaration)) {
                        			$extra->setTypedefDeclaration($declaration);
					}
						if (length($extra->name())) {
							if (ref($apiOwner) ne "HeaderDoc::Header") {
								if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToVars($extra); }
							} else { # headers group by type
								if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToTypedefs($extra); }
					    	}
					}
				} elsif ($typestring =~ /MACRO/) {
					# throw these away.
					# $extra->setPDefineDeclaration($declaration);
					# $apiOwner->addToPDefines($extra);
				} elsif ($typestring =~ /#define/) {
					print "SPDF[2]\n" if ($hangDebug);
					$extra->setPDefineDeclaration($declaration);
					print "END SPDF[2]\n" if ($hangDebug);
					if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToPDefines($extra); }
				} elsif ($typestring =~ /struct/ || $typestring =~ /union/ || ($lang eq "pascal" && $typestring =~ /record/)) {
					if ($typestring =~ /union/) {
						$extra->isUnion(1);
					} else {
						$extra->isUnion(0);
					}
					# $extra->declaration($declaration);
# print "PRE (DEC IS $declaration)\n";
					$extra->setStructDeclaration($declaration);
# print "POST\n";
					if (ref($apiOwner) ne "HeaderDoc::Header") {
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToVars($extra); }
					} else {
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToStructs($extra); }
					}
				} elsif ($typestring =~ /enum/) {
					$extra->declaration($declaration);
					$extra->declarationInHTML($extra->getEnumDeclaration($declaration));
					if (ref($apiOwner) ne "HeaderDoc::Header") {
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToVars($extra); }
					} else {
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToEnums($extra); }
					}
				} elsif ($typestring =~ /\#define/) {
					print "SPDF[3]\n" if ($hangDebug);
					$extra->setPDefineDeclaration($declaration);
					print "END SPDF[3]\n" if ($hangDebug);
					if (($blockmode != 2) || ($extra != $curObj)) { $headerObject->addToPDefines($extra); }
				} elsif ($typestring =~ /(function|method|operator|ftmplt)/) {
					if ($method) {
						$extra->setMethodDeclaration($declaration);
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToMethods($extra); }
					} else {
						print "SFD\n" if ($hangDebug);
						$extra->setFunctionDeclaration($declaration);
						print "END SFD\n" if ($hangDebug);
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToFunctions($extra); }
					}
					if ($typestring eq "ftmplt") {
						$extra->isTemplate(1);
					}
				} elsif ($typestring =~ /constant/) {
					$extra->declaration($declaration);
					if ($varIsConstant) {
					    $extra->setConstantDeclaration($declaration);
                                            if (length($extra->name())) {
                                                    if (ref($apiOwner) ne "HeaderDoc::Header") {
                                                        $extra->accessControl($cppAccessControlState);
                                                        if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToVars($extra); }
                                                    } else { # headers group by type
                                                            if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToConstants($extra); }
                                                }
                                            }
					} else {
						$extra->setVarDeclaration($declaration);
                                                if (ref($apiOwner) ne "HeaderDoc::Header") {
                                                    $extra->accessControl($cppAccessControlState);
						}
						if (($blockmode != 2) || ($extra != $curObj)) { $apiOwner->addToVars($extra); }
					}
				} else {
					my $linenum = $inputCounter + $blockOffset;
					warn("$filename:$linenum:Unknown typestring $typestring returned by blockParse\n");
				}
				$extra->checkDeclaration();
			}
		}
		if ($hangDebug) {
			print "Point C\n";
			print "inputCounter is $inputCounter, #inputLines is $#inputLines\n";
		}

		while ($inputLines[$inputCounter] !~ /\S/ && ($inputCounter <= $#inputLines)) { $inputCounter++; }
		if ($hangDebug) { print "Point D\n"; }
		if ($curtype eq "UNKNOWN") { $curtype = $outertype; }
		if ((($outertype ne $curtype && $innertype ne $curtype && $posstypes !~ /$curtype/)) && (($inputCounter > $#inputLines) || warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "blockParse:$outertype", "18a"))) {
			warn "No matching declaration found.  Last name was $curname\n";
			warn "$outertype ne $curtype && $innertype ne $curtype && $posstypes !~ $curtype\n";
			last;
		}
		if ($hangDebug) { print "Point E\n"; }
		if ($blockmode) {
			warn "next line: ".$inputLines[$inputCounter]."\n" if ($hangDebug);
			$blockmode = 2;
			$HeaderDoc::ignore_apiuid_errors = 1;
			if (warnHDComment($inputLines[$inputCounter], $inputCounter + $blockOffset, "blockParse:$outertype", "18a")) {
				$blockmode = 0;
				warn "Block Mode Ending\n" if ($hangDebug);
			}
			$HeaderDoc::ignore_apiuid_errors = 0;
		}
		print "PTCT: $posstypes =? $curtype\n" if ($localDebug || $blockDebug);
	}
	if (length($blockDec)) {
		$curObj->declaration($blockDec);
		$curObj->declarationInHTML($blockDec);
	}
	if ($hangDebug) { print "Point F\n"; }
	print "Out of Block\n" if ($localDebug || $blockDebug);
	# the end of this block assumes that inputCounter points
	# to the last line grabbed, but right now it points to the
	# next line available.  Back it up by one.
	$inputCounter--;
	# warn("NEWDEC:\n$declaration\nEND NEWDEC\n");

				}  ## end blockParse handler
				
	        }
			$inCPPHeader = $inOCCHeader = $inPerlScript = $inShellScript = $inPHPScript = $inJavaSource = $inHeader = $inUnknown = $inFunction = $inFunctionGroup = $inGroup = $inTypedef = $inUnion = $inStruct = $inConstant = $inVar = $inPDefine = $inEnum = $inMethod = $inClass = 0;
	        $inputCounter++;
		print "Input line number: $inputCounter\n" if ($localDebug);
	    } # end processing individual line array
	    
	    if (ref($apiOwner) ne "HeaderDoc::Header") { # if we've been filling a class/protocol/category object, add it to the header
	        my $name = $apiOwner->name();
	        my $refName = ref($apiOwner);

			# print "$classType : ";
			SWITCH: {
				($classType eq "php" ) && do { 
					push (@classObjects, $apiOwner);
					$headerObject->addToClasses($apiOwner); 
					last SWITCH; };
				($classType eq "java" ) && do { 
					push (@classObjects, $apiOwner);
					$headerObject->addToClasses($apiOwner); 
					last SWITCH; };
				($classType eq "cpp" ) && do { 
					push (@classObjects, $apiOwner);
					$headerObject->addToClasses($apiOwner); 
					last SWITCH; };
				($classType eq "cppt" ) && do { 
					push (@classObjects, $apiOwner);
					$headerObject->addToClasses($apiOwner); 
					last SWITCH; };
				($classType eq "occ") && do { 
					push (@classObjects, $apiOwner);
					$headerObject->addToClasses($apiOwner); 
					$objCClassNameToObject{$apiOwner->name()} = $apiOwner;
					last SWITCH; };           
				($classType eq "intf") && do { 
					push (@classObjects, $apiOwner);
					$headerObject->addToProtocols($apiOwner); 
					last SWITCH; 
				};           
				($classType eq "occCat") && do {
					push (@categoryObjects, $apiOwner);
					print "INSERTED CATEGORY into $headerObject\n" if ($ctdebug);
					$headerObject->addToCategories($apiOwner);
					last SWITCH; 
				};           
				($classType eq "C") && do {
					# $cppAccessControlState = "public:";
					$cppAccessControlState = "";
					push (@classObjects, $apiOwner);
					$headerObject->addToClasses($apiOwner);
					last SWITCH;
				};
foreach my $testclass ( $headerObject->classes() ) { print $testclass->name() . "\n"; }
			my $linenum = $inputCounter - 1;
                    	print "$filename:$linenum:Unknown class type '$classType' (known: cpp, objC, intf, occCat)\n";		
			}
	    }
    } # end processing array of line arrays
    push (@headerObjects, $headerObject);
}

if (!$quietLevel) {
    print "======= Beginning post-processing =======\n";
}

if (@classObjects && !$xml_output) {
    foreach my $class (@classObjects) {
	mergeClass($class);
    }
}

# we merge ObjC methods declared in categories into the owning class,
# if we've seen it during processing
if (@categoryObjects && !$xml_output) {
    foreach my $obj (@categoryObjects) {
        my $nameOfAssociatedClass = $obj->className();
        my $categoryName = $obj->categoryName();
        my $localDebug = 0;
        
		if (exists $objCClassNameToObject{$nameOfAssociatedClass}) {
			my $associatedClass = $objCClassNameToObject{$nameOfAssociatedClass};
			$associatedClass->addToMethods($obj->methods());
			
			my $owner = $obj->headerObject();
			
			print "Found category with name $categoryName and associated class $nameOfAssociatedClass\n" if ($localDebug);
			print "Associated class exists\n" if ($localDebug);
			print "Added methods to associated class\n" if ($localDebug);
			if (ref($owner)) {
			    my $numCatsBefore = $owner->categories();
			    # $owner->printObject();
			    $owner->removeFromCategories($obj);
			    my $numCatsAfter = $owner->categories();
				print "Number of categories before: $numCatsBefore after:$numCatsAfter\n" if ($localDebug);
			    
			} else {
				my $filename = $HeaderDoc::headerObject->filename();
				my $linenum = $obj->linenum();
                    		print "$filename:$linenum:Couldn't find Header object that owns the category with name $categoryName.\n";
			}
		} else {
			print "Found category with name $categoryName and associated class $nameOfAssociatedClass\n" if ($localDebug);
			print "Associated class doesn't exist\n" if ($localDebug);
        }
    }
}

foreach my $obj (@headerObjects) {
    if ($xml_output) {
	$obj->writeHeaderElementsToXMLPage();
    } else {
	$obj->createFramesetFile();
	$obj->createTOCFile();
	$obj->writeHeaderElements(); 
	$obj->writeHeaderElementsToCompositePage();
	$obj->createContentFile();
	$obj->writeExportsWithName($rootFileName) if (($export) || ($testingExport));
    }
    if ("$write_control_file" eq "1") {
	print "Writing doc server control file... ";
	$obj->createMetaFile();
	print "done.\n";
    }
}

if ($quietLevel eq "0") {
    print "...done\n";
}
exit 0;


#############################  Subroutines ###################################

# /*! @function warnHDComment
#     @param teststring string to be checked for headerdoc markup
#     @param linenum line number
#     @param dectype declaration type
#     @param dp debug point string
#  */
sub warnHDComment
{
    my $line = shift;
    my $linenum = shift;
    my $dectype = shift;
    my $dp = shift;
    my $filename = $HeaderDoc::headerObject->filename();
    my $localDebug = 2; # Set to 2 so I wouldn't keep turning this off.

    my $debugString = "";
    if ($localDebug) { $debugString = " [debug point $dp]"; }

    if ($line =~ /\/\*\!/) {
	if (!$HeaderDoc::ignore_apiuid_errors) {
		warn("$filename:$linenum: WARNING: Unexpected headerdoc markup found in $dectype declaration$debugString.  Output may be broken.\n");
	}
	return 1;
    }
    return 0;
}


sub mergeClass
{
	my $class = shift;
	my $superName = $class->checkShortLongAttributes("Superclass");
	my $merge_content = 1;
	if ($class->isMerged()) { return; }

	# If superclass was not explicitly specified in the header and if
	# the 'S' (include all superclass documentation) flag was not
	# specified on the command line, don't include any superclass
	# documentation here.
	if (!$class->explicitSuper() && !$HeaderDoc::IncludeSuper) {
		$merge_content = 0;
	}

	if ($superName) {
	    if (!($superName eq $class->name())) {
		my $super = 0;
		foreach my $mergeclass (@classObjects) {
		    if ($mergeclass->name eq $superName) {
			$super = $mergeclass;
		    }
		}
		if ($super) {
		    if (!$super->isMerged()) {
			mergeClass($super);
		    }
		    my @methods = $super->methods();
		    my @functions = $super->functions();
		    my @vars = $super->vars();
		    my @structs = $super->structs();
		    my @enums = $super->enums();
		    my @pdefines = $super->pDefines();
		    my @typedefs = $super->typedefs();
		    my @constants = $super->constants();
		    my @classes = $super->classes();
		    my $name = $super->name();

		    my $discussion = $super->discussion();

		    $class->inheritDoc($discussion);

		    if ($merge_content) {

		        my @childfunctions = $class->functions();
		        my @childmethods = $class->methods();
		        my @childvars = $class->vars();
		        my @childstructs = $class->structs();
		        my @childenums = $class->enums();
		        my @childpdefines = $class->pDefines();
		        my @childtypedefs = $class->typedefs();
		        my @childconstants = $class->constants();
		        my @childclasses = $class->classes();

		        if (@methods) {
			    foreach my $method (@methods) {
				if ($method->accessControl() eq "private") {
					next;
				}
				my $include = 1;
				foreach my $childmethod (@childmethods) {
				    if ($method->name() eq $childmethod->name()) {
					if ($method->parsedParamCompare($childmethod)) {
						$include = 0; last;
					}
				    }
				}
				if (!$include) { next; }
				my $newobj = $method->clone();
				$class->addToMethods($method);
				$newobj->apiOwner($class);
				if ($newobj->origClass() eq "") {
					$newobj->origClass($name);
				}
			    }
		        }
		        if (@functions) {
			    foreach my $function (@functions) {
				if ($function->accessControl() eq "private") {
					next;
				}
				my $include = 1;
				foreach my $childfunction (@childfunctions) {
				    if ($function->name() eq $childfunction->name()) {
					if ($function->parsedParamCompare($childfunction)) {
						$include = 0; last;
					}
				    }
				}
				if (!$include) { next; }
				my $newobj = $function->clone();
				$class->addToFunctions($newobj);
				$newobj->apiOwner($class);
				if ($newobj->origClass() eq "") {
					$newobj->origClass($name);
				}
			    }
		        }
		        if (@vars) {
			    foreach my $var (@vars) {
				if ($var->accessControl() eq "private") {
					next;
				}
				my $include = 1;
				foreach my $childvar (@childvars) {
				    if ($var->name() eq $childvar->name()) {
					$include = 0; last;
				    }
				}
				if (!$include) { next; }
				my $newobj = $var->clone();
				$class->addToVars($newobj);
				$newobj->apiOwner($class);
				if ($newobj->origClass() eq "") {
					$newobj->origClass($name);
				}
			    }
		        }
		        if (@structs) {
			    foreach my $struct (@structs) {
				if ($struct->accessControl() eq "private") {
					next;
				}
				my $include = 1;
				foreach my $childstruct (@childstructs) {
				    if ($struct->name() eq $childstruct->name()) {
					$include = 0; last;
				    }
				}
				my $newobj = $struct->clone();
				$class->addToStructs($newobj);
				$newobj->apiOwner($class);
				if ($newobj->origClass() eq "") {
					$newobj->origClass($name);
				}
			    }
		        }
		        if (@enums) {
			    foreach my $enum (@enums) {
				if ($enum->accessControl() eq "private") {
					next;
				}
				my $include = 1;
				foreach my $childenum (@childenums) {
				    if ($enum->name() eq $childenum->name()) {
					$include = 0; last;
				    }
				}
				my $newobj = $enum->clone();
				$class->addToEnums($newobj);
				$newobj->apiOwner($class);
				if ($newobj->origClass() eq "") {
					$newobj->origClass($name);
				}
			    }
		        }
		        if (@pdefines) {
			    foreach my $pdefine (@pdefines) {
				if ($pdefine->accessControl() eq "private") {
					next;
				}
				my $include = 1;
				foreach my $childpdefine (@childpdefines) {
				    if ($pdefine->name() eq $childpdefine->name()) {
					$include = 0; last;
				    }
				}
				my $newobj = $pdefine->clone();
				$class->addToPDefines($newobj);
				$newobj->apiOwner($class);
				if ($newobj->origClass() eq "") {
					$newobj->origClass($name);
				}
			    }
		        }
		        if (@typedefs) {
			    foreach my $typedef (@typedefs) {
				if ($typedef->accessControl() eq "private") {
					next;
				}
				my $include = 1;
				foreach my $childtypedef (@childtypedefs) {
				    if ($typedef->name() eq $childtypedef->name()) {
					$include = 0; last;
				    }
				}
				my $newobj = $typedef->clone();
				$class->addToTypedefs($newobj);
				$newobj->apiOwner($class);
				if ($newobj->origClass() eq "") {
					$newobj->origClass($name);
				}
			    }
		        }
		        if (@constants) {
			    foreach my $constant (@constants) {
				if ($constant->accessControl() eq "private") {
					next;
				}
				my $include = 1;
				foreach my $childconstant (@childconstants) {
				    if ($constant->name() eq $childconstant->name()) {
					$include = 0; last;
				    }
				}
				my $newobj = $constant->clone();
				$class->addToConstants($newobj);
				$newobj->apiOwner($class);
				if ($newobj->origClass() eq "") {
					$newobj->origClass($name);
				}
			    }
		        }
		        if (@classes) {
			    foreach my $class (@classes) {
				if ($class->accessControl() eq "private") {
					next;
				}
				my $include = 1;
				foreach my $childclass (@childclasses) {
				    if ($class->name() eq $childclass->name()) {
					$include = 0; last;
				    }
				}
				my $newobj = $class->clone();
				$class->addToClasses($newobj);
				$newobj->apiOwner($class);
				if ($newobj->origClass() eq "") {
					$newobj->origClass($name);
				}
			    }
		        }
		    } # if ($merge_content)
		}
	    }
	}
	$class->isMerged(1);
}

sub emptyHDok
{
    my $line = shift;
    my $okay = 0;

    SWITCH: {
	($line =~ /\@(function|method|)group/) && do { $okay = 1; };
	($line =~ /\@language/) && do { $okay = 1; };
	($line =~ /\@header/) && do { $okay = 1; };
	($line =~ /\@framework/) && do { $okay = 1; };
	($line =~ /\@\/define(d)?block/) && do { $okay = 1; };
	($line =~ /\@lineref/) && do { $okay = 1; };
    }
    return $okay;
}

# /*! @function getLineArrays
#     @abstract splits the input files into multiple text blocks
#   */
sub getLineArrays {
# @@@
    my $classDebug = 0;
    my $localDebug = 0;
    my $blockDebug = 0;
    my $rawLineArrayRef = shift;
    my @arrayOfLineArrays = ();
    my @generalHeaderLines = ();
    my @classStack = ();
	
    my $inputCounter = 0;
    my $lastArrayIndex = @{$rawLineArrayRef};
    my $line = "";
    my $className = "";
    my $classType = "";

    while ($inputCounter <= $lastArrayIndex) {
        $line = ${$rawLineArrayRef}[$inputCounter];
        
	# inputCounter should always point to the current line being processed

        # we're entering a headerdoc comment--look ahead for @class tag
	my $startline = $inputCounter;

	print "MYLINE: $line\n" if ($localDebug);
        if (($line =~ /^\s*\/\*\!/) || (($lang eq "java") && ($line =~ /^\s*\/\*\*/))) {  # entering headerDoc comment
			print "inHDComment\n" if ($localDebug);
			my $headerDocComment = "";
			{
				local $^W = 0;  # turn off warnings since -w is overly sensitive here
				my $in_textblock = 0; my $in_pre = 0;
				while (($line !~ /\*\//) && ($inputCounter <= $lastArrayIndex)) {
				    # if ($lang eq "java") {
					$line =~ s/\{\s*\@linkplain\s+(.*?)\}/\@link $1\@\/link/sgi;
					$line =~ s/\{\s*\@link\s+(.*?)\}/<code>\@link $1\@\/link<\/code>/sgi;
					$line =~ s/\{\s*\@docroot\s*\}/\\\@\\\@docroot/gi;
					# if ($line =~ /value/) { warn "line was: $line\n"; }
					$line =~ s/\{\@value\}/\@value/sgi;
					$line =~ s/\{\@inheritDoc\}/\@inheritDoc/sgi;
					# if ($line =~ /value/) { warn "line now: $line\n"; }
				    # }
				    $line =~ s/([^\\])\@docroot/$1\\\@\\\@docroot/gi;
				    my $templine = $line;
				    while ($templine =~ s/\@textblock//i) { $in_textblock++; }
				    while ($templine =~ s/\@\/textblock//i) { $in_textblock--; }
				    while ($templine =~ s/<pre>//i) { $in_pre++; }
				    while ($templine =~ s/<\/pre>//i) { $in_pre--; }
				    if (!$in_textblock && !$in_pre) {
					$line =~ s/^[ \t]*//; # remove leading whitespace
				    }
				    $line =~ s/^[*]\s*$/\n/; # replace sole asterisk with paragraph divider
				    $line =~ s/^[*]\s+(.*)/$1/; # remove asterisks that precede text
				    $headerDocComment .= $line;
				    # warnHDComment(${$rawLineArrayRef}[$inputCounter], $inputCounter, "HeaderDoc comment", "32");
			            $line = ${$rawLineArrayRef}[++$inputCounter];
				    warnHDComment(${$rawLineArrayRef}[$inputCounter], $inputCounter, "HeaderDoc comment", "33");
				}
				$line =~ s/\{\s*\@docroot\s*\}/\\\@\\\@docroot/g;
				$headerDocComment .= $line ;
				# warnHDComment(${$rawLineArrayRef}[$inputCounter], $inputCounter, "HeaderDoc comment", "34");
				$line = ${$rawLineArrayRef}[++$inputCounter];

				# A HeaderDoc comment block immediately
				# after another one can be legal after some
				# tag types (e.g. @language, @header).
				# We'll postpone this check until the
				# actual parsing.
				# 
				if (!emptyHDok($headerDocComment)) {
					my $emptyDebug = 0;
					warn "curline is $line" if ($emptyDebug);
					warnHDComment(${$rawLineArrayRef}[$inputCounter], $inputCounter, "HeaderDoc comment", "35");
				}
			}
			# print "first line after $headerDocComment is $line\n";

			# test for @class, @protocol, or @category comment
			# here is where we create an array of class-specific lines
			# first, get the class name
			my $name = ""; my $type = "";
			if (($headerDocComment =~ /^\/\*!\s*\@class|\@interface|\@protocol|\@category\s*/i ||
			    ($headerDocComment =~ /^\/\*\!\s*\w+/ && (($name,$type)=classLookAhead($rawLineArrayRef, $inputCounter, $lang, $HeaderDoc::sublang)))) ||
			    ($lang eq "java" &&
				($headerDocComment =~ /^\/\*!\s*\@class|\@interface|\@protocol|\@category\s*/i ||
				($headerDocComment =~ /^\/\*\*\s*\w+/ && (($name,$type)=classLookAhead($rawLineArrayRef, $inputCounter, $lang, $HeaderDoc::sublang)))))) {
				print "INCLASS\n" if ($localDebug);
				if (length($type)) {
					$headerDocComment =~ s/\/\*(\*|\!)/\/\*$1 \@$type $name\n/s;
					print "CLARETURNED: \"$name\" \"$type\"\n" if ($localDebug || $classDebug || $blockDebug);
				}
				my $class = HeaderDoc::ClassArray->new(); # @@@
			   
				# insert line number (short form, since we're in a class)
				my $ln = $startline;
				my $linenumline = "/*! \@lineref $ln */\n";
				if ($lang eq "perl" || $lang eq "shell") {
					$linenumline = "# $linenumline";
				}
				$class->push($linenumline);
				# end insert line number

				($className = $headerDocComment) =~ s/.*\@class|\@protocol|\@category\s+(\w+)\s+.*/$1/s;
				$class->pushlines ($headerDocComment);

				# print "LINE IS $line\n";
				while (($line !~ /class\s|\@class|\@interface\s|\@protocol\s|typedef\s+struct\s/) && ($inputCounter <= $lastArrayIndex)) {
					$class->push ($line);  
					warnHDComment(${$rawLineArrayRef}[$inputCounter], $inputCounter, "class", "36");
					$line = ${$rawLineArrayRef}[++$inputCounter];
					warnHDComment(${$rawLineArrayRef}[$inputCounter], $inputCounter, "class", "37");
				}
				# $class->push ($line);  
				my $initial_bracecount = 0; # ($templine =~ tr/{//) - ($templine =~ tr/}//);
				print "[CLASSTYPE]line $line\n" if ($localDebug);

				SWITCH: {
					($line =~ /^\s*\@protocol\s+/ ) && 
						do { 
							$classType = "objCProtocol";  
							# print "FOUND OBJCPROTOCOL\n"; 
							$HeaderDoc::sublang="occ";
							$initial_bracecount++;
							last SWITCH; 
						};
					($line =~ /^\s*typedef\s+struct\s+/ ) && 
						do { 
							$classType = "C";  
							# print "FOUND C CLASS\n"; 
							last SWITCH; 
						};
					($line =~ /^\s*template\s+/ ) && 
						do { 
							$classType = "cppt";  
							# print "FOUND CPP TEMPLATE CLASS\n"; 
							$HeaderDoc::sublang="cpp";
							last SWITCH; 
						};
					($line =~ /^\s*(public|private|protected|)\s*class\s+/ ) && 
						do { 
							$classType = "cpp";  
							# print "FOUND CPP CLASS\n"; 
							$HeaderDoc::sublang="cpp";
							last SWITCH; 
						};
					($line =~ /^\s*(\@class|\@interface)\s+/ ) && 
						do { 
						        # it's either an ObjC class or category
						        if ($line =~ /\(.*\)/) {
								$classType = "objCCategory"; 
								# print "FOUND OBJC CATEGORY\n"; 
						        } else {
								$classType = "objC"; 
								 # print "FOUND OBJC CLASS\n"; 
						        }
							$HeaderDoc::sublang="occ";
							if ($1 ne "\@class") {
							    $initial_bracecount++;
							}
							last SWITCH; 
						};
					print "Unknown class type (known: cpp, cppt, objCCategory, objCProtocol, C,)\nline=\"$line\"";		
				}
				if ($lang eq "php") {$classType = "php";}
				elsif ($lang eq "java") {$classType = "java";}

				# now we're at the opening line of the class declaration
				# push it into the array
				# print "INCLASS! (line: $inputCounter $line)\n";
				my $inClassBraces = $initial_bracecount;
				my $leftBraces = 0;
				my $rightBraces = 0;

				# make sure we've seen at least one left brace
				# at the start of the class

				# $line = ${$rawLineArrayRef}[++$inputCounter];
				if (!($initial_bracecount) && $line !~ /;/) {
				    while (($inputCounter <= $lastArrayIndex)
					    && (!($leftBraces))) {
					print "[IP]line=$line\n" if ($localDebug);
					$class->push ($line);
					# print "line $line\n" if ($localDebug);
					# print "LINE IS[1] $line\n";
                                       	$leftBraces = $line =~ tr/{//;
                                       	$rightBraces = $line =~ tr/}//;
                                        $inClassBraces += $leftBraces;
                                        $inClassBraces -= $rightBraces;
					warnHDComment(${$rawLineArrayRef}[$inputCounter], $inputCounter, "class", "38");
					$line = ${$rawLineArrayRef}[++$inputCounter];
					# this is legal (headerdoc markup in
					# first line of class).
					# warnHDComment(${$rawLineArrayRef}[$inputCounter], $inputCounter, "class", "39");
				    }
				}
			# print "LINE IS[2] $line\n";
				# $inputCounter++;

				$class->push($line);
				$class->bracecount_inc($inClassBraces);
				my $bc = $class->bracecount();
				print "NEW: pushing class $class bc=$bc\n" if ($classDebug);
				print "CURLINE: $line\n" if ($localDebug);

				push(@classStack, $class);
			} else {

			    # HeaderDoc comment, but not a class

			    print "[2] line = $line\n" if ($localDebug);
			    my $class = pop(@classStack);
			    if ($class) {
				my $bc = $class->bracecount();
				print "popped class $class bc=$bc\n" if ($classDebug);
				$class->pushlines($headerDocComment);
				$class->push($line);
			   
				# now collect class lines until the closing
				# curly brace

				if (($classType =~ /cpp/) || ($classType =~ /^C/) || ($classType =~ /cppt/) ||
				    ($classType =~ /php/) || ($classType =~ /java/)) {
					my $leftBraces = $headerDocComment =~ tr/{// + $line =~ tr/{//;
					$class->bracecount_inc($leftBraces);
					my $rightBraces = $headerDocComment =~ tr/}// + $line =~ tr/}//;
					$class->bracecount_dec($rightBraces);
				}
				if (($classType =~ /objC/) || ($classType =~ /objCProtocol/) || ($classType =~ /objCCategory/)) {
					# @@@ VERIFY this next line used to be !~, but I think it should be =~
					if ($headerDocComment =~ /\@end/ || $line =~ /\@end/) {
						$class->bracecount_dec();
					}
				}
				if (my $bc=$class->bracecount()) {
					print "pushing class $class bc=$bc\n" if ($classDebug);
					push(@classStack, $class);
				} else {
					# push (@arrayOfLineArrays, \@classLines);
					my @array = $class->getarray();
					push (@arrayOfLineArrays, \@array);
					print "OUT OF CLASS[2]! (headerDocComment: $inputCounter $headerDocComment)\n" if ($localDebug);

					# insert line number
					my $tc = pop(@classStack);
					my $ln = $inputCounter + 1;
					my $linenumline = "/*! \@lineref $ln */\n";
					if ($lang eq "perl" || $lang eq "shell") {
						$linenumline = "# $linenumline";
					}
					if ($tc) {
						$tc->push($linenumline);
						push(@classStack, $tc);
					} else {
						push(@generalHeaderLines, $linenumline);
					}
					# end insert line number

# print "class declaration was:\n"; foreach my $line (@array) { print "$line\n"; }
					if ($localDebug) {
					    print "array is:\n";
					    foreach my $arrelem (@array) {
						print "$arrelem\n";
					    }
					    print "end of array.\n";
					}
				}
			    } else {
				# globalpushlines (\@generalHeaderLines, $headerDocComment);
				my @linearray = split (/\n/, $headerDocComment);
				foreach my $arrayline (@linearray) {
					push(@generalHeaderLines, "$arrayline\n");
				}
				push(@generalHeaderLines, $line);

				# push (@generalHeaderLines, $headerDocComment);
			    }
			}
		} else {
			print "[3]line=$line\n" if ($classDebug > 3);
			my $class = pop(@classStack);
			if ($class) {
				my $bc = $class->bracecount();
				print "[tail]popped class $class bc=$bc\n" if ($classDebug);

				$class->push($line);
				print "pushed line $line\n" if ($classDebug);
				   
				# now collect class lines until the closing
				# curly brace
	
				if (($classType =~ /cpp/) || ($classType =~ /^C/) || ($classType =~ /cppt/) ||
				    ($classType =~ /php/) || ($classType =~ /java/)) {
					my $leftBraces = $line =~ tr/{//;
					$class->bracecount_inc($leftBraces);
					my $rightBraces = $line =~ tr/}//;
					$class->bracecount_dec($rightBraces);
					print "lb=$leftBraces, rb=$rightBraces\n" if ($localDebug);
				} elsif (($classType =~ /objC/) || ($classType =~ /objCProtocol/) || ($classType =~ /objCCategory/)) {
					if ($line =~ /\@end/) {
						$class->bracecount_dec();
					}
					my $leftBraces = $line =~ tr/{//;
					$class->bracecount_inc($leftBraces);
					my $rightBraces = $line =~ tr/}//;
					$class->bracecount_dec($rightBraces);
					print "lb=$leftBraces, rb=$rightBraces\n" if ($localDebug || $classDebug);
					my $bc = $class->bracecount();
					print "bc=$bc\n" if ($localDebug || $classDebug);
				} else {
					print "WARNING: Unknown classtype $classType\n";
				}
				if (my $bc=$class->bracecount()) {
					print "pushing class $class bc=$bc\n" if ($classDebug);
					push(@classStack, $class);
				} else {
					# push (@arrayOfLineArrays, \@classLines);
					my @array = $class->getarray();
					push (@arrayOfLineArrays, \@array);
					print "OUT OF CLASS! (line: $inputCounter $line)\n" if ($localDebug);
					# insert line number
					my $tc = pop(@classStack);
					my $ln = $inputCounter + 1;
					my $linenumline = "/*! \@lineref $ln */\n";
					if ($lang eq "perl" || $lang eq "shell") {
						$linenumline = "# $linenumline";
					}
					if ($tc) {
						$tc->push($linenumline);
						push(@classStack, $tc);
					} else {
						push(@generalHeaderLines, $linenumline);
					}
					# end insert line number

# print "class declaration was:\n"; foreach my $line (@array) { print "$line\n"; }
					my $localDebug = 0;
					if ($localDebug) {
					    print "array is:\n";
					    foreach my $arrelem (@array) {
						print "$arrelem\n";
					    }
					    print "end of array.\n";
					}
				}
			} else {
				push (@generalHeaderLines, $line); print "PUSHED $line\n" if ($blockDebug);
			}
		}
		$inputCounter++;
	}
    push (@arrayOfLineArrays, \@generalHeaderLines);
    return @arrayOfLineArrays;
}

# /*! @function processCPPHeaderComment
#   */
sub processCPPHeaderComment {
# 	for now, we do nothing with this comment
    return;
}

# /*! @function removeSlashSlashComment
#   */
sub removeSlashSlashComment {
    my $line = shift;
    $line =~ s/\/\/.*$//;
    return $line;
}


sub get_super {
my $classType = shift;
my $dec = shift;
my $super = "";
my $localDebug = 0;

    print "GS: $dec EGS\n" if ($localDebug);

    $dec =~ s/\n/ /smg;

    if ($classType =~ /^occ/) {
	if ($dec !~ s/^\s*\@interface\s*//s) {
	    if ($dec !~ s/^\s*\@protocol\s*//s) {
	    	$dec !~ s/^\s*\@class\s*//s;
	    }
	}
	if ($dec =~ /(\w+)\s*\(\s*(\w+)\s*\)/) {
	    $super = $1; # delegate is $2
        } elsif (!($dec =~ s/.*?://s)) {
	    $super = "";
	} else {
	    $dec =~ s/\(.*//sg;
	    $dec =~ s/\{.*//sg;
	    $super = $dec;
	}
    } elsif ($classType =~ /^cpp$/) {
	$dec !~ s/^\s*\class\s*//s;
        if (!($dec =~ s/.*?://s)) {
	    $super = "";
	} else {
	    $dec =~ s/\(.*//sg;
	    $dec =~ s/\{.*//sg;
	    $dec =~ s/^\s*//sg;
	    $dec =~ s/^public//g;
	    $dec =~ s/^private//g;
	    $dec =~ s/^protected//g;
	    $dec =~ s/^virtual//g;
	    $super = $dec;
	}
    }

    $super =~ s/^\s*//;
    $super =~ s/\s.*//;

    print "$super is super\n" if ($localDebug);
    return $super;
}

# /*! @function determineClassType
#     @discussion determines the class type.
#   */
sub determineClassType {
	my $lineCounter   = shift;
	my $apiOwner      = shift;
	my $inputLinesRef = shift;
	my @inputLines    = @$inputLinesRef;
	my $classType = "unknown";
	my $tempLine = "";
	my $localDebug = 0;

 	do {
	# print "inc\n";
		$tempLine = $inputLines[$lineCounter];
		$lineCounter++;
	} while (($tempLine !~ /class|\@class|\@interface|\@protocol|typedef\s+struct/) && ($lineCounter <= $#inputLines));

	if ($tempLine =~ s/class\s//) {
	 	$classType = "cpp";  
	}
	if ($tempLine =~ s/typedef\s+struct\s//) {
	    # print "===>Cat: $tempLine\n";
	    $classType = "C"; # standard C "class", such as a
		                       # COM interface
	}
	if ($tempLine =~ s/(\@class|\@interface)\s//) { 
	    if ($tempLine =~ /\(.*\)/ && ($1 ne "\@class")) {
			# print "===>Cat: $tempLine\n";
			$classType = "occCat";  # a temporary distinction--not in apple_ref spec
									# methods in categories will be lumped in with rest of class, if existent
		} else {
			# print "===>Class: $tempLine\n";
			$classType = "occ"; 
		}
	}
	if ($tempLine =~ s/\@protocol\s//) {
	 	$classType = "intf";  
	}
	if ($lang eq "php") {
		$classType = "php";
	}
	if ($lang eq "java") {
		$classType = "java";
	}
print "determineClassType: returning $classType\n" if ($localDebug);
	if ($classType eq "unknown") {
		print "Bogus class ($tempLine)\n";
	}
	
	$HeaderDoc::sublang = $classType;
	return $classType;
}

# /*! @function processClassComments
#   */
sub processClassComment {
	my $apiOwner = shift;
	my $headerObj = $apiOwner;
	my $rootOutputDir = shift;
	my $fieldArrayRef = shift;
	my @fields = @$fieldArrayRef;
	my $classType = shift;
	my $filename = $HeaderDoc::headerObject->filename();
	my $linenum = shift;
	
	SWITCH: {
		($classType eq "php" ) && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); last SWITCH; };
		($classType eq "java" ) && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); last SWITCH; };
		($classType eq "cpp" ) && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); last SWITCH; };
		($classType eq "cppt" ) && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); last SWITCH; };
		($classType eq "occ") && do { $apiOwner = HeaderDoc::ObjCClass->new; $apiOwner->filename($filename); last SWITCH; };           
		($classType eq "occCat") && do { $apiOwner = HeaderDoc::ObjCCategory->new; $apiOwner->filename($filename); last SWITCH; };           
		($classType eq "intf") && do { $apiOwner = HeaderDoc::ObjCProtocol->new; $apiOwner->filename($filename); last SWITCH; };           
		($classType eq "C") && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); $apiOwner->CClass(1); last SWITCH; };
		print "Unknown type ($classType). known: classes (ObjC and C++), ObjC categories and protocols\n";		
	}
	# preserve class nesting
	$apiOwner->linenum($linenum);
	$apiOwner->apiOwner($HeaderDoc::currentClass);
	$HeaderDoc::currentClass = $apiOwner;

	if ($xml_output) {
	    $apiOwner->outputformat("hdxml");
	} else { 
	    $apiOwner->outputformat("html");
	}
	$apiOwner->headerObject($headerObj);
	$apiOwner->outputDir($rootOutputDir);
	foreach my $field (@fields) {
		SWITCH: {
			($field =~ /^\/\*\!/) && do {last SWITCH;}; # ignore opening /*!
			(($lang eq "java") && ($field =~ /^\s*\/\*\*/)) && do {last SWITCH;}; # ignore opening /**
			($field =~ s/^(class|interface|template)(\s+)/$2/) && 
				do {
					my ($name, $disc);
					my $filename = $HeaderDoc::headerObject->filename();
					# print "CLASSNAMEANDDISC:\n";
					($name, $disc) = &getAPINameAndDisc($field);
					my $classID = ref($apiOwner);
					$apiOwner->name($name);
					$apiOwner->filename($filename);
					if (length($disc)) {$apiOwner->discussion($disc);};
					$functionGroup = "";
					$HeaderDoc::globalGroup = "";
                	last SWITCH;
            	};
			($field =~ s/^see(also|)(\s+)/$2/) &&
				do {
					$apiOwner->see($field);
				};
			($field =~ s/^protocol(\s+)/$1/) && 
				do {
					my ($name, $disc);
					my $filename = $HeaderDoc::headerObject->filename();
					($name, $disc) = &getAPINameAndDisc($field); 
					$apiOwner->name($name);
					$apiOwner->filename($filename);
					if (length($disc)) {$apiOwner->discussion($disc);};
					$functionGroup = "";
					$HeaderDoc::globalGroup = "";
					last SWITCH;
				};
			($field =~ s/^category(\s+)/$1/) && 
				do {
					my ($name, $disc);
					my $filename = $HeaderDoc::headerObject->filename();
					($name, $disc) = &getAPINameAndDisc($field); 
					$apiOwner->name($name);
					$apiOwner->filename($filename);
					if (length($disc)) {$apiOwner->discussion($disc);};
					$functionGroup = "";
					$HeaderDoc::globalGroup = "";
					last SWITCH;
				};
            			($field =~ s/^templatefield(\s+)/$1/) && do {     
                                	$field =~ s/^\s+|\s+$//g;
                    			$field =~ /(\w*)\s*(.*)/s;
                    			my $fName = $1;
                    			my $fDesc = $2;
                    			my $fObj = HeaderDoc::MinorAPIElement->new();
					$fObj->linenum($linenum);
					$fObj->apiOwner($apiOwner);
                    			$fObj->outputformat($apiOwner->outputformat);
                    			$fObj->name($fName);
                    			$fObj->discussion($fDesc);
                    			$apiOwner->addToFields($fObj);
# print "inserted field $fName : $fDesc";
                                	last SWITCH;
                        	};
			($field =~ s/^super(class|)(\s+)/$2/) && do { $apiOwner->attribute("Superclass", $field, 0); $apiOwner->explicitSuper(1); last SWITCH; };
			($field =~ s/^throws(\s+)/$1/) && do {$apiOwner->throws($field); last SWITCH;};
			($field =~ s/^exception(\s+)/$1/) && do {$apiOwner->throws($field); last SWITCH;};
			($field =~ s/^abstract(\s+)/$1/) && do {$apiOwner->abstract($field); last SWITCH;};
			($field =~ s/^discussion(\s+)/$1/) && do {$apiOwner->discussion($field); last SWITCH;};
			($field =~ s/^availability(\s+)/$1/) && do {$apiOwner->availability($field); last SWITCH;};
			($field =~ s/^since(\s+)/$1/) && do {$apiOwner->availability($field); last SWITCH;};
            		($field =~ s/^author(\s+)/$1/) && do {$apiOwner->attribute("Author", $field, 0); last SWITCH;};
			($field =~ s/^version(\s+)/$1/) && do {$apiOwner->attribute("Version", $field, 0); last SWITCH;};
            		($field =~ s/^deprecated(\s+)/$1/) && do {$apiOwner->attribute("Deprecated", $field, 0); last SWITCH;};
            		($field =~ s/^version(\s+)/$1/) && do {$apiOwner->attribute("Version", $field, 0); last SWITCH;};
			($field =~ s/^updated(\s+)/$1/) && do {$apiOwner->updated($field); last SWITCH;};
	    ($field =~ s/^attribute(\s+)/$1/) && do {
		    my ($attname, $attdisc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$apiOwner->attribute($attname, $attdisc, 0);
		    } else {
			warn "$filename:$linenum:Missing name/discussion for attribute\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributelist(\s+)/$1/) && do {
		    $field =~ s/^\s*//s;
		    $field =~ s/\s*$//s;
		    my ($name, $lines) = split(/\n/, $field, 2);
		    $name =~ s/^\s*//s;
		    $name =~ s/\s*$//s;
		    $lines =~ s/^\s*//s;
		    $lines =~ s/\s*$//s;
		    if (length($name) && length($lines)) {
			my @attlines = split(/\n/, $lines);
			foreach my $line (@attlines) {
			    $apiOwner->attributelist($name, $line);
			}
		    } else {
			warn "$filename:$linenum:Missing name/discussion for attributelist\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributeblock(\s+)/$1/) && do {
		    my ($attname, $attdisc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$apiOwner->attribute($attname, $attdisc, 1);
		    } else {
			warn "$filename:$linenum:Missing name/discussion for attributeblock\n";
		    }
		    last SWITCH;
		};
			($field =~ s/^namespace(\s+)/$1/) && do {$apiOwner->namespace($field); last SWITCH;};
			($field =~ s/^instancesize(\s+)/$1/) && do {$apiOwner->attribute("Instance Size", $field, 0); last SWITCH;};
			($field =~ s/^performance(\s+)/$1/) && do {$apiOwner->attribute("Performance", $field, 1); last SWITCH;};
			# ($field =~ s/^subclass(\s+)/$1/) && do {$apiOwner->attributelist("Subclasses", $field); last SWITCH;};
			($field =~ s/^nestedclass(\s+)/$1/) && do {$apiOwner->attributelist("Nested Classes", $field); last SWITCH;};
			($field =~ s/^coclass(\s+)/$1/) && do {$apiOwner->attributelist("Co-Classes", $field); last SWITCH;};
			($field =~ s/^helper(class|)(\s+)/$2/) && do {$apiOwner->attributelist("Helper Classes", $field); last SWITCH;};
			($field =~ s/^helps(\s+)/$1/) && do {$apiOwner->attribute("Helps", $field, 0); last SWITCH;};
			($field =~ s/^classdesign(\s+)/$1/) && do {$apiOwner->attribute("Class Design", $field, 1); last SWITCH;};
			($field =~ s/^dependency(\s+)/$1/) && do {$apiOwner->attributelist("Dependencies", $field); last SWITCH;};
			($field =~ s/^ownership(\s+)/$1/) && do {$apiOwner->attribute("Ownership Model", $field, 1); last SWITCH;};
			($field =~ s/^security(\s+)/$1/) && do {$apiOwner->attribute("Security", $field, 1); last SWITCH;};
			($field =~ s/^whysubclass(\s+)/$1/) && do {$apiOwner->attribute("Reason to Subclass", $field, 1); last SWITCH;};
			print "Unknown field in class comment: $field\n";
		}
	}
	return $apiOwner;
}

# /*! @function processHeaderComment
#   */ 
sub processHeaderComment {
    my $apiOwner = shift;
    my $rootOutputDir = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $linenum = $apiOwner->linenum();
    my $filename = $apiOwner->filename();
    my $localDebug = 0;

	foreach my $field (@fields) {
	    # print "header field: |$field|\n";
		SWITCH: {
			($field =~ /^\/\*\!/)&& do {last SWITCH;}; # ignore opening /*!
			(($lang eq "java") && ($field =~ /^\s*\/\*\*/)) && do {last SWITCH;}; # ignore opening /**
			($field =~ s/^see(also)\s+//) &&
				do {
					$apiOwner->see($field);
				};
			(($field =~ /^header\s+/) ||
			 ($field =~ /^framework\s+/)) && 
			    do {
			 	if ($field =~ s/^framework\s+//) {
					$apiOwner->isFramework(1);
				} else {
					$field =~ s/^header\s+//;
				}
				
				my ($name, $disc);
				($name, $disc) = &getAPINameAndDisc($field); 
				my $longname = $name; #." (".$apiOwner->name().")";
				if (length($name)) {
					print "Setting header name to $longname\n" if ($debugging);
					$apiOwner->name($longname);
				}
				print "Discussion is:\n" if ($debugging);
				print "$disc\n" if ($debugging);
				if (length($disc)) {$apiOwner->discussion($disc);};
				last SWITCH;
			};
            ($field =~ s/^availability\s+//) && do {$apiOwner->availability($field); last SWITCH;};
	    ($field =~ s/^since\s+//) && do {$apiOwner->availability($field); last SWITCH;};
            ($field =~ s/^author\s+//) && do {$apiOwner->attribute("Author", $field, 0); last SWITCH;};
	    ($field =~ s/^version\s+//) && do {$apiOwner->attribute("Version", $field, 0); last SWITCH;};
            ($field =~ s/^deprecated\s+//) && do {$apiOwner->attribute("Deprecated", $field, 0); last SWITCH;};
            ($field =~ s/^version\s+//) && do {$apiOwner->attribute("Version", $field, 0); last SWITCH;};
	    ($field =~ s/^attribute\s+//) && do {
		    my ($attname, $attdisc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$apiOwner->attribute($attname, $attdisc, 0);
		    } else {
			warn "$filename:$linenum:Missing name/discussion for attribute\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributelist\s+//) && do {
		    $field =~ s/^\s*//s;
		    $field =~ s/\s*$//s;
		    my ($name, $lines) = split(/\n/, $field, 2);
		    $name =~ s/^\s*//s;
		    $name =~ s/\s*$//s;
		    $lines =~ s/^\s*//s;
		    $lines =~ s/\s*$//s;
		    if (length($name) && length($lines)) {
			my @attlines = split(/\n/, $lines);
			foreach my $line (@attlines) {
			    $apiOwner->attributelist($name, $line);
			}
		    } else {
			warn "$filename:$linenum:Missing name/discussion for attributelist\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributeblock\s+//) && do {
		    my ($attname, $attdisc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$apiOwner->attribute($attname, $attdisc, 1);
		    } else {
			warn "$filename:$linenum:Missing name/discussion for attributeblock\n";
		    }
		    last SWITCH;
		};
            ($field =~ s/^updated\s+//) && do {$apiOwner->updated($field); last SWITCH;};
            ($field =~ s/^abstract\s+//) && do {$apiOwner->abstract($field); last SWITCH;};
            ($field =~ s/^discussion\s+//) && do {$apiOwner->discussion($field); last SWITCH;};
            ($field =~ s/^copyright\s+//) && do { $apiOwner->headerCopyrightOwner($field); last SWITCH;};
            ($field =~ s/^meta\s+//) && do {$apiOwner->HTMLmeta($field); last SWITCH;};
	    ($field =~ s/^language\s+//) && do {
		SWITCH {
		    ($field =~ /^\s*c\+\+\s*$/i) && do { $HeaderDoc::sublang = "cpp"; last SWITCH; };
		    ($field =~ /^\s*objc\s*$/i) && do { $HeaderDoc::sublang = "occ"; last SWITCH; };
		    ($field =~ /^\s*pascal\s*$/i) && do { $HeaderDoc::sublang = "pascal"; last SWITCH; };
		    ($field =~ /^\s*perl\s*$/i) && do { $HeaderDoc::sublang = "perl"; last SWITCH; };
		    ($field =~ /^\s*shell\s*$/i) && do { $HeaderDoc::sublang = "shell"; last SWITCH; };
		    ($field =~ /^\s*php\s*$/i) && do { $HeaderDoc::sublang = "php"; last SWITCH; };
		    ($field =~ /^\s*javascript\s*$/i) && do { $HeaderDoc::sublang = "javascript"; last SWITCH; };
		    ($field =~ /^\s*java\s*$/i) && do { $HeaderDoc::sublang = "java"; last SWITCH; };
		    ($field =~ /^\s*c\s*$/i) && do { $HeaderDoc::sublang = "C"; last SWITCH; };
			{
				warn("$filename:$linenum:Unknown language $field in header comment\n");
			};
		};
	    };
            ($field =~ s/^CFBundleIdentifier\s+//i) && do {$apiOwner->attribute("CFBundleIdentifier", $field, 0); last SWITCH;};
            ($field =~ s/^related\s+//i) && do {$apiOwner->attributelist("Related Headers", $field); last SWITCH;};
            ($field =~ s/^(compiler|)flag\s+//) && do {$apiOwner->attributelist("Compiler Flags", $field); last SWITCH;};
            ($field =~ s/^preprocinfo\s+//) && do {$apiOwner->attribute("Preprocessor Behavior", $field, 1); last SWITCH;};
	    ($field =~ s/^whyinclude\s+//) && do {$apiOwner->attribute("Reason to Include", $field, 1); last SWITCH;};
            ($field =~ s/^ignore\s+//) && do { $field =~ s/\n//smg; push(@perHeaderIgnorePrefixes, $field); if (!($reprocess_input)) {$reprocess_input = 1;} print "ignoring $field" if ($localDebug); last SWITCH;};
            warn("$filename:$linenum:Unknown field in header comment: $field\n");
		}
	}


	return $apiOwner;
}

sub strip
{
    my $filename = shift;
    my $output_path = shift;
    my $inputRef = shift;
    my @inputLines = @$inputRef;
    my $output_file = "$output_path$pathSeparator$filename";

    if (-e $output_file) {
	# don't risk writing over original header
	$output_file .= "-stripped";
	print "WARNING: output file exists.  Saving as\n\n";
	print "        $output_file\n\n";
	print "instead.\n";
    }

    open(OUTFILE, ">$output_file") || die "Can't write $output_file.\n";
    if ($^O =~ /MacOS/i) {MacPerl::SetFileInfo('R*ch', 'TEXT', "$output_file");};

    my $inComment = 0;
    my $text = "";
    my $localDebug = 0;
    foreach my $line (@inputLines) {
	print "line $line\n" if ($localDebug);
	print "inComment $inComment\n" if ($localDebug);
        if (($line =~ /^\/\*\!/) || (($lang eq "java") && ($line =~ /^\s*\/\*\*/))) {  # entering headerDoc comment
		# on entering a comment, set state to 1 (in comment)
		$inComment = 1;
	}
	if ($inComment && ($line =~ /\*\//)) {
		# on leaving a comment, set state to 2 (leaving comment)
		$inComment = 2;
	}

	if (!$inComment) { $text .= $line; }

	if ($inComment == 2) {
		# state change back to 0 (we just skipped the last line of the comment)
		$inComment = 0;
	}
    }

# print "text is $text\n";
    print OUTFILE $text;

    close OUTFILE;
}

sub classLookAhead
{
    my $lineref = shift;
    my $inputCounter = shift;
    my $lang = shift;
    my $sublang = shift;
    my @linearray = @{$lineref};
    my $inComment = 0;
    my $inILC = 0;
    my $inAt = 0;
    my $localDebug = 0;

    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
        $typedefname, $varname, $constname, $structisbrace, $macronameref)
		= parseTokens($lang, $sublang);

    my $nametoken = "";
    my $typetoken = "";

    while ($inputCounter < scalar(@linearray)) {
	my $line = $linearray[$inputCounter];
	my @parts = split(/((\/\*|\/\/|\*\/|\W))/, $line);

	print "CLALINE: $line\n" if ($localDebug);

	foreach my $token (@parts) {
		if (!length($token)) {
			next;
			print "CLA:notoken\n" if ($localDebug);
		} elsif ($token eq "$soc") {
			$inComment = 1;
			print "CLA:soc\n" if ($localDebug);
		} elsif ($token eq "$ilc") {
			$inILC = 1;
			print "CLA:ilc\n" if ($localDebug);
		} elsif ($token eq "$eoc") {
			$inComment = 0;
			print "CLA:eoc\n" if ($localDebug);
		} elsif ($token =~ /\s+/) {
			print "CLA:whitespace\n" if ($localDebug);
		} else {
			print "CLA:text\n" if ($localDebug);
			if ($token =~ /\;/) {
				next;
				print "CLA:semi\n" if ($localDebug);
			} elsif ($token =~ /\@/) {
				$inAt = 1;
				print "CLA:inAt\n" if ($localDebug);
			} elsif (!$inAt && $token =~ /class/) {
				print "CLA:cpp_or_java_class\n" if ($localDebug);
				$typetoken = "class";
			} elsif ($inAt && $token =~ /(class|interface|protocol)/) {
				print "CLA:occ_$1\n" if ($localDebug);
				$typetoken = $1;
			} else {
				# The first non-comment token isn't a class.
				if ($typetoken eq "") {
					print "CLA:NOTACLASS:\"$token\"\n" if ($localDebug);
					return ();
				} else {
					print "CLA:CLASSNAME:\"$token\"\n" if ($localDebug);
					return ($token, $typetoken);
				}
			}
		}
	}

	$inputCounter++; $inILC = 0;
    }

    # Yikes!  We ran off the end of the file!
    warn "ClassLookAhead ran off EOF\n";
    return ();
}

sub printVersionInfo {
    my $av = HeaderDoc::APIOwner->VERSION();
    my $hev = HeaderDoc::HeaderElement->VERSION();
    my $hv = HeaderDoc::Header->VERSION();
    my $cppv = HeaderDoc::CPPClass->VERSION();
    my $objcv = HeaderDoc::ObjCClass->VERSION();
    my $objcprotocolv = HeaderDoc::ObjCProtocol->VERSION();
    my $fv = HeaderDoc::Function->VERSION();
    my $mv = HeaderDoc::Method->VERSION();
    my $tv = HeaderDoc::Typedef->VERSION();
    my $sv = HeaderDoc::Struct->VERSION();
    my $cv = HeaderDoc::Constant->VERSION();
    my $vv = HeaderDoc::Var->VERSION();
    my $ev = HeaderDoc::Enum->VERSION();
    my $uv = HeaderDoc::Utilities->VERSION();
    my $me = HeaderDoc::MinorAPIElement->VERSION();
    
	print "----------------------------------------------------\n";
	print "\tHeaderDoc version $VERSION.\n";
	print "\tModules:\n";
	print "\t\tAPIOwner - $av\n";
	print "\t\tHeaderElement - $hev\n";
	print "\t\tHeader - $hv\n";
	print "\t\tCPPClass - $cppv\n";
	print "\t\tObjClass - $objcv\n";
	print "\t\tObjCProtocol - $objcprotocolv\n";
	print "\t\tFunction - $fv\n";
	print "\t\tMethod - $mv\n";
	print "\t\tTypedef - $tv\n";
	print "\t\tStruct - $sv\n";
	print "\t\tConstant - $cv\n";
	print "\t\tEnum - $ev\n";
	print "\t\tVar - $vv\n";
	print "\t\tMinorAPIElement - $me\n";
	print "\t\tUtilities - $uv\n";
	print "----------------------------------------------------\n";
}

################################################################################
# Version Notes
# 1.61 (02/24/2000) Fixed getLineArrays to respect paragraph breaks in comments that 
#                   have an asterisk before each line.
################################################################################

