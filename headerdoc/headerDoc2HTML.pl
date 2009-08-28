#!/usr/bin/perl
#
# Script name: headerDoc2HTML
# Synopsis: Scans a file for headerDoc comments and generates an HTML
#           file from the comments it finds.
#
# Last Updated: $Date: 2009/04/13 22:19:30 $
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
# $Revision: 1.71 $
#####################################################################

my $HeaderDoc_Version = "8.7";
my $VERSION = '$Revision: 1.71 $';

$HeaderDoc::defaultHeaderComment = "Use the links in the table of contents to the left to access the documentation.<br>\n";    


################ General Constants ###################################
my $isMacOS;
my $pathSeparator;
my $specifiedOutputDir;
# my $export;
my $debugging;
my $testingExport = 0;
my $printVersion;
my $quietLevel;
my $xml_output;
my $man_output;
my $function_list_output;
my $doxytag_output;
my $headerdoc_strip;
my $use_stdout;
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
my @doxyTagFiles;
# @HeaderDoc::ignorePrefixes = ();
# @HeaderDoc::perHeaderIgnorePrefixes = ();
# %HeaderDoc::perHeaderIncludes = ();
my $reprocess_input = 0;
$HeaderDoc::nodec = 0;
my $functionGroup = "";
# $HeaderDoc::outerNamesOnly = 0;
$HeaderDoc::globalGroup = "";
$HeaderDoc::hidetokens = 0;
$HeaderDoc::exitstatus = 0;
$HeaderDoc::skiplist = "";
$HeaderDoc::idl_language = "idl";
$HeaderDoc::c_compiler = "/usr/bin/gcc";

$HeaderDoc::newTOC = 0;

my @headerObjects;	# holds finished objects, ready for printing
					# we defer printing until all header objects are ready
					# so that we can merge ObjC category methods into the 
					# headerObject that holds the class, if it exists.
my @categoryObjects;	    # holds finished objects that represent ObjC categories
my %objCClassNameToObject = ();	# makes it easy to find the class object to add category methods to
my %headerIncluded = ();

%HeaderDoc::appleRefUsed = ();
%HeaderDoc::availability_defs = ();
%HeaderDoc::availability_has_args = ();
@HeaderDoc::exclude_patterns = ();

my @classObjects;
$HeaderDoc::fileDebug = 0;
$HeaderDoc::debugFile = "";
# $HeaderDoc::debugFile = "AAutoToolbar.h";
# $HeaderDoc::debugFile = "IOFWCommand.h";
# $HeaderDoc::debugFile = "IONetworkInterface.h";
					
# Turn on autoflushing of 'print' output.  This is useful
# when HeaderDoc is operating in support of a GUI front-end
# which needs to get each line of log output as it is printed. 
$| = 1;

# Check options in BEGIN block to avoid overhead of loading supporting 
# modules in error cases.
my $uninstalledModulesPath;
BEGIN {
    use FindBin qw ($Bin);
    use Cwd;
    use Getopt::Std;
    use File::Find;

    $HeaderDoc::skipNextPDefine = 0;
    %HeaderDoc::ignorePrefixes = ();
    %HeaderDoc::perHeaderIgnorePrefixes = ();
    $HeaderDoc::perHeaderObjectID = 0;
    # NOTE: The following line is just a declaration.  The default
    # values are added later.
    %HeaderDoc::perHeaderIgnoreFuncMacros = ();
    %HeaderDoc::perHeaderIncludes = ();
    %HeaderDoc::perHeaderRanges = ();
    $HeaderDoc::outerNamesOnly = 0;
    %HeaderDoc::namerefs = ();
    $HeaderDoc::uniquenumber = 0;
    $HeaderDoc::counter = 0;
    $HeaderDoc::specified_config_file = "";
    $HeaderDoc::use_iframes = 1;

    use lib '/Library/Perl/TechPubs'; # Apple configuration workaround
    use lib '/AppleInternal/Library/Perl'; # Apple configuration workaround

    my %options = ();
    $lookupTableDirName = "LookupTables";
    $functionFilename = "functions.tab";;
    $typesFilename = "types.tab";
    $enumsFilename = "enumConstants.tab";

    $scriptDir = cwd();
    $HeaderDoc::groupright = 0;
    $HeaderDoc::parse_javadoc = 0;
    $HeaderDoc::force_parameter_tagging = 0;
    $HeaderDoc::truncate_inline = 0;
    $HeaderDoc::dumb_as_dirt = 1;
    $HeaderDoc::add_link_requests = 1;
    $HeaderDoc::use_styles = 0;
    $HeaderDoc::ignore_apiuid_errors = 0;
    $HeaderDoc::maxDecLen = 60; # Wrap functions, etc. if declaration longer than this length

    if ($^O =~ /MacOS/io) {
		$pathSeparator = ":";
		$isMacOS = 1;
		#$Bin seems to return a colon after the path on certain versions of MacPerl
		#if it's there we take it out. If not, leave it be
		#WD-rpw 05/09/02
		($uninstalledModulesPath = $FindBin::Bin) =~ s/([^:]*):$/$1/o;
    } else {
		$pathSeparator = "/";
		$isMacOS = 0;
    }
	$uninstalledModulesPath = "$FindBin::Bin"."$pathSeparator"."Modules";
	
	foreach (qw(Mac::Files)) {
	    $MOD_AVAIL{$_} = eval "use $_; 1";
    }
    $HeaderDoc::twig_available = 0;
    foreach my $path (@INC) {
	# print STDERR "$path\n";
	my $name = $path.$pathSeparator."XML".$pathSeparator."Twig.pm";
	# print STDERR "NAME: $name\n";
	if (-f $name) {
		$HeaderDoc::twig_available = 1;
	}
    }
    $HeaderDoc::FreezeThaw_available = 0;
    foreach my $path (@INC) {
	# print STDERR "$path\n";
	my $name = $path.$pathSeparator."FreezeThaw.pm";
	# print STDERR "NAME: $name\n";
	if (-f $name) {
		$HeaderDoc::FreezeThaw_available = 1;
	}
    }
}

    use lib $uninstalledModulesPath;
    use HeaderDoc::Utilities qw(linesFromFile);
    use HeaderDoc::Utilities qw(processTopLevel);

    if ($HeaderDoc::twig_available) {
	# This doesn't work!  Need alternate solution.
	# use HeaderDoc::Regen;
    }

    &getopts("CDEFHM:NOPQST:Xabc:de:fghijlmno:pqrstuv", \%options);

    if ($options{q}) {
	$quietLevel = "1";
    } else {
	$quietLevel = "0";
    }
    if ($options{Q}) {
	$HeaderDoc::enableParanoidWarnings = 1;
    } else {
	$HeaderDoc::enableParanoidWarnings = 0;
    }
    if ($options{v}) {
    	# print STDERR "Getting version information for all modules.  Please wait...\n";
	$printVersion = 1;
    }

    if ($options{r}) {
# print STDERR "TWIG? $HeaderDoc::twig_available\n";
	if ($HeaderDoc::twig_available) {
		print STDERR "Regenerating headers.\n";
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

    if ($options{F}) {
	# Use old-style frames.
	$HeaderDoc::use_iframes = 0;
    }

    if ($options{S}) {
	$HeaderDoc::IncludeSuper = 1;
    } else {
	$HeaderDoc::IncludeSuper = 0;
    }
    if ($options{C} || $HeaderDoc::use_iframes) {
	$HeaderDoc::ClassAsComposite = 1;
    } else {
	$HeaderDoc::ClassAsComposite = 0;
    }
    if ($options{E}) {
	$HeaderDoc::process_everything = 1;
    } else {
	$HeaderDoc::process_everything = 0;
    }

    if ($options{a}) {
	# Align columns
	$HeaderDoc::align_columns = 1;
    } else {
	$HeaderDoc::align_columns = 0;
    }

    if ($options{b}) {
	# "basic" mode - turn off some smart processing
	$HeaderDoc::dumb_as_dirt = 1;
    } else {
	$HeaderDoc::dumb_as_dirt = 0;
    }

    if ($options{c}) {
	# Use alternate config file.
	$HeaderDoc::specified_config_file = $options{c};
    }

    if ($options{g}) {
	# Group right side content by group.
	$HeaderDoc::groupright = 1;
    }

    if ($options{j}) {
	# Allow JavaDoc syntax in C.
	$HeaderDoc::parse_javadoc = 1;
    }

    if ($options{p}) {
	$HeaderDoc::enable_cpp = 1;
    } else {
	$HeaderDoc::enable_cpp = 0;
    }

    # Ignore names specified in @header, @class, @category, and other
    # API owner tags
    if ($options{N}) {
	$HeaderDoc::ignore_apiowner_names = 2;
    } elsif ($options{n}) {
	$HeaderDoc::ignore_apiowner_names = 1;
    } else {
	$HeaderDoc::ignore_apiowner_names = 0;
    }

    if ($options{l}) {
	# "linkless" mode - don't add link requests
	$HeaderDoc::add_link_requests = 0;
    } else {
	$HeaderDoc::add_link_requests = 1;
    }

    if ($options{M}) {
	$HeaderDoc::man_section = $options{M};
    } else {
	$HeaderDoc::man_section = 1;
    }

    if ($options{m}) {
	# man page output mode - implies xml
	$man_output = 1;
	$xml_output = 1;
    } else {
	$man_output = 0;
    }

    if ($options{e}) {
	my $exclude_list_file = $options{e};

	print STDERR "EXCLUDE LIST FILE is \"$exclude_list_file\".  CWD is ".cwd()."\n" if (!$quietLevel);
	my @templines = &linesFromFile($exclude_list_file);
	@HeaderDoc::exclude_patterns = ();
	foreach my $line (@templines) {
		$line =~ s/\n//g;
		push(@HeaderDoc::exclude_patterns, $line);
	}
    } else {
	@HeaderDoc::exclude_patterns = ();
    }

    if ($options{s}) {
	$headerdoc_strip = 1;
    } else {
	$headerdoc_strip = 0;
    }

    if ($options{i}) {
	$HeaderDoc::truncate_inline = 0;
    } else {
	$HeaderDoc::truncate_inline = 1;
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
    if ($options{t}) {
	if (!$quietLevel) {
		print STDERR "Forcing strict parameter tagging.\n";
	}
	$HeaderDoc::force_parameter_tagging = 1;
    }
    if ($options{T}) {
	if (!$HeaderDoc::FreezeThaw_available) {
		warn "FreezeThaw Perl module not found in library path.  Please\n";
		warn "install FreezeThaw and try again.\n";
		exit -1;
	}
	$HeaderDoc::testmode = $options{T};
    }
    if ($options{O}) {
	$HeaderDoc::outerNamesOnly = 1;
    } else {
	$HeaderDoc::outerNamesOnly = 0;
    }
    if ($options{d}) {
            print STDERR "\tDebugging on...\n\n";
            $debugging = 1;
    }

    if ($options{o}) {
	if ($use_stdout) {
		die("-o and -P are mutually exclusive.");
	}
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
			print STDERR "\nDocumentation will be written to $specifiedOutputDir\n";
		}
    }
    $lookupTableDir = "$scriptDir$pathSeparator$lookupTableDirName";
    # if (($options{x}) || ($testingExport)) {
        # if ((-e "$lookupTableDir$pathSeparator$functionFilename") && (-e "$lookupTableDir$pathSeparator$typesFilename")) {
                # print STDERR "\nWill write database files to an Export directory within each top-level HTML directory.\n\n";
                # $export = 1;
        # } else {
                # print STDERR "\nLookup table files not available. Cannot export data.\n";
            # $export = 0;
                # $testingExport = 0;
        # }
    # }

    $use_stdout = 0;
    if (!$headerdoc_strip && !$man_output) {
      if ($options{X}) {
	print STDERR "XML output mode.\n" if ($quietLevel eq "0");
	$xml_output = 1;
      } elsif ($options{D}) {
	print STDERR "Doxygen tagfile output mode.\n" if ($quietLevel eq "0");
	$doxytag_output = 1;
	if ($use_stdout) {
		die("-o and -D are mutually exclusive\n");
	}
      } elsif ($options{f}) {
	print STDERR "FUNCTION LIST output mode.\n" if ($quietLevel eq "0");
	$function_list_output = 1;
	$use_stdout = 1;
      } else {
	print STDERR "HTML output mode.\n" if ($quietLevel eq "0");
	$xml_output = 0;
      }
    }
# print STDERR "output mode is $xml_output\n";
    # Pipe mode (single file only)
    if ($options{P} || $use_stdout) {
	$use_stdout = 1;
	if ($doxytag_output) {
		die("-P and -D are mutually exclusive\n");
	}
	if (!$xml_output && !$function_list_output) {
		printf STDERR "XML output (-X) implicitly enabled by -P flag.\n";
		$xml_output = 1;
	}
	if (!$HeaderDoc::ClassAsComposite) {
		printf STDERR "ClassAsComposite (-C) implicitly enabled by -P flag.\n";
		$HeaderDoc::ClassAsComposite = 1;
	}
    }


    if (!$HeaderDoc::testmode && !$printVersion) {
      if (($#ARGV == 0) && (-d $ARGV[0])) {
        my $inputDir = $ARGV[0];
        if ($inputDir =~ /$pathSeparator$/) {
			$inputDir =~ s|(.*)$pathSeparator$|$1|; # get rid of trailing slash, if any
		}		
		if ( -f $inputDir.$pathSeparator."skiplist") {
			my @skiplist = &linesFromFile($inputDir."/skiplist");
			foreach my $skipfile (@skiplist) {
				if ($skipfile !~ /^\s*\#/ && $skipfile =~ /\S/) {
					$skipfile =~ s/^\s+//sg;
					$skipfile =~ s/\s+$//sg;
					$HeaderDoc::skiplist .= $skipfile."\n";
					print STDERR "Will skip $skipfile\n" if ($debugging);
				}
			}
			# $HeaderDoc::skiplist =~ s/\s/\@/g;
		}
		if ($^O =~ /MacOS/io) {
			find(\&getHeaders, $inputDir);
		} else {
			&find({wanted => \&getHeaders, follow => 1, follow_skip => 2}, $inputDir);
		}
      } else {
        print STDERR "Will process one or more individual files.\n" if ($debugging);
        foreach my $singleFile (@ARGV) {
            if (-f $singleFile) {
		if ($singleFile =~ /\.(cpp|c|C|h|m|M|i|hdoc|php|php\d|class|pas|p|java|j|jav|jsp|js|jscript|html|shtml|dhtml|htm|shtm|dhtm|pl|pm|bsh|csh|ksh|sh|defs|idl)$/o) {
                    push(@inputFiles, $singleFile);
		} else {
		    warn "File $singleFile is not of a known header or source code file type\n";
		}
            } else {
		    warn "HeaderDoc: file/directory not found: $singleFile\n";
	    }
        }
      }
      if ($debugging) {
	foreach my $if (@inputFiles) {
		print STDERR "FILE: $if\n";
	}
      }
      unless (@inputFiles) {
        print STDERR "No valid input files specified. \n\n";
        if ($isMacOS) {
            die "\tTo use HeaderDoc, drop a header file or folder of header files on this application.\n\n";
            } else {
                    die "\tUsage: headerdoc2html [-dq] [-o <output directory>] <input file(s) or directory>.\n\n";
            }
      }
    }
    

# /*! @function getDoxyTagFiles
#   */
    sub getDoxyTagFiles {
        my $filePath = $File::Find::name;
        my $fileName = $_;

        
	if ($fileName =~ /\.doxytagtemp$/o) {
		push(@doxyTagFiles, $filePath);
	}
    }

# /*! @function getHeaders
#   */
    sub getHeaders {
        my $filePath = $File::Find::name;
        my $fileName = $_;

        
	if ($fileName =~ /\.(cpp|c|C|h|m|M|i|hdoc|php|php\d|class|pas|p|java|j|jav|jsp|js|jscript|html|shtml|dhtml|htm|shtm|dhtm|pl|pm|bsh|csh|ksh|sh|defs|idl)$/o) {
	    # Skip lists use exact filename matches and must be in the
	    # the base directory being processed.  Exclude list is
	    # preferred, uses regular expressions, and can live in
	    # any file.  The filename of the exclude list is specified
	    # with the -e command-line flag.
	    if ($HeaderDoc::skiplist =~ /^\s*\Q$fileName\E\s*$/m) {
		print STDERR "skipped $filePath\n";
	    } elsif (in_exclude_list($filePath)) {
		print STDERR "skipped $filePath (found in exclude list)\n";
	    } else {
        	push(@inputFiles, $filePath);
		# print STDERR "will process $filePath ($fileName)\n";
		# print STDERR "SKIPLIST: ".$HeaderDoc::skiplist."\n";
	    }
        }
    }

    sub in_exclude_list($)
    {
	my $filepath = shift;
	foreach my $pattern (@HeaderDoc::exclude_patterns) {
		if ($pattern =~ /\S/) {
			# print STDERR "Checking $filepath against pattern \"$pattern\".\n";
			if ($filepath =~ $pattern) {
				return 1;
			}
		}
	}
	return 0;
    }


$HeaderDoc::curParserState = undef;

use strict;
use File::Copy;
use File::Basename;
use lib $uninstalledModulesPath;

# use Devel::Peek;

# Classes and other modules specific to HeaderDoc
# use HeaderDoc::DBLookup;
use HeaderDoc::Utilities qw(linesFromFile emptyHDok addAvailabilityMacro);
use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc
                            printArray linesFromFile printHash
                            updateHashFromConfigFiles getHashFromConfigFile
                            quote parseTokens
                            stringToFields warnHDComment get_super validTag
                            filterHeaderDocComment processHeaderComment
                            getLineArrays resolveLink objectForUID getAbsPath
                            allow_everything getAvailabilityMacros);
use HeaderDoc::BlockParse qw(blockParseOutside getAndClearCPPHash);
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
use HeaderDoc::Group;
use HeaderDoc::MinorAPIElement;
use HeaderDoc::ParseTree;
use HeaderDoc::ParserState;
use HeaderDoc::IncludeHash;
use HeaderDoc::Dependency;
use HeaderDoc::LineRange;
use HeaderDoc::AvailHelper;
use HeaderDoc::Test;

$HeaderDoc::modulesPath = $INC{'HeaderDoc/ParseTree.pm'};
$HeaderDoc::modulesPath =~ s/ParseTree.pm$//so;
# print STDERR "Module path is ".$HeaderDoc::modulesPath."\n";
# foreach my $key (%INC) {
	# print STDERR "KEY: $key\nVALUE: ".$INC{$key}."\n";
# }

if ($printVersion) {
	################ Version Info ##############################
	&printVersionInfo();
	exit $HeaderDoc::exitstatus;
}

if ($HeaderDoc::testmode) {
	print STDERR "Running tests.\n";
	if ($HeaderDoc::testmode eq "create") {
		newtest();
	} else {
		runtests($HeaderDoc::testmode, \@ARGV);
	}
	exit $HeaderDoc::exitstatus;
}


################ Setup from Configuration File #######################
my $localConfigFileName = "headerDoc2HTML.config";
my $preferencesConfigFileName = "com.apple.headerDoc2HTML.config";
my $homeDir;
my $usersPreferencesPath;
my $systemPreferencesPath;
my $usersAppSupportPath;
my $systemAppSupportPath;
#added WD-rpw 07/30/01 to support running on MacPerl
#modified WD-rpw 07/01/02 to support the MacPerl 5.8.0
if ($^O =~ /MacOS/io) {
	eval 
	{
		require "FindFolder.pl";
		$homeDir = MacPerl::FindFolder("D");	#D = Desktop. Arbitrary place to put things
		$usersPreferencesPath = MacPerl::FindFolder("P");	#P = Preferences
		$usersAppSupportPath = MacPerl::FindFolder("P");	#P = Preferences
	};
	if ($@) {
		import Mac::Files;
		$homeDir = Mac::Files::FindFolder(kOnSystemDisk(), kDesktopFolderType());
		$usersPreferencesPath = Mac::Files::FindFolder(kOnSystemDisk(), kPreferencesFolderType());
		$usersAppSupportPath = Mac::Files::FindFolder(kOnSystemDisk(), kPreferencesFolderType());
	}
	$systemPreferencesPath = $usersPreferencesPath;
	$systemAppSupportPath = $usersAppSupportPath;
} else {
	$homeDir = (getpwuid($<))[7];
	$usersPreferencesPath = $homeDir.$pathSeparator."Library".$pathSeparator."Preferences";
	$usersAppSupportPath = $homeDir.$pathSeparator."Library".$pathSeparator."Application Support".$pathSeparator."Apple".$pathSeparator."HeaderDoc";
	$systemPreferencesPath = "/Library/Preferences";
	$systemAppSupportPath = "/Library/Application Support/Apple/HeaderDoc";
}

# The order of files in this array determines the order that the config files will be read
# If there are multiple config files that declare a value for the same key, the last one read wins
my $CWD = cwd();
my @configFiles = ($systemPreferencesPath.$pathSeparator.$preferencesConfigFileName, $usersPreferencesPath.$pathSeparator.$preferencesConfigFileName, $Bin.$pathSeparator.$localConfigFileName, $CWD.$pathSeparator.$localConfigFileName);

if (length($HeaderDoc::specified_config_file)) {
	@configFiles = ();
	push(@configFiles, $HeaderDoc::specified_config_file);
}

# default configuration, which will be modified by assignments found in config files.
my %config = (
    ignorePrefixes => "",
    externalStyleSheets => "",
    externalTOCStyleSheets => "",
    tocStyleImports => "",
    styleSheetExtrasFile => "",
    styleImports => "",
    # appleTOC intentionally not defined.
    classAsComposite => $HeaderDoc::ClassAsComposite,
    copyrightOwner => "",
    defaultFrameName => "index.html",
    compositePageName => "CompositePage.html",
    masterTOCName => "MasterTOC.html",
    apiUIDPrefix => "apple_ref",
    htmlHeader => "",
    htmlFooter => "",
    htmlHeaderFile => "",
    htmlFooterFile => "",
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
    varStyle => "",
    templateStyle => "",
    introductionName => "Introduction",
    superclassName => "Superclass",
    idlLanguage => "idl",
    cCompiler => "/usr/bin/gcc"
);

%config = &updateHashFromConfigFiles(\%config,\@configFiles);

getAvailabilityMacros($HeaderDoc::modulesPath."Availability.list", $quietLevel);

if ($config{"ignorePrefixes"}) {
    my $localDebug = 0;
    my @prefixlist = split(/\|/, $config{"ignorePrefixes"});
    foreach my $prefix (@prefixlist) {
	print STDERR "ignoring $prefix\n" if ($localDebug);
	# push(@HeaderDoc::ignorePrefixes, $prefix);
	$prefix =~ s/^\s*//so;
	$prefix =~ s/\s*$//so;
	$HeaderDoc::ignorePrefixes{$prefix} = $prefix;
    }
}

if ($config{"externalStyleSheets"}) {
    $HeaderDoc::externalStyleSheets = $config{"externalStyleSheets"};
    $HeaderDoc::externalStyleSheets =~ s/[\n\r]/ /sgo;
    $HeaderDoc::use_styles = 1;
}
if ($config{"externalTOCStyleSheets"}) {
    $HeaderDoc::externalTOCStyleSheets = $config{"externalTOCStyleSheets"};
    $HeaderDoc::externalTOCStyleSheets =~ s/[\n\r]/ /sgo;
    $HeaderDoc::use_styles = 1;
}
if ($config{"tocStyleImports"}) {
    $HeaderDoc::tocStyleImports = $config{"tocStyleImports"};
    $HeaderDoc::tocStyleImports =~ s/[\n\r]/ /sgo;
    $HeaderDoc::use_styles = 1;
}
if ($config{"idlLanguage"}) {
    $HeaderDoc::idl_language = $config{"idlLanguage"};
}

if ($config{"cCompiler"}) {
    $HeaderDoc::c_compiler = $config{"cCompiler"};
}

if ($config{"styleSheetExtrasFile"} ne "") {
    my $found = 0;
    my $basename = $config{"styleSheetExtrasFile"};
    my $oldRS = $/;
    $/ = undef;
    # my @extrasFiles = ($Bin.$pathSeparator.$basename, $usersPreferencesPath.$pathSeparator.$basename, $basename);
    my @extrasFiles = ($systemPreferencesPath.$pathSeparator.$basename, $usersPreferencesPath.$pathSeparator.$basename, $Bin.$pathSeparator.$basename, $CWD.$pathSeparator.$basename, $usersAppSupportPath.$pathSeparator.$basename, $systemAppSupportPath.$pathSeparator.$basename);
    foreach my $filename (@extrasFiles) {
	if (open(READFILE, "<$filename")) {
		$HeaderDoc::styleSheetExtras = <READFILE>;
		close(READFILE);
		$found = 1;
	}
    }
    $/ = $oldRS;
    if (!$found) {
	die("Could not find file $basename in expected locations.\n");
    }

    $HeaderDoc::styleSheetExtras =~ s/[\n\r]/ /sgo;
    $HeaderDoc::use_styles = 1;
}

if ($config{"styleImports"}) {
    $HeaderDoc::styleImports = $config{"styleImports"};
    $HeaderDoc::styleImports =~ s/[\n\r]/ /sgo;
    $HeaderDoc::use_styles = 1;
}

if (defined $config{"appleTOC"}) {
	$HeaderDoc::newTOC = $config{"appleTOC"};
	$HeaderDoc::newTOC =~ s/\s*//;
} else {
	$HeaderDoc::newTOC = 0;
}

if ($config{"classAsComposite"}) {
	$HeaderDoc::ClassAsComposite = $config{"classAsComposite"};
	$HeaderDoc::ClassAsComposite =~ s/\s*//;
} else {
	$HeaderDoc::ClassAsComposite = 0;
}

if ($config{"copyrightOwner"}) {
    HeaderDoc::APIOwner->copyrightOwner($config{"copyrightOwner"});
}
if ($config{"defaultFrameName"}) {
    HeaderDoc::APIOwner->defaultFrameName($config{"defaultFrameName"});
}
if ($config{"compositePageName"}) {
    HeaderDoc::APIOwner->compositePageName($config{"compositePageName"});
}
if ($config{"apiUIDPrefix"}) {
    HeaderDoc::APIOwner->apiUIDPrefix($config{"apiUIDPrefix"});
}
if ($config{"htmlHeader"}) {
    HeaderDoc::APIOwner->htmlHeader($config{"htmlHeader"});
}
if ($config{"htmlFooter"}) {
    HeaderDoc::APIOwner->htmlFooter($config{"htmlFooter"});
}
my $oldRecSep = $/;
undef $/;

if ($config{"htmlHeaderFile"}) {
    my $basename = $config{"htmlHeaderFile"};
    my @htmlHeaderFiles = ($Bin.$pathSeparator.$basename, $usersPreferencesPath.$pathSeparator.$basename, $basename);
    foreach my $filename (@htmlHeaderFiles) {
	if (open(HTMLHEADERFILE, "<$filename")) {
	    my $headerString = <HTMLHEADERFILE>;
	    close(HTMLHEADERFILE);
	    # print STDERR "HEADER: $headerString";
	    HeaderDoc::APIOwner->htmlHeader($headerString);
	}
    }
}
if ($config{"htmlFooterFile"}) {
    my $basename = $config{"htmlFooterFile"};
    my @htmlFooterFiles = ($Bin.$pathSeparator.$basename, $usersPreferencesPath.$pathSeparator.$basename, $basename);
    foreach my $filename (@htmlFooterFiles) {
	if (open(HTMLFOOTERFILE, "<$filename")) {
	    my $headerString = <HTMLFOOTERFILE>;
	    close(HTMLFOOTERFILE);
	    # print STDERR "FOOTER: $headerString";
	    HeaderDoc::APIOwner->htmlFooter($headerString);
	}
    }
}
$/ = $oldRecSep;

if ($config{"dateFormat"}) {
    $HeaderDoc::datefmt = $config{"dateFormat"};
    if ($HeaderDoc::datefmt !~ /\S/) {
	$HeaderDoc::datefmt = "%B %d, %Y";
    }
} else {
    $HeaderDoc::datefmt = "%B %d, %Y";
}
HeaderDoc::APIOwner->fix_date();

if ($config{"textStyle"}) {
	HeaderDoc::APIOwner->setStyle("text", $config{"textStyle"});
}

if ($config{"commentStyle"}) {
	HeaderDoc::APIOwner->setStyle("comment", $config{"commentStyle"});
}

if ($config{"preprocessorStyle"}) {
	HeaderDoc::APIOwner->setStyle("preprocessor", $config{"preprocessorStyle"});
}

if ($config{"funcNameStyle"}) {
	HeaderDoc::APIOwner->setStyle("function", $config{"funcNameStyle"});
}

if ($config{"stringStyle"}) {
	HeaderDoc::APIOwner->setStyle("string", $config{"stringStyle"});
}

if ($config{"charStyle"}) {
	HeaderDoc::APIOwner->setStyle("char", $config{"charStyle"});
}

if ($config{"numberStyle"}) {
	HeaderDoc::APIOwner->setStyle("number", $config{"numberStyle"});
}

if ($config{"keywordStyle"}) {
	HeaderDoc::APIOwner->setStyle("keyword", $config{"keywordStyle"});
}

if ($config{"typeStyle"}) {
	HeaderDoc::APIOwner->setStyle("type", $config{"typeStyle"});
}

if ($config{"paramStyle"}) {
	HeaderDoc::APIOwner->setStyle("param", $config{"paramStyle"});
}

if ($config{"varStyle"}) {
	HeaderDoc::APIOwner->setStyle("var", $config{"varStyle"});
}

if ($config{"templateStyle"}) {
	HeaderDoc::APIOwner->setStyle("template", $config{"templateStyle"});
}

if ($config{"introductionName"}) {
	$HeaderDoc::introductionName=$config{"introductionName"};
}

if ($config{"superclassName"}) {
	$HeaderDoc::superclassName=$config{"superclassName"};
}


# ################ Exporting ##############################
# if ($export || $testingExport) {
	# HeaderDoc::DBLookup->loadUsingFolderAndFiles($lookupTableDir, $functionFilename, $typesFilename, $enumsFilename);
# }

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
my $inAvailabilityMacro = 0;
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
my $rootFileName;

my %HeaderFileProcessedThisRound = ();
%HeaderDoc::HeaderFileCPPArgHashHash = ();
%HeaderDoc::HeaderFileCPPHashHash = ();

my $includeDebug = 0;

if (!$quietLevel) {
    print STDERR "======= Parsing Input Files =======\n";
}

if ($use_stdout && (scalar(@inputFiles) > 1)) {
	die("-P flag limits you to a single input file.\n");
}

if ($debugging) { print STDERR "Processing includes.\n"; }
foreach my $inputFile (@inputFiles) {
    my $fullpath=getAbsPath($inputFile);
    my @rawInputLines = &linesFromFile($inputFile);
    if ($debugging) { print STDERR "Checking file $inputFile\n"; }
    # Grab any #include directives.
    processIncludes(\@rawInputLines, $fullpath);
}
if ($debugging) { print STDERR "Done processing includes.  Fixing dependencies.\n"; }

my @fileList = ();

if (1 || $HeaderDoc::enable_cpp) {
	my $deplistref = fix_dependency_order(\@inputFiles);
	if ($deplistref) {
		@fileList = @{$deplistref};
	} else {
		@fileList = @inputFiles;
	}
} else {
	@fileList = @inputFiles
}
if ($debugging) { print STDERR "Done fixing dependencies.  Filling fileList hash.\n"; }

if ($#fileList != $#inputFiles) {
	die("File counts don't match: ".$#fileList." != ".$#inputFiles.".\n");
}

# Remove duplicates.
my %filelisthash = ();
my @oldfileList = @fileList;
@fileList = ();
foreach my $inputFile (@oldfileList) {
	if (!$filelisthash{$inputFile}) {
		$filelisthash{$inputFile} = 1;
		push(@fileList, $inputFile);
	}
}
if ($debugging) { print STDERR "Done filling fileList hash\n"; }

@oldfileList = (); # free memory
%filelisthash = (); # free memory

my $sort_entries = $HeaderDoc::sort_entries;

foreach my $inputFile (@fileList) {
	my $headerObject;  # this is the Header object that will own the HeaderElement objects for this file.
	my $cppAccessControlState = "protected:"; # the default in C++
	my $objcAccessControlState = "private:"; # the default in Objective C
	$HeaderDoc::AccessControlState = "";

	my @perHeaderClassObjects = ();
	my @perHeaderCategoryObjects = ();
	$HeaderDoc::perHeaderObjectID = 0;

	# Restore this setting if it got changed on a per-header basis.
	$HeaderDoc::sort_entries = $sort_entries;

    my @path = split (/$pathSeparator/, $inputFile);
    my $filename = pop (@path);
    if ($HeaderDoc::HeaderFileCPPHashHash{$inputFile}) {
	print STDERR "Already procesed $inputFile.  Skipping.\n" if ($includeDebug);
	next;
    }

    if (basename($filename) eq $HeaderDoc::debugFile) {
	$HeaderDoc::fileDebug = 1;
	print STDERR "Enabling debug mode for this file.\n";
    }

    my $sublang = "";
    if ($quietLevel eq "0") {
	if ($headerdoc_strip) {
		print STDERR "\nStripping $inputFile\n";
	} elsif ($regenerate_headers) {
		print STDERR "\nRegenerating $inputFile\n";
	} else {
		print STDERR "\nProcessing $inputFile\n";
	}
    }
    # @@@ DAG WARNING: The next line doesn't do anything anymore. @@@
    # %HeaderDoc::perHeaderIgnoreFuncMacros = ( "OSDeclareDefaultStructors" => "OSDeclareDefaultStructors", "OSDeclareAbstractStructors" => "OSDeclareAbstractStructors" );
    ## if ($filename =~ /\.idl$/) {
	## # print STDERR "IDL FILE\n";
	## # %HeaderDoc::perHeaderIgnoreFuncMacros = ( "cpp_quote" => "cpp_quote" );
	## HeaderDoc::BlockParse::cpp_add_string("#define cpp_quote(a) a", 0);
	## Disabled because the syntax is cpp_quote(" content here ").  Note the quotes.
    ## }
    %HeaderDoc::perHeaderIgnorePrefixes = ();
    $HeaderDoc::globalGroup = "";
    $reprocess_input = 0;
    
    my $headerDir = join("$pathSeparator", @path);
    ($rootFileName = $filename) =~ s/\.(cpp|c|C|h|m|M|i|hdoc|php|php\d|class|pas|p|java|j|jav|jsp|js|jscript|html|shtml|dhtml|htm|shtm|dhtm|pl|pm|bsh|csh|ksh|sh|defs|idl)$/_$1/;
    if ($filename =~ /\.(php|php\d|class)$/) {
	$lang = "php";
	$sublang = "php";
    } elsif ($filename =~ /\.c$/) {
	# treat a C program similar to PHP, since it could contain k&r-style declarations
	$lang = "Csource";
	$sublang = "Csource";
    } elsif ($filename =~ /\.(C|cpp)$/) {
	# Don't allow K&R C declarations in C++ source code.
	# Set C++ flags from the very beginning.
	$lang = "C";
	$sublang = "cpp";
    } elsif ($filename =~ /\.(m|M)$/) {
	# Don't allow K&R C declarations in ObjC source code.
	# Set C++ flags from the very beginning.
	$lang = "C";
	$sublang = "occ";
    } elsif ($filename =~ /\.(s|d|)htm(l?)$/i) {
	$lang = "java";
	$sublang = "javascript";
    } elsif ($filename =~ /\.j(s|sp|script)$/i) {
	$lang = "java";
	$sublang = "javascript";
    } elsif ($filename =~ /\.j(ava|av|)$/i) {
	$lang = "java";
	$sublang = "java";
    } elsif ($filename =~ /\.p(as|)$/i) {
	$lang = "pascal";
	$sublang = "pascal";
    } elsif ($filename =~ /\.p(l|m)$/i) {
	$lang = "perl";
	$sublang = "perl";
    } elsif ($filename =~ /\.(c|b|k|)sh$/i) {
	$lang = "shell";
	if ($filename =~ /\.csh$/i) {
		$sublang = "csh";
	} else {
		$sublang = "shell";
	}
    } else {
	$lang = "C";
	$sublang = "C";
    }

    $HeaderDoc::lang = $lang;
    $HeaderDoc::sublang = $sublang;

    if ($filename =~ /\.idl$/o) { 
	$HeaderDoc::lang = "C";
	$HeaderDoc::sublang = "IDL";
    }

    if ($filename =~ /\.defs$/o) { 
	$HeaderDoc::lang = "C";
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
    $HeaderDoc::rootOutputDir = $rootOutputDir;

    # my @cookedInputLines;
    my $localDebug = 0;

    my @rawInputLines = &linesFromFile($inputFile);
    my $encoding = $HeaderDoc::lastFileEncoding;
    print STDERR "ENCODING GUESS: $encoding\n" if ($localDebug);

    # IS THIS STILL NEEDED?
    # foreach my $line (@rawInputLines) {
	# foreach my $prefix (keys %HeaderDoc::ignorePrefixes) {
	    # if ($line =~ s/^\s*$prefix\s*//g) {
		# print STDERR "ignored $prefix\n" if ($localDebug);
	    # }
	# }
	# push(@cookedInputLines, $line);
    # }
    # @rawInputLines = @cookedInputLines;

    @HeaderDoc::cppHashList = ();
    @HeaderDoc::cppArgHashList = ();
	
REDO:
print STDERR "REDO" if ($debugging);
    # check for HeaderDoc comments -- if none, move to next file
    my @headerDocCommentLines = grep(/^\s*\/\*\!/, @rawInputLines);
    if ((!@headerDocCommentLines) && ($lang eq "java" || $HeaderDoc::parse_javadoc)) {
	@headerDocCommentLines = grep(/^\s*\/\*\*[^\*]/, @rawInputLines);
    }
    if ((!@headerDocCommentLines) && ($lang eq "perl" || $lang eq "shell")) {
	@headerDocCommentLines = grep(/^\s*\#\s*\/\*\!/, @rawInputLines);
    }
    if ((!@headerDocCommentLines) && ($lang eq "pascal")) {
	@headerDocCommentLines = grep(/^\s*\{\!/, @rawInputLines);
    }
    if (!@headerDocCommentLines && ((!$HeaderDoc::process_everything) || (!allow_everything($lang, $sublang)))) {
	if ($quietLevel eq "0") {
            print STDERR "    Skipping. No HeaderDoc comments found.\n";
	}
        next;
    }

    my $fullpath=getAbsPath($inputFile);
    my $basefilename = basename($inputFile);

    if (!$headerdoc_strip) {
	# Don't do this if we're stripping.  It wastes memory and
	# creates unnecessary empty directories in the output path.

	$headerObject = HeaderDoc::Header->new();
	$headerObject->encoding($encoding);

	# SDump($headerObject, "point1");
	$headerObject->linenuminblock(0);
	$headerObject->blockoffset(0);
	# $headerObject->linenum(0);
	$headerObject->apiOwner($headerObject);
	$HeaderDoc::headerObject = $headerObject;
	# SDump($headerObject, "point2");

	# if ($quietLevel eq "0") {
	# print STDERR "output mode is $xml_output\n";
	# }
	if ($use_stdout) {
		$headerObject->use_stdout(1);
	} else {
		$headerObject->use_stdout(0);
	}
	if ($xml_output) {
		$headerObject->outputformat("hdxml");
	} elsif ($function_list_output) { 
		$headerObject->outputformat("functions");
	} else { 
		$headerObject->outputformat("html");
	}
	$headerObject->outputDir($rootOutputDir);
	$headerObject->name($filename);
	$headerObject->filename($filename);
	$headerObject->fullpath($fullpath);
    } else {
	$headerObject = HeaderDoc::Header->new();
	$HeaderDoc::headerObject = $headerObject;
	if ($use_stdout) {
		$headerObject->use_stdout(1);
	} else {
		$headerObject->use_stdout(0);
	}
	$headerObject->filename($filename);
	$headerObject->linenuminblock(0);
	$headerObject->blockoffset(0);
	# $headerObject->linenum(0);
    }
	# SDump($headerObject, "point3");
	
    # scan input lines for class declarations
    # return an array of array refs, the first array being the header-wide lines
    # the others (if any) being the class-specific lines
	my @lineArrays = &getLineArrays(\@rawInputLines, $lang, $sublang);

# print STDERR "NLA: " . scalar(@lineArrays) . "\n";
    
    my $processEverythingDebug = 0;
    my $localDebug = 0 || $debugging;
    my $linenumdebug = 0;

    if ($headerdoc_strip) {
	# print STDERR "input file is $filename, output dir is $rootOutputDir\n";
	my $outdir = "";
	if (length ($specifiedOutputDir)) {
        	$outdir ="$specifiedOutputDir";
	} elsif (@path) {
        	$outdir ="$headerDir";
	} else {
        	$outdir = "strip_output";
	}
	strip($filename, $outdir, $rootOutputDir, $inputFile, \@rawInputLines);
	print STDERR "done.\n" if ($quietLevel eq "0");
	next;
    }
    if ($regenerate_headers) {
	HeaderDoc::Regen->regenerate($inputFile, $rootOutputDir);
	print STDERR "done.\n" if ($quietLevel eq "0");
	next;
    }

	# SDump($headerObject, "point4");
    my $retainheader = 0;
    foreach my $arrayRef (@lineArrays) {
        my $blockOffset = 0;
        my @inputLines = @$arrayRef;
	$HeaderDoc::nodec = 0;


	    # look for /*! comments and collect all comment fields into the appropriate objects
        my $apiOwner = $headerObject;  # switches to a class/protocol/category object, when within a those declarations
	my ($case_sensitive, $keywordhashref) = $apiOwner->keywords();
	$HeaderDoc::currentClass = $apiOwner;
	    print STDERR "inHeader\n" if ($localDebug);
	    my $inputCounter = 0;
	    my $ctdebug = 0;
	    my $classType = "unknown";
	    print STDERR "CLASS TYPE CHANGED TO $classType\n" if ($ctdebug);
	    my $nlines = $#inputLines;
	    print STDERR "PROCESSING LINE ARRAY\n" if ($HeaderDoc::inputCounterDebug);
	    while ($inputCounter <= $nlines) {
			my $line = "";           
	        
			if ($inputLines[$inputCounter] =~ /^\s*#include[ \t]+(.*)$/) {
				my $rest = $1;
				$rest =~ s/^\s*//s;
				$rest =~ s/\s*$//s;
				if ($rest !~ s/^\<(.*)\>$/$1/s) {
					$rest =~ s/^\"(.*)\"$/$1/s;
				}
				my $filename = basename($rest);
				if ($HeaderDoc::HeaderFileCPPHashHash{$filename}) {
					my $includehash = HeaderDoc::IncludeHash->new();
					$includehash->{FILENAME} = $filename;
					$includehash->{LINENUM} = $inputCounter + $blockOffset;
					$includehash->{HASHREF} = $HeaderDoc::HeaderFileCPPHashHash{$filename};
					push(@HeaderDoc::cppHashList, $includehash);
# print STDERR "PUSH HASH\n";
					push(@HeaderDoc::cppArgHashList, $HeaderDoc::HeaderFileCPPArgHashHash{$filename});
				}
			}
	        	print STDERR "Input line number[1]: $inputCounter\n" if ($localDebug);
			print STDERR "last line ".$inputLines[$inputCounter-1]."\n" if ($localDebug);
			print STDERR "next line ".$inputLines[$inputCounter]."\n" if ($localDebug);
	        	if ($inputLines[$inputCounter] =~ /^\s*(public|private|protected)/o) {
				$cppAccessControlState = $&;
	        		if ($inputLines[$inputCounter] =~ /^\s*(public|private|protected)\s*:/o) {
					# trim leading whitespace and tabulation
					$cppAccessControlState =~ s/^\s+//o;
					# trim ending ':' and whitespace
					$cppAccessControlState =~ s/\s*:\s*$/$1/so;
					# set back the ':'
					$cppAccessControlState = "$cppAccessControlState:";
				}
			}
	        	if ($inputLines[$inputCounter] =~ /^\s*(\@public|\@private|\@protected)/o) {
				$objcAccessControlState = $&;
	        		if ($inputLines[$inputCounter] =~ /^\s*(\@public|\@private|\@protected)\s+/o) {
					# trim leading whitespace and tabulation
					$objcAccessControlState =~ s/^\s+//o;
					# trim ending ':' and whitespace
					$objcAccessControlState =~ s/\s*:\s*$/$1/so;
					# set back the ':'
					$objcAccessControlState = "$objcAccessControlState:";
				}
			}


			my @fields = ();
			$HeaderDoc::allow_multi = 0; # Treat any #if/#ifdef blocks we find as being blocks of similar functions.
	        	if (($lang ne "pascal" && (
			     ($lang ne "perl" && $lang ne "shell" && $inputLines[$inputCounter] =~ /^\s*\/\*\!/o) ||
			     (($lang eq "perl" || $lang eq "shell") && ($inputLines[$inputCounter] =~ /^\s*\#\s*\/\*\!/o)) ||
			     (($lang eq "java" || $HeaderDoc::parse_javadoc) && ($inputLines[$inputCounter] =~ /^\s*\/\*\*[^*]/o)))) ||
			    (($lang eq "pascal") && ($inputLines[$inputCounter] =~ s/^\s*\{!/\/\*!/so))) {  # entering headerDoc comment
				my $newlinecount = 0;
				# slurp up comment as line

				if (($lang ne "pascal" && ($inputLines[$inputCounter] =~ /\s*\*\//o)) ||
				    ($lang eq "pascal" && ($inputLines[$inputCounter] =~ s/\s*\}/\*\//so))) { # closing comment marker on same line

					my $linecopy = $inputLines[$inputCounter];
					# print STDERR "LINE IS \"$linecopy\".\n" if ($linenumdebug);
					$newlinecount = ($linecopy =~ tr/\n//);
					$blockOffset += $newlinecount - 1;
					print STDERR "NEWLINECOUNT: $newlinecount\n" if ($linenumdebug);
					print STDERR "BLOCKOFFSET: $blockOffset\n" if ($linenumdebug);

					my $newline = $inputLines[$inputCounter++];
					if ($lang eq "perl" || $lang eq "shell") {
						$newline =~ s/^\s*\#//s;
						$newline =~ s/\n( |\t)*\#/\n/sg;
						# print "NEWLINE: $newline\n";
					}
					$line .= $newline;
					print STDERR "INCREMENTED INPUTCOUNTER [M1]\n" if ($HeaderDoc::inputCounterDebug);
					# This is perfectly legal.  Don't warn
					# necessarily.
					if (!emptyHDok($line)) {
						warnHDComment(\@inputLines, $inputCounter, $blockOffset, "HeaderDoc comment", "1", $line);
					}
	        			print STDERR "Input line number[2]: $inputCounter\n" if ($localDebug);
					print STDERR "next line ".$inputLines[$inputCounter]."\n" if ($localDebug);
				} else {                                       # multi-line comment
					# my $in_textblock = 0; my $in_pre = 0;
					my $nInputLines = $nlines;
					do {
						my $templine = $inputLines[$inputCounter];
						# print STDERR "HERE: $templine\n";
						# while ($templine =~ s/\@textblock//io) { $in_textblock++; }  
						# while ($templine =~ s/\@\/textblock//io) { $in_textblock--; }
						# while ($templine =~ s/<pre>//io) { $in_pre++; print STDERR "IN PRE\n" if ($localDebug);}
						# while ($templine =~ s/<\/pre>//io) { $in_pre--; print STDERR "OUT OF PRE\n" if ($localDebug);}
						# if (!$in_textblock && !$in_pre) {
							# $inputLines[$inputCounter] =~ s/^[\t ]*[*]?[\t ]+(.*)$/$1/o; # remove leading whitespace, and any leading asterisks
							# if ($line !~ /\S/) {
								# $line = "<br><br>\n";
							# } 
						# }
						my $newline = $inputLines[$inputCounter++];
						print STDERR "INCREMENTED INPUTCOUNTER [M2]\n" if ($HeaderDoc::inputCounterDebug);
						warnHDComment(\@inputLines, $inputCounter, $blockOffset, "HeaderDoc comment", "2");
						$newline =~ s/^ \*//o;
						print STDERR "NEWLINE [A] IS $newline\n" if ($localDebug);
						if ($lang eq "perl" || $lang eq "shell") {
						    $newline =~ s/^\s*\#//o;
						}
						$line .= $newline;
	        				print STDERR "Input line number[3]: $inputCounter\n" if ($localDebug);
						print STDERR "next line ".$inputLines[$inputCounter]."\n" if ($localDebug);
					} while ((($lang eq "pascal" && ($inputLines[$inputCounter] !~ /\}/o)) ||($lang ne "pascal" && ($inputLines[$inputCounter] !~ s/\*\//\*\//so))) && ($inputCounter <= $nInputLines));
					my $newline = $inputLines[$inputCounter++];
					print STDERR "INCREMENTED INPUTCOUNTER [M3]\n" if ($HeaderDoc::inputCounterDebug);
					# This is not inherently wrong.
					if (!emptyHDok($line)) {
# print STDERR "LINE WAS $line\n";
						my $dectype = "HeaderDoc comment";
						if ($line =~ /^\s*\/\*\!\s*\@define(d)?block\s+/s) {
							$dectype = "defineblock";
						}
						warnHDComment(\@inputLines, $inputCounter, $blockOffset, $dectype, "3");
					}
					if ($lang eq "perl" || $lang eq "shell") {
					    print STDERR "NEWLINE [B] IS $newline\n" if ($localDebug);
					    $newline =~ s/^\s*\#//o;
					}
					if ($newline !~ /^ \*\//o) {
						$newline =~ s/^ \*//o;
					}
					$line .= $newline;              # get the closing comment marker
	        		print STDERR "Input line number[4]: $inputCounter\n" if ($localDebug);
				print STDERR "last line ".$inputLines[$inputCounter-1]."\n" if ($localDebug);
				print STDERR "next line ".$inputLines[$inputCounter]."\n" if ($localDebug);
			    } # end of multi-line comment

				# print STDERR "ic=$inputCounter\n" if ($localDebug);

			    # HeaderDoc-ize JavaDoc/PerlDoc comments
			    if (($lang eq "perl" || $lang eq "shell") && ($line =~ /^\s*\#\s*\/\*\!/o)) {
				$line =~ s/^\s*\#\s*\/\*\!/\/\*\!/o;
			    }
			    if (($lang eq "java" || $HeaderDoc::parse_javadoc) && ($line =~ /^\s*\/\*\*[^*]/o)) {
				$line =~ s/^\s*\/\*\*/\/\*\!/o;
			    }
			    $line =~ s/^\s+//o;              # trim leading whitespace
			    $line =~ s/^(.*)\*\/\s*$/$1/so;  # remove closing comment marker

			    print STDERR "CURRENT line \"$line\"\n" if ($localDebug);

			    ($inHeader, $inClass, $inInterface, $inCPPHeader, $inOCCHeader, $inPerlScript, $inShellScript, $inPHPScript, $inJavaSource, $inFunctionGroup, $inGroup, $inFunction, $inPDefine, $inTypedef, $inUnion, $inStruct, $inConstant, $inVar, $inEnum, $inMethod, $inAvailabilityMacro, $inUnknown, $classType, $line, $inputCounter, $blockOffset, $filename, $linenumdebug, $localDebug) = processTopLevel($inHeader, $inClass, $inInterface, $inCPPHeader, $inOCCHeader, $inPerlScript, $inShellScript, $inPHPScript, $inJavaSource, $inFunctionGroup, $inGroup, $inFunction, $inPDefine, $inTypedef, $inUnion, $inStruct, $inConstant, $inVar, $inEnum, $inMethod, $inAvailabilityMacro, $inUnknown, $classType, $line, $inputCounter, $blockOffset, $inputFile, $linenumdebug, $localDebug);

				# $inputCounter--; # inputCounter is current line.
				my $linenum = $inputCounter - 1;
				# $line =~ s/\n\n/\n<br><br>\n/go; # change newline pairs into HTML breaks, for para formatting
				my $fieldref = stringToFields($line, $fullpath, $linenum);
				@fields = @{$fieldref};
				$HeaderDoc::allow_multi = 1; # Treat any #if/#ifdef blocks we find as being blocks of similar functions.
			} elsif ($HeaderDoc::process_everything && allow_everything($lang, $sublang)) {
				# print STDERR "POINT\n";

				# print STDERR "IC: $inputCounter\n";
				my ($tempInputCounter, $dec, $type, $name, $pt, $value, $pplref, $returntype, $pridec,
				    $parseTree, $simpleTDcontents, $bpavail) = &HeaderDoc::BlockParse::blockParse($fullpath, $blockOffset, \@inputLines,
				    $inputCounter, 0, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes,
				    \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);

				if ($dec !~ /^(\/\*.*?\*\/|\/\/.*?(\n|\r)|\n|\r)*(\/\*\!)/) {
					print STDERR "DECLARATION WITH NO MARKUP ENCOUNTERED.\n" if ($processEverythingDebug);
					$inUnknown = 1; # Only process declaration if we don't encounter a HeaderDoc comment on the way.
				} else {
					print STDERR "DECLARATION WITH MARKUP ENCOUNTERED.\n" if ($processEverythingDebug);
				}
				$HeaderDoc::allow_multi = 0; # Drop any #if/#ifdef blocks or partial blocks on the floor for safety.
				@fields = ();
			} # end slurping up
			# print "CHECKPOINT: INUNKNOWN: $inUnknown\n";

				my $preAtPart = "";


				if ($inCPPHeader) {print STDERR "inCPPHeader\n" if ($debugging); $HeaderDoc::sublang="cpp"; };
				if ($inOCCHeader) {print STDERR "inCPPHeader\n" if ($debugging); $HeaderDoc::sublang="occ"; };
				if ($inPerlScript) {print STDERR "inPerlScript\n" if ($debugging); $lang="php";};
				if ($inPHPScript) {print STDERR "inPHPScript\n" if ($debugging); $lang="php";};
				if ($inJavaSource) {print STDERR "inJavaSource\n" if ($debugging); $lang="java";};
				if ($inHeader) {
					print STDERR "inHeader\n" if ($debugging); 
					$functionGroup = "";
					$HeaderDoc::globalGroup = "";
					processHeaderComment($apiOwner, $rootOutputDir, \@fields, $lang, $debugging, $reprocess_input);
					$HeaderDoc::currentClass = $apiOwner;
					$inputCounter--;
					print STDERR "DECREMENTED INPUTCOUNTER [M5]\n" if ($HeaderDoc::inputCounterDebug);
					if ($reprocess_input == 1) {
					    # my @cookedInputLines;
					    my $localDebug = 0;

					    # foreach my $line (@rawInputLines) {
						# foreach my $prefix (keys %HeaderDoc::perHeaderIgnorePrefixes) {
						    # if ($line =~ s/^\s*$prefix\s*//g) {
							# print STDERR "ignored $prefix\n" if ($localDebug);
						    # }
						# }
						# push(@cookedInputLines, $line);
					    # }
					    # @rawInputLines = @cookedInputLines;
					    $reprocess_input = 2;
					    goto REDO;
					}
				};
				if ($inGroup) {
					print STDERR "inGroup\n" if ($debugging); 
					my $rawname = $line;
					$rawname =~ s/.*\/\*!\s*\@(group|name)\s+//sio;
					$rawname =~ s/\s*\*\/.*//o;
					my ($name, $desc, $is_nameline_disc) = getAPINameAndDisc($rawname);
					$name =~ s/^\s+//smgo;
					$name =~ s/\s+$//smgo;

					if ($is_nameline_disc) { $name .= " ".$desc; $desc = ""; }

					print STDERR "group name is $name\n" if ($debugging);
					my $group = $apiOwner->addGroup($name); #(, $desc);
					$group->processComment(\@fields);
					$HeaderDoc::globalGroup = $name;
					$inputCounter--;
					print STDERR "DECREMENTED INPUTCOUNTER [M6]\n" if ($HeaderDoc::inputCounterDebug);
				};
				if ($inFunctionGroup) {
					print STDERR "inFunctionGroup\n" if ($debugging); 
					my $rawname = $line;
					if (!($rawname =~ s/.*\/\*!\s+\@functiongroup\s+//io)) {
						$rawname =~ s/.*\/\*!\s+\@methodgroup\s+//io;
						print STDERR "inMethodGroup\n" if ($debugging);
					}
					$rawname =~ s/\s*\*\/.*//o;
					my ($name, $desc, $is_nameline_disc) = getAPINameAndDisc($rawname);
					$name =~ s/^\s+//smgo;
					$name =~ s/\s+$//smgo;

					if ($is_nameline_disc) { $name .= " ".$desc; $desc = ""; }

					print STDERR "group name is $name\n" if ($debugging);
					my $group = $apiOwner->addGroup($name); # (, $desc);
					$group->processComment(\@fields);
					$functionGroup = $name;
					$inputCounter--;
					print STDERR "DECREMENTED INPUTCOUNTER [M7]\n" if ($HeaderDoc::inputCounterDebug);
				};


    if ($inUnknown || $inTypedef || $inStruct || $inEnum || $inUnion || $inConstant || $inVar || $inFunction || $inMethod || $inPDefine || $inClass || $inAvailabilityMacro) {
	# my $localDebug = 1;
	my $hangDebug  = 0;
	my $parmDebug  = 0;
	my $blockDebug = 0;

# print STDERR "WRAPPER: FIELDS:\n";
# foreach my $field (@fields) {
	# print STDERR "FIELD: $field\n";
# }
# print STDERR "ENDFIELDS\n";
# print STDERR "preAtPart: $preAtPart\n";

	if ($inClass && $debugging) { print STDERR "INCLASS (MAIN)\n";
		print STDERR "line is $line\n";
		print STDERR "IC: $inputCounter\n";
		print STDERR "CUR LINE: ".$inputLines[$inputCounter-1]."\n";
		print STDERR "NEXT LINE: ".$inputLines[$inputCounter]."\n";
	}
	# print STDERR "LINE_0:  ".$inputLines[$inputCounter + 0]."\n";
	# print STDERR "LINE_1:  ".$inputLines[$inputCounter + 1]."\n";
	# print STDERR "LINE_2:  ".$inputLines[$inputCounter + 2]."\n";
	# print STDERR "LINE_3:  ".$inputLines[$inputCounter + 3]."\n";
	# print STDERR "LINE_4:  ".$inputLines[$inputCounter + 4]."\n";

	my $subparse = 0;
	my $subparseTree = undef;
	my $classref = undef;
	my $catref = undef;
	my $newInputCounter;
	print STDERR "CALLING blockParseOutside WITH IC: $inputCounter (".$inputLines[$inputCounter]."), NODEC: ".$HeaderDoc::nodec."\n" if ($debugging);
	my $junk = undef;
	print STDERR "BLOCKOFFSET IN LOOP: $blockOffset\n" if ($linenumdebug);

	my $bpPrintDebug = $localDebug || 0;
                print STDERR "my (\$newInputCounter, \$cppAccessControlState, \$classType, \$classref, \$catref, \$blockOffset, \$numcurlybraces, \$foundMatch) =
            blockParseOutside($apiOwner, $inFunction, $inUnknown,
                $inTypedef, $inStruct, $inEnum, $inUnion,
                $inConstant, $inVar, $inMethod, $inPDefine,
                $inClass, $inInterface, $blockOffset, \@perHeaderCategoryObjects
,
                \@perHeaderClassObjects, $classType, $cppAccessControlState,
                \@fields, $fullpath, $functionGroup,
                $headerObject, $inputCounter, \@inputLines,
                $lang, $nlines, $preAtPart, $xml_output, $localDebug,
                $hangDebug, $parmDebug, $blockDebug, $subparse,
                $subparseTree, $HeaderDoc::nodec, $HeaderDoc::allow_multi);\n" if ($bpPrintDebug);
                print STDERR "FIELDS:\n" if ($bpPrintDebug);
                printArray(@fields) if ($bpPrintDebug);
		print "FIRSTLINE: ".$inputLines[$inputCounter]."\n" if ($bpPrintDebug);

	($newInputCounter, $cppAccessControlState, $classType, $classref, $catref, $blockOffset, $junk) =
	    blockParseOutside($apiOwner, $inFunction, $inUnknown,
		$inTypedef, $inStruct, $inEnum, $inUnion,
		$inConstant, $inVar, $inMethod, $inPDefine,
		$inClass, $inInterface, $blockOffset, \@perHeaderCategoryObjects,
		\@perHeaderClassObjects, $classType, $cppAccessControlState,
		\@fields, $fullpath, $functionGroup,
		$headerObject, $inputCounter, \@inputLines,
		$lang, $nlines, $preAtPart, $xml_output, $localDebug,
		$hangDebug, $parmDebug, $blockDebug, $subparse,
		$subparseTree, $HeaderDoc::nodec, $HeaderDoc::allow_multi);
	print STDERR "BLOCKOFFSET RETURNED: $blockOffset\n" if ($linenumdebug);
	$HeaderDoc::nodec = 0;
	@perHeaderClassObjects = @{$classref};
	@perHeaderCategoryObjects = @{$catref};

	# This fix for infinite loops is WRONG.
	# @@@ FIXME DAG @@@
	print "IC: $inputCounter NIC: $newInputCounter\n" if ($HeaderDoc::inputCounterDebug);
        # if ($inputCounter > $newInputCounter) {
                # $inputCounter++;
		# print STDERR "INCREMENTED INPUTCOUNTER [M8]\n" if ($HeaderDoc::inputCounterDebug);
        # } else {                        
        $inputCounter = $newInputCounter;
        # }                
    } # end "inUnknown, etc."
			$inCPPHeader = $inOCCHeader = $inPerlScript = $inShellScript = $inPHPScript = $inJavaSource = $inInterface = $inHeader = $inUnknown = $inFunction = $inAvailabilityMacro = $inFunctionGroup = $inGroup = $inTypedef = $inUnion = $inStruct = $inConstant = $inVar = $inPDefine = $inEnum = $inMethod = $inClass = 0;
	        $inputCounter++;
		print STDERR "INCREMENTED INPUTCOUNTER [M9] TO $inputCounter\n" if ($HeaderDoc::inputCounterDebug);
		print STDERR "Input line number[8]: $inputCounter\n" if ($localDebug);
	    } # end processing individual line array
	    print STDERR "DONE PROCESSING LINE ARRAY\n" if ($HeaderDoc::inputCounterDebug);

	    if (ref($apiOwner) ne "HeaderDoc::Header") { # if we've been filling a class/protocol/category object, add it to the header
	        my $name = $apiOwner->name();
	        my $refName = ref($apiOwner);

			# print STDERR "$classType : ";
			SWITCH: {
				($classType eq "php" ) && do { 
					push (@perHeaderClassObjects, $apiOwner);
					$headerObject->addToClasses($apiOwner); 
					last SWITCH; };
				($classType eq "java" ) && do { 
					push (@perHeaderClassObjects, $apiOwner);
					$headerObject->addToClasses($apiOwner); 
					last SWITCH; };
				($classType eq "cpp" ) && do { 
					push (@perHeaderClassObjects, $apiOwner);
					$headerObject->addToClasses($apiOwner); 
					last SWITCH; };
				($classType eq "cppt" ) && do { 
					push (@perHeaderClassObjects, $apiOwner);
					$headerObject->addToClasses($apiOwner); 
					last SWITCH; };
				($classType eq "occ") && do { 
					push (@perHeaderClassObjects, $apiOwner);
					if ($headerIncluded{$basefilename}) {
						$retainheader = 1;
					}
					$headerObject->addToClasses($apiOwner); 
					$objCClassNameToObject{$apiOwner->name()} = $apiOwner;
					last SWITCH; };           
				($classType eq "intf") && do { 
					push (@perHeaderClassObjects, $apiOwner);
					$headerObject->addToProtocols($apiOwner); 
					last SWITCH; 
				};           
				($classType eq "occCat") && do {
					push (@perHeaderCategoryObjects, $apiOwner);
					print STDERR "INSERTED CATEGORY into $headerObject\n" if ($ctdebug);
					$headerObject->addToCategories($apiOwner);
					last SWITCH; 
				};           
				($classType eq "C") && do {
					# $cppAccessControlState = "public:";
					$cppAccessControlState = "";
					push (@perHeaderClassObjects, $apiOwner);
					$headerObject->addToClasses($apiOwner);
					last SWITCH;
				};
			foreach my $testclassref ( $headerObject->classes() ) {
				my $testclass = %{$testclassref};
				bless($testclass, "HeaderDoc::APIOwner");
				bless($testclass, $testclass->class());
				print STDERR $testclass->name() . "\n";
			}
			my $linenum = $inputCounter - 1;
                    	print STDERR $headerObject->fullpath().":$linenum: warning: Unknown class type '$classType' (known: cpp, objC, intf, occCat)\n";		
			}
	    }
    } # end processing array of line arrays

    $headerObject->reparentModuleMembers();
    my @newobjs = ();
    foreach my $class (@perHeaderClassObjects) {
	if (!$class->isModule()) {
		push(@newobjs, $class);
	}
    }
    @perHeaderClassObjects = @newobjs;

	# SDump($headerObject, "point5");
    if ($retainheader) {
	push (@headerObjects, $headerObject);
	# print STDERR "Retaining header\n";
    }
    my ($headercpphashref, $headercpparghashref) = getAndClearCPPHash();
    my %headercpphash = %{$headercpphashref};
    my %headercpparghash = %{$headercpparghashref};

    my $includeListRef = $HeaderDoc::perHeaderIncludes{$fullpath};
    if ($includeListRef) {
	my @includeList = @{$includeListRef};
	print STDERR "LISTING PER HEADER INCLUDES\n" if ($includeDebug);
	foreach my $include (@includeList) {
		print STDERR "INCLUDE: $include\n" if ($includeDebug);
		my $pathname = $include;
		$pathname =~ s/^\s*//s;
		$pathname =~ s/\s*$//s;
		if ($pathname !~ s/^\<(.*)\>$/$1/s) {
			$pathname =~ s/^\"(.*)\"$/$1/s;
		}

		print STDERR "SANITIZED PATHNAME: $pathname\n" if ($includeDebug);
		my $includedfilename = basename($pathname);
		print STDERR "INCLUDED FILENAME: $includedfilename\n" if ($includeDebug);
		if ($HeaderDoc::HeaderFileCPPHashHash{$includedfilename}) {
			# Merge the hashes.

			print STDERR "FOUND.  MERGING HASHES\n" if ($includeDebug);
			%headercpphash = (%headercpphash, %{$HeaderDoc::HeaderFileCPPHashHash{$includedfilename}});
			%headercpparghash = (%headercpparghash, %{$HeaderDoc::HeaderFileCPPArgHashHash{$includedfilename}});
		}
		print STDERR "\n" if ($includeDebug);
	}
    } else {
	print STDERR "NO PER HEADER INCLUDES (NO REF)\n" if ($includeDebug);
    }

    # NOTE: These MUST not be modified to use the full filename or path.
    # If you do, C preprocessing interaction between headers will fail.
    $HeaderDoc::HeaderFileCPPHashHash{$basefilename} = \%headercpphash;
    $HeaderDoc::HeaderFileCPPArgHashHash{$basefilename} = \%headercpparghash;

    # This is safe to do on a per-header basis, as we've already forced
    # dependency ordering.
    foreach my $class (@perHeaderClassObjects) {
	if ($headerIncluded{$basefilename}) {
		# print STDERR "Retaining class\n";
		push(@classObjects, $class);
	}
    }
    if (@perHeaderClassObjects && !$xml_output) {
        foreach my $class (@perHeaderClassObjects) {
	    mergeClass($class);
        }
    }

    # print STDERR "CLASSES: ".scalar(@perHeaderClassObjects)."\n";
    # print STDERR "CATEGORIES: ".scalar(@perHeaderCategoryObjects)."\n";
    # print STDERR "HEADERS: ".scalar(@headerObjects)."\n";
    
    # foreach my $obj (@perHeaderCategoryObjects) {
	# print STDERR "CO: $obj\n";
    # }

    # we merge ObjC methods declared in categories into the owning class,
    # if we've seen it during processing.  Since we do dependency ordering,
    # we should have seen it by now if we're ever going to.
    if (@perHeaderCategoryObjects && !$xml_output) {
        foreach my $obj (@perHeaderCategoryObjects) {
            my $nameOfAssociatedClass = $obj->className();
            my $categoryName = $obj->categoryName();
            my $localDebug = 0;

	    # print STDERR "FOR CATEGORY: \"$categoryName\" CLASS IS \"$nameOfAssociatedClass\"\n";
        
		if (exists $objCClassNameToObject{$nameOfAssociatedClass}) {
			my $associatedClass = $objCClassNameToObject{$nameOfAssociatedClass};
			print STDERR "AC: $associatedClass\n" if ($localDebug);
			print STDERR "OBJ: $obj\n" if ($localDebug);
			my $methods = $obj->methods();
			$associatedClass->addToMethods($obj->methods());

			my $owner = $obj->headerObject();
			
			print STDERR "Found category with name $categoryName and associated class $nameOfAssociatedClass\n" if ($localDebug);
			print STDERR "Associated class exists\n" if ($localDebug);
			print STDERR "Added methods to associated class\n" if ($localDebug);
			if (ref($owner)) {
			    my $numCatsBefore = $owner->categories();
			    # $owner->printObject();
			    $owner->removeFromCategories($obj);
			    my $numCatsAfter = $owner->categories();
				print STDERR "Number of categories before: $numCatsBefore after:$numCatsAfter\n" if ($localDebug);
			    
			} else {
				my $fullpath = $HeaderDoc::headerObject->fullpath();
				my $linenum = $obj->linenum();
                    		print STDERR "$fullpath:$linenum: warning: Couldn't find Header object that owns the category with name $categoryName.\n";
			}
			my $assocapio = $associatedClass->APIOwner();
			if ($man_output) {
				$assocapio->writeHeaderElementsToManPage();
			} elsif ($function_list_output) {
				$assocapio->writeFunctionListToStdOut();
			} elsif ($xml_output) {
				$assocapio->writeHeaderElementsToXMLPage();
			} else {
				$assocapio->createFramesetFile();
				$assocapio->createTOCFile();
				$assocapio->writeHeaderElements(); 
				$assocapio->writeHeaderElementsToCompositePage();
				$assocapio->createContentFile() if (!$HeaderDoc::ClassAsComposite);
			}
			if ($doxytag_output) {
				$assocapio->writeHeaderElementsToDoxyFile();
			}
		} else {
			print STDERR "Found category with name $categoryName and associated class $nameOfAssociatedClass\n" if ($localDebug);
			print STDERR "Associated class doesn't exist\n" if ($localDebug);
            }
        }
    }
    # SDump($headerObject, "point5a");
    if ($man_output) {
	$headerObject->writeHeaderElementsToManPage();
    } elsif ($function_list_output) {
	$headerObject->writeFunctionListToStdOut();
    } elsif ($xml_output) {
	$headerObject->writeHeaderElementsToXMLPage();
    } else {
	# SDump($headerObject, "point5a1");
	$headerObject->createFramesetFile();
	# SDump($headerObject, "point5a2");
	$headerObject->createTOCFile();
	# SDump($headerObject, "point5a3");
	$headerObject->writeHeaderElements(); 
	# SDump($headerObject, "point5a4");
	$headerObject->writeHeaderElementsToCompositePage();
	# SDump($headerObject, "point5a5");
	$headerObject->createContentFile() if (!$HeaderDoc::ClassAsComposite);
	# SDump($headerObject, "point5a6");
    }
    if ($doxytag_output) {
	$headerObject->writeHeaderElementsToDoxyFile();
    }
    # SDump($headerObject, "point5b");
    if ("$write_control_file" eq "1") {
	print STDERR "Writing doc server control file... ";
	$headerObject->createMetaFile();
	print STDERR "done.\n";
    }
    # my $old_handle = select (STDOUT); # "select" STDOUT and save
                                  # previously selected handle
    # $| = 1; # perform flush after each write to STDOUT
    # print STDERR "Freeing data\n"; 
	# print STDERR "";
    # sleep(5);
    # SDump($headerObject, "point5c");
    foreach my $class (@perHeaderClassObjects) {
	if (!$headerIncluded{$basefilename}) {
		$class->free($retainheader ? 2 : 0);
	}
    }
    # SDump($headerObject, "point5d");
    if (!$retainheader) {
	$headerObject->free($headerIncluded{$basefilename});
	$HeaderDoc::headerObject = undef;
	$HeaderDoc::currentClass = undef;
    }
    # SDump($headerObject, "point6");
    $headerObject = undef;

	# SDump($headerObject, "point7");
    # select ($old_handle); # restore previously selected handle
    # print STDERR "freed.\n";
    # sleep(5);
}
    # sleep(5);

# if (!$quietLevel) {
    # print STDERR "======= Beginning post-processing =======\n";
# }
if ($doxytag_output) {
    mergeDoxyTags($specifiedOutputDir);
}

if ($quietLevel eq "0") {
    print STDERR "...done\n";
}

if ($HeaderDoc::exitstatus != 0) {
    print STDERR "WARNING: One or more input files could not be read.  Be sure to check the\n";
    print STDERR "output to make sure that all desired content was documented.\n";
}

# print STDERR "COUNTER: ".$HeaderDoc::counter."\n";
exit $HeaderDoc::exitstatus;


#############################  Subroutines ###################################


# /*! The mergeClass function is used for merging bits of
#     a superclass into subclasses when the \@superclass
#     tag is specified.
#
#     It is also always used for C psuedoclass classes
#     because any pseudo-superclass relationship isn't
#     really a superclass.
# */
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
				$HeaderDoc::ignore_apiuid_errors = 1;
				my $junk = $newobj->apirefSetup(1);
				$HeaderDoc::ignore_apiuid_errors = 0;
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
				$HeaderDoc::ignore_apiuid_errors = 1;
				my $junk = $newobj->apirefSetup(1);
				$HeaderDoc::ignore_apiuid_errors = 0;
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
				$HeaderDoc::ignore_apiuid_errors = 1;
				my $junk = $newobj->apirefSetup(1);
				$HeaderDoc::ignore_apiuid_errors = 0;
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
				$HeaderDoc::ignore_apiuid_errors = 1;
				my $junk = $newobj->apirefSetup(1);
				$HeaderDoc::ignore_apiuid_errors = 0;
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
				$HeaderDoc::ignore_apiuid_errors = 1;
				my $junk = $newobj->apirefSetup(1);
				$HeaderDoc::ignore_apiuid_errors = 0;
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
				$HeaderDoc::ignore_apiuid_errors = 1;
				my $junk = $newobj->apirefSetup(1);
				$HeaderDoc::ignore_apiuid_errors = 0;
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
				$HeaderDoc::ignore_apiuid_errors = 1;
				my $junk = $newobj->apirefSetup(1);
				$HeaderDoc::ignore_apiuid_errors = 0;
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
				$HeaderDoc::ignore_apiuid_errors = 1;
				my $junk = $newobj->apirefSetup(1);
				$HeaderDoc::ignore_apiuid_errors = 0;
			    }
		        }
		        if (@classes) {
			    foreach my $classref (@classes) {
				my $class = %{$class};
				bless($class, "HeaderDoc::APIOwner");
				bless($class, $class->class());
				if ($class->accessControl() eq "private") {
					next;
				}
				my $include = 1;
				foreach my $childclassref (@childclasses) {
				    my $childclass = %{$childclassref};
				    bless($class, "HeaderDoc::APIOwner");
				    bless($class, $class->class());
				    
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
				$HeaderDoc::ignore_apiuid_errors = 1;
				my $junk = $newobj->apirefSetup(1);
				$HeaderDoc::ignore_apiuid_errors = 0;
			    }
		        }
		    } # if ($merge_content)
		}
	    }
	}

	my $ai = $class->alsoInclude();
	if ($ai) {
		my @aiarray = @{$ai};
		foreach my $entry (@aiarray) {
			my $explicit = 0;
			my $uid = "";
			$entry =~ s/^\s*//s;
			$entry =~ s/\s*$//s;
			if ($entry =~ /^\/\//) {
				$uid = $entry;
				$explicit = 1;
			} else {
				$uid = resolveLink($entry, "included")
			}

			print "UID IS \"$uid\"\n";
			my $obj = objectForUID($uid);
			if (!$obj) {
				warn "Object for \"$uid\" could not be found.\n";
				if (!$explicit) {
					warn "    This should not happen.  Please file a bug.\n";
				}
			} else {
				my $objcl = ref($obj) || $obj;

				print "OBJ IS $obj\n";

				if ($objcl =~ /HeaderDoc::Function/) {
					$class->addToFunctions($obj);
					$obj->apiOwner()->removeFromFunctions($obj);
				} elsif ($objcl =~ /HeaderDoc::PDefine/) {
					$class->addToPDefines($obj);
					$obj->apiOwner()->removeFromPDefines($obj);
				} else {
					warn "Don't know how to add object of type $objcl to pseudoclass\n";
				}
			}
		}
	}
	$class->isMerged(1);
}

# /*! Merge doxygen tag files and delete the partial files. */
sub mergeDoxyTags
{
    my $outputDir = shift;

    find(\&getDoxyTagFiles, $outputDir);
    my $tagfileoutput = "";
    my $temp = $/;
    $/ = undef;
    foreach my $file (@doxyTagFiles) {
	open(MYFILE, "<$file");
	my $temp = <MYFILE>;
	$temp =~ s/^\s*<tagfile>\n*//s;
	$temp =~ s/\n\s*<\/tagfile>.*$//s;
	$tagfileoutput .= "\n".$temp;
	close(MYFILE);
	if (!$debugging) {
		unlink($file);
	}
    }
    $/ = $temp;
    $tagfileoutput =~ s/^\n//;

    my $tagfile = "$outputDir".$pathSeparator."doxytags.xml";
    open(MYFILE, ">$tagfile");
    print MYFILE "<tagfile>\n";
    print MYFILE $tagfileoutput;
    print MYFILE "\n</tagfile>\n";
    close(MYFILE);
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

	my $nlines = $#inputLines;
 	do {
	# print STDERR "inc\n";
		$tempLine = $inputLines[$lineCounter];
		$lineCounter++;
	} while (($tempLine !~ /class|\@class|\@interface|\@protocol|typedef\s+struct/o) && ($lineCounter <= $nlines));

	if ($tempLine =~ s/class\s//o) {
	 	$classType = "cpp";  
	}
	if ($tempLine =~ s/typedef\s+struct\s//o) {
	    # print STDERR "===>Cat: $tempLine\n";
	    $classType = "C"; # standard C "class", such as a
		                       # COM interface
	}
	if ($tempLine =~ s/(\@class|\@interface)\s//o) { 
	    if ($tempLine =~ /\(.*\)/o && ($1 ne "\@class")) {
			# print STDERR "===>Cat: $tempLine\n";
			$classType = "occCat";  # a temporary distinction--not in apple_ref spec
									# methods in categories will be lumped in with rest of class, if existent
		} else {
			# print STDERR "===>Class: $tempLine\n";
			$classType = "occ"; 
		}
	}
	if ($tempLine =~ s/\@protocol\s//o) {
	 	$classType = "intf";  
	}
	if ($lang eq "php") {
		$classType = "php";
	}
	if ($lang eq "java") {
		$classType = "java";
	}
print STDERR "determineClassType: returning $classType\n" if ($localDebug);
	if ($classType eq "unknown") {
		print STDERR "Bogus class ($tempLine)\n";
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
	my $inputCounter = shift;
	my $blockOffset = shift;
	my $linenum = $inputCounter + $blockOffset;
	
	SWITCH: {
		($classType eq "php" ) && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); last SWITCH; };
		($classType eq "java" ) && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); last SWITCH; };
		($classType eq "cpp" ) && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); last SWITCH; };
		($classType eq "cppt" ) && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); last SWITCH; };
		($classType eq "occ") && do { $apiOwner = HeaderDoc::ObjCClass->new; $apiOwner->filename($filename); last SWITCH; };           
		($classType eq "occCat") && do { $apiOwner = HeaderDoc::ObjCCategory->new; $apiOwner->filename($filename); last SWITCH; };           
		($classType eq "intf") && do { $apiOwner = HeaderDoc::ObjCProtocol->new; $apiOwner->filename($filename); last SWITCH; };           
		($classType eq "C") && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); $apiOwner->CClass(1); last SWITCH; };
		print STDERR "Unknown type ($classType). known: classes (ObjC and C++), ObjC categories and protocols\n";		
	}
	# preserve class nesting
	$apiOwner->linenuminblock($inputCounter);
	$apiOwner->blockoffset($blockOffset);
	# $apiOwner->linenum($linenum);
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
			($field =~ /^\/\*\!/o) && do {last SWITCH;}; # ignore opening /*!
			(($lang eq "java" || $HeaderDoc::parse_javadoc) && ($field =~ /^\s*\/\*\*[^*]/o)) && do {last SWITCH;}; # ignore opening /**
			($field =~ s/^(class|interface|template)(\s+)/$2/io) && 
				do {
					my ($name, $disc, $is_nameline_disc);
					my $filename = $HeaderDoc::headerObject->filename();
					# print STDERR "CLASSNAMEANDDISC:\n";
					($name, $disc, $is_nameline_disc) = &getAPINameAndDisc($field);
					my $classID = ref($apiOwner);
					$apiOwner->name($name);
					$apiOwner->filename($filename);
					if (length($disc)) {
						if ($is_nameline_disc) {
							$apiOwner->nameline_discussion($disc);
						} else {
							$apiOwner->discussion($disc);
						}
					}
					$functionGroup = "";
					$HeaderDoc::globalGroup = "";
                	last SWITCH;
            	};
			($field =~ /^see(also|)(\s+)/i) &&
				do {
					$apiOwner->see($field);
					last SWITCH;
				};
			($field =~ s/^protocol(\s+)/$1/io) && 
				do {
					my ($name, $disc, $is_nameline_disc);
					my $filename = $HeaderDoc::headerObject->filename();
					($name, $disc, $is_nameline_disc) = &getAPINameAndDisc($field); 
					$apiOwner->name($name);
					$apiOwner->filename($filename);
					if (length($disc)) {
						if ($is_nameline_disc) {
							$apiOwner->nameline_discussion($disc);
						} else {
							$apiOwner->discussion($disc);
						}
					}
					$functionGroup = "";
					$HeaderDoc::globalGroup = "";
					last SWITCH;
				};
			($field =~ s/^category(\s+)/$1/io) && 
				do {
					my ($name, $disc, $is_nameline_disc);
					my $filename = $HeaderDoc::headerObject->filename();
					($name, $disc, $is_nameline_disc) = &getAPINameAndDisc($field); 
					$apiOwner->name($name);
					$apiOwner->filename($filename);
					if (length($disc)) {
						if ($is_nameline_disc) {
							$apiOwner->nameline_discussion($disc);
						} else {
							$apiOwner->discussion($disc);
						}
					}
					$functionGroup = "";
					$HeaderDoc::globalGroup = "";
					last SWITCH;
				};
            			($field =~ s/^templatefield(\s+)/$1/io) && do {     
                                	$field =~ s/^\s+|\s+$//go;
                    			$field =~ /(\w*)\s*(.*)/so;
                    			my $fName = $1;
                    			my $fDesc = $2;
                    			my $fObj = HeaderDoc::MinorAPIElement->new();
					$fObj->linenuminblock($inputCounter);
					$fObj->blockoffset($blockOffset);
					# $fObj->linenum($linenum);
					$fObj->apiOwner($apiOwner);
                    			$fObj->outputformat($apiOwner->outputformat);
                    			$fObj->name($fName);
                    			$fObj->discussion($fDesc);
                    			$apiOwner->addToFields($fObj);
# print STDERR "inserted field $fName : $fDesc";
                                	last SWITCH;
                        	};
			($field =~ s/^super(class|)(\s+)/$2/io) && do { $apiOwner->attribute("Superclass", $field, 0); $apiOwner->explicitSuper(1); last SWITCH; };
			($field =~ s/^throws(\s+)/$1/io) && do {$apiOwner->throws($field); last SWITCH;};
			($field =~ s/^exception(\s+)/$1/io) && do {$apiOwner->throws($field); last SWITCH;};
			($field =~ s/^abstract(\s+)/$1/io) && do {$apiOwner->abstract($field); last SWITCH;};
			($field =~ s/^brief(\s+)/$1/io) && do {$apiOwner->abstract($field, 1); last SWITCH;};
			($field =~ s/^description(\s+|$)/$1/io) && do {$apiOwner->discussion($field); last SWITCH;};
			($field =~ s/^details(\s+|$)/$1/io) && do {$apiOwner->discussion($field); last SWITCH;};
			($field =~ s/^discussion(\s+|$)/$1/io) && do {$apiOwner->discussion($field); last SWITCH;};
			($field =~ s/^availability(\s+)/$1/io) && do {$apiOwner->availability($field); last SWITCH;};
			($field =~ s/^since(\s+)/$1/io) && do {$apiOwner->availability($field); last SWITCH;};
            		($field =~ s/^author(\s+)/$1/io) && do {$apiOwner->attribute("Author", $field, 0); last SWITCH;};
			($field =~ s/^version(\s+)/$1/io) && do {$apiOwner->attribute("Version", $field, 0); last SWITCH;};
            		($field =~ s/^deprecated(\s+)/$1/io) && do {$apiOwner->attribute("Deprecated", $field, 0); last SWITCH;};
            		($field =~ s/^version(\s+)/$1/io) && do {$apiOwner->attribute("Version", $field, 0); last SWITCH;};
			($field =~ s/^updated(\s+)/$1/io) && do {$apiOwner->updated($field); last SWITCH;};
			($field =~ s/^indexgroup(\s+)/$1/io) && do {$apiOwner->indexgroup($field); last SWITCH;};
	    ($field =~ s/^attribute(\s+)/$1/io) && do {
		    my ($attname, $attdisc, $is_nameline_disc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$apiOwner->attribute($attname, $attdisc, 0);
		    } else {
			warn $HeaderDoc::headerObject->fullpath().":$linenum: warning: Missing name/discussion for attribute\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributelist(\s+)/$1/io) && do {
		    $field =~ s/^\s*//so;
		    $field =~ s/\s*$//so;
		    my ($name, $lines) = split(/\n/, $field, 2);
		    $name =~ s/^\s*//so;
		    $name =~ s/\s*$//so;
		    $lines =~ s/^\s*//so;
		    $lines =~ s/\s*$//so;
		    if (length($name) && length($lines)) {
			my @attlines = split(/\n/, $lines);
			foreach my $line (@attlines) {
			    $apiOwner->attributelist($name, $line);
			}
		    } else {
			warn $HeaderDoc::headerObject->fullpath().":$linenum: warning: Missing name/discussion for attributelist\n";
		    }
		    last SWITCH;
		};
	    ($field =~ s/^attributeblock(\s+)/$1/io) && do {
		    my ($attname, $attdisc, $is_nameline_disc) = &getAPINameAndDisc($field);
		    if (length($attname) && length($attdisc)) {
			$apiOwner->attribute($attname, $attdisc, 1);
		    } else {
			warn $HeaderDoc::headerObject->fullpath().":$linenum: warning: Missing name/discussion for attributeblock\n";
		    }
		    last SWITCH;
		};
			($field =~ s/^namespace(\s+)/$1/io) && do {$apiOwner->namespace($field); last SWITCH;};
			($field =~ s/^instancesize(\s+)/$1/io) && do {$apiOwner->attribute("Instance Size", $field, 0); last SWITCH;};
			($field =~ s/^performance(\s+)/$1/io) && do {$apiOwner->attribute("Performance", $field, 1); last SWITCH;};
			# ($field =~ s/^subclass(\s+)/$1/io) && do {$apiOwner->attributelist("Subclasses", $field); last SWITCH;};
			($field =~ s/^nestedclass(\s+)/$1/io) && do {$apiOwner->attributelist("Nested Classes", $field); last SWITCH;};
			($field =~ s/^coclass(\s+)/$1/io) && do {$apiOwner->attributelist("Co-Classes", $field); last SWITCH;};
			($field =~ s/^helper(class|)(\s+)/$2/io) && do {$apiOwner->attributelist("Helper Classes", $field); last SWITCH;};
			($field =~ s/^helps(\s+)/$1/io) && do {$apiOwner->attribute("Helps", $field, 0); last SWITCH;};
			($field =~ s/^classdesign(\s+)/$1/io) && do {$apiOwner->attribute("Class Design", $field, 1); last SWITCH;};
			($field =~ s/^dependency(\s+)/$1/io) && do {$apiOwner->attributelist("Dependencies", $field); last SWITCH;};
			($field =~ s/^ownership(\s+)/$1/io) && do {$apiOwner->attribute("Ownership Model", $field, 1); last SWITCH;};
			($field =~ s/^security(\s+)/$1/io) && do {$apiOwner->attribute("Security", $field, 1); last SWITCH;};
			($field =~ s/^whysubclass(\s+)/$1/io) && do {$apiOwner->attribute("Reason to Subclass", $field, 1); last SWITCH;};
			# print STDERR "Unknown field in class comment: $field\n";
			warn $HeaderDoc::headerObject->fullpath().":$linenum: warning: Unknown field (\@$field) in class comment (".$apiOwner->name().")[1]\n";
		}
	}
	return $apiOwner;
}


sub mkdir_recursive
{
    my $path = shift;
    my $mask = shift;

    my @pathparts = split (/$pathSeparator/, $path);
    my $curpath = "";

    my $first = 1;
    foreach my $pathpart (@pathparts) {
	if ($first) {
	    $first = 0;
	    $curpath = $pathpart;
	} elsif (! -e "$curpath$pathSeparator$pathpart")  {
	    if (!mkdir("$curpath$pathSeparator$pathpart", 0777)) {
		return 0;
	    }
	    $curpath .= "$pathSeparator$pathpart";
	} else {
	    $curpath .= "$pathSeparator$pathpart";
	}
    }

    return 1;
}

sub strip
{
    my $filename = shift;
    my $short_output_path = shift;
    my $long_output_path = shift;
    my $input_path_and_filename = shift;
    my $inputRef = shift;
    my @inputLines = @$inputRef;
    my $localDebug = 0;

    # for same layout as HTML files, do this:
    # my $output_file = "$long_output_path$pathSeparator$filename";
    # my $output_path = "$long_output_path";

    # to match the input file layout, do this:
    my $output_file = "$short_output_path$pathSeparator$input_path_and_filename";
    my $output_path = "$short_output_path";

    my @pathparts = split(/($pathSeparator)/, $input_path_and_filename);
    my $junk = pop(@pathparts);

    my $input_path = "";
    foreach my $part (@pathparts) {
	$input_path .= $part;
    }

    if ($localDebug) {
	print STDERR "output path: $output_path\n";
	print STDERR "short output path: $short_output_path\n";
	print STDERR "long output path: $long_output_path\n";
	print STDERR "input path and filename: $input_path_and_filename\n";
	print STDERR "input path: $input_path\n";
	print STDERR "filename: $filename\n";
	print STDERR "output file: $output_file\n";
    }

    if (-e $output_file) {
	# don't risk writing over original header
	$output_file .= "-stripped";
	print STDERR "WARNING: output file exists.  Saving as\n\n";
	print STDERR "        $output_file\n\n";
	print STDERR "instead.\n";
    }

    # mkdir -p $output_path

    if (! -e "$output_path$pathSeparator$input_path")  {
	unless (mkdir_recursive ("$output_path$pathSeparator$input_path", 0777)) {
	    die "Error: $output_path$pathSeparator$input_path does not exist. Exiting. \n$!\n";
	}
    }

    open(OUTFILE, ">$output_file") || die "Can't write $output_file.\n";
    if ($^O =~ /MacOS/io) {MacPerl::SetFileInfo('R*ch', 'TEXT', "$output_file");};

    my $inComment = 0;
    my $text = "";
    my $localDebug = 0;
    foreach my $line (@inputLines) {
	print STDERR "line $line\n" if ($localDebug);
	print STDERR "inComment $inComment\n" if ($localDebug);
        if (($line =~ /^\/\*\!/o) || (($lang eq "java" || $HeaderDoc::parse_javadoc) && ($line =~ /^\s*\/\*\*[^*]/o))) {  # entering headerDoc comment
		# on entering a comment, set state to 1 (in comment)
		$inComment = 1;
	}
	if ($inComment && ($line =~ /\*\//o)) {
		# on leaving a comment, set state to 2 (leaving comment)
		$inComment = 2;
	}

	if (!$inComment) { $text .= $line; }

	if ($inComment == 2) {
		# state change back to 0 (we just skipped the last line of the comment)
		$inComment = 0;
	}
    }

# print STDERR "text is $text\n";
    print OUTFILE $text;

    close OUTFILE;
}

sub classLookAhead
{
    my $lineref = shift;
    my $inputCounter = shift;
    my $lang = shift;
    my $sublang = shift;

    my $searchname = "";
    if (@_) {
	$searchname = shift;
    }

    my @linearray = @{$lineref};
    my $inComment = 0;
    my $inILC = 0;
    my $inAt = 0;
    my $localDebug = 0;

    my ($sotemplate, $eotemplate, $operator, $soc, $eoc, $ilc, $ilc_b, $sofunction,
        $soprocedure, $sopreproc, $lbrace, $rbrace, $unionname, $structname,
	$enumname,
        $typedefname, $varname, $constname, $structisbrace, $macronameref,
	$classregexp, $classbraceregexp, $classclosebraceregexp, $accessregexp,
        $requiredregexp, $propname, $objcdynamicname, $objcsynthesizename)
		= parseTokens($lang, $sublang);

    my $nametoken = "";
    my $typetoken = "";
    my $occIntfName = "";
    my $occColon = 0;
    my $occParen = 0;

    my $nlines = scalar(@linearray);
    while ($inputCounter < $nlines) {
	my $line = $linearray[$inputCounter];
	my @parts = split(/((\/\*|\/\/|\*\/|\W))/, $line);

	print STDERR "CLALINE: $line\n" if ($localDebug);

	foreach my $token (@parts) {
		print STDERR "TOKEN: $token\n" if ($localDebug);
		if (!length($token)) {
			next;
			print STDERR "CLA:notoken\n" if ($localDebug);
		} elsif ($token eq "$soc") {
			$inComment = 1;
			print STDERR "CLA:soc\n" if ($localDebug);
		} elsif ($token eq "$ilc") {
			$inILC = 1;
			print STDERR "CLA:ilc\n" if ($localDebug);
		} elsif ($token eq "$eoc") {
			$inComment = 0;
			print STDERR "CLA:eoc\n" if ($localDebug);
		} elsif ($token =~ /\s+/o) {
			print STDERR "CLA:whitespace\n" if ($localDebug);
		} elsif ($inComment) {
			print STDERR "CLA:comment\n" if ($localDebug);
		} elsif (length($occIntfName)) {
			if ($token eq ":") {
				$occColon = 1;
				$occIntfName .= $token;
			} elsif ($occColon && $token !~ /\s/o) {
				my $testnameA = $occIntfName;
				$occIntfName .= $token;
				my $testnameB = $searchname;
				$testnameB =~ s/:$//so;
				if ((!length($searchname)) || $testnameA eq $testnameB) {
					return ($occIntfName, $typetoken);
				} else {
					$occIntfName = ""; $typetoken = "";
				}
			} elsif ($token eq "(") {
				$occParen++;
				$occIntfName .= $token;
			} elsif ($token eq ")") {
				$occParen--;
				$occIntfName .= $token;
			} elsif ($token =~ /\s/o) {
				$occIntfName .= $token;
			} elsif (!$occParen) {
				# return ($occIntfName, $typetoken);
				my $testnameA = $occIntfName;
				my $testnameB = $searchname;
				$testnameB =~ s/:$//so;
				if ((!length($searchname)) || $testnameA eq $testnameB) {
					return ($occIntfName, $typetoken);
				} else {
					$occIntfName = ""; $typetoken = "";
				}
			}
		} else {
			print STDERR "CLA:text\n" if ($localDebug);
			if ($token =~ /\;/o) {
				print STDERR "CLA:semi\n" if ($localDebug);
				next;
			} elsif ($token =~ /\@/o) {
				$inAt = 1;
				print STDERR "CLA:inAt\n" if ($localDebug);
			} elsif (!$inAt && $token =~ /^class$/o) {
				print STDERR "CLA:cpp_or_java_class\n" if ($localDebug);
				$typetoken = "class";
			} elsif ($inAt && $token =~ /^(class|interface|protocol)$/o) {
				print STDERR "CLA:occ_$1\n" if ($localDebug);
				$typetoken = $1;
			} else {
				# The first non-comment token isn't a class.
				if ($typetoken eq "") {
					print STDERR "CLA:NOTACLASS:\"$token\"\n" if ($localDebug);
					return ();
				} else {
					print STDERR "CLA:CLASSNAME:\"$token\"\n" if ($localDebug);
					if ($typetoken eq "interface") {
						$occIntfName = $typetoken;
					} else {
						if ((!length($searchname)) || $token eq $searchname) {
							print STDERR "RETURNING CLASS\n" if ($localDebug);
							return ($token, $typetoken);
						} else { $typetoken = ""; }
					}
				}
			}
		}
	}

	$inputCounter++; $inILC = 0;
	print STDERR "INCREMENTED INPUTCOUNTER [M12]\n" if ($HeaderDoc::inputCounterDebug);
    }

    # Yikes!  We ran off the end of the file!
    if (!length($searchname)) { warn "ClassLookAhead ran off EOF\n"; }
    return ();
}

# /*! Grab any #include directives. */
sub processIncludes($$)
{
    my $lineArrayRef = shift;
    my $pathname = shift;
    my @lines = @{$lineArrayRef};
    my $filename = basename($pathname);
    my $ah = HeaderDoc::AvailHelper->new();

    my @includeList = ();

    my $availDebug = 0;

    # The next few lines were a bad idea.  If you have two files
    # with the same name, the include lists got merged....
    # my $includeListRef = $HeaderDoc::perHeaderIncludes{$fullpath};
    # if ($includeListRef) {
	# @includeList = @{$includeListRef};
    # }

    my @ranges = ();
    my @rangestack = ();

    my $linenum = 1;
    my $continuation = 0;
    my $contline = "";
    foreach my $line (@lines) {
      if ($continuation) {
	if ($line =~ /\\\s*$/) {
		$contline .= $line;
	} else {
		my $rangeref = pop(@rangestack);
		my $range = ${$rangeref};

		$contline .= $line;
		$continuation = 0;

		$range->text($ah->parseString($contline, $pathname, $linenum));
		push(@rangestack, \$range);
	}
      } else {
	my $hackline = $line;
	if ($hackline =~ s/^\s*#include\s+//so) {
		my $incfile = "";
		if ($hackline =~ /^(<.*?>)/o) {
			$incfile = $1;
		} elsif ($hackline =~ /^(\".*?\")/o) {
			$incfile = $1;
		} else {
			warn "$pathname:$linenum: warning: Unable to determine include file name for \"$line\".\n";
		}
		if (length($incfile)) {
			push(@includeList, $incfile);
		}
	}
	if ($hackline =~ s/^\s*#ifdef\s+//so) {
		print STDERR "STARTRANGE ifdef: $hackline\n" if ($availDebug);
		my $range = HeaderDoc::LineRange->new();
		$range->start($linenum);
		push(@rangestack, \$range);
	}
	if ($hackline =~ s/^\s*#ifndef\s+//so) {
		print STDERR "STARTRANGE ifndef: $hackline\n" if ($availDebug);
		my $range = HeaderDoc::LineRange->new();
		$range->start($linenum);
		$range->text("");
		push(@rangestack, \$range);
	}
	if ($hackline =~ s/^\s*#if\s+//so) {
		print STDERR "STARTRANGE if: $hackline\n" if ($availDebug);
		my $range = HeaderDoc::LineRange->new();
		$range->start($linenum);

		if ($hackline =~ /\\\s*$/) {
			$continuation = 1;
		} else {
			$range->text($ah->parseString($hackline, $pathname, $linenum));
		}

		push(@rangestack, \$range);
	}
	if ($hackline =~ s/^\s*#endif\s+//so) {
		print STDERR "ENDRANGE: $hackline\n" if ($availDebug);
		my $rangeref = pop(@rangestack);
		if ($rangeref) {
			my $range = ${$rangeref};
			bless($range, "HeaderDoc::LineRange");
			$range->end($linenum);
			if (length($range->text())) { push(@ranges, \$range); }
		} else {
			warn "$pathname:$linenum: warning: Unbalanced #endif found in prescan.\n";
		}
	}
	$linenum++;
      }
    }

    if (0) {
	print STDERR "Includes for \"$filename\":\n";
	foreach my $name (@includeList) {
		print STDERR "$name\n";
	}
    }
    if ($availDebug) {
	print STDERR "Ranges for \"$filename\":\n";
	foreach my $rangeref (@ranges) {
		my $range = ${$rangeref};
		bless($range, "HeaderDoc::LineRange");
		print STDERR "-----\n";
		print STDERR "START: ".$range->start()."\n";
		print STDERR "END: ".$range->end()."\n";
		print STDERR "TEXT: ".$range->text()."\n";
	}
    }

    $HeaderDoc::perHeaderIncludes{$pathname} = \@includeList;
    $HeaderDoc::perHeaderRanges{$pathname} = \@ranges;
}

my %pathForInclude;

sub fix_dependency_order
{
    my $inputlistref = shift;
    my @inputfiles = @{$inputlistref};
    my $treetop = undef;
    # my %refhash = ();
    my $localDebug = 0;

    %pathForInclude = ();

    print STDERR "Scanning dependencies.\n" if ($localDebug || $debugging);
    my $foundcount = 0;
    foreach my $rawfilename (@inputfiles) {
	my $filename = basename($rawfilename);
	my $fullpath=getAbsPath($rawfilename);
	$pathForInclude{$filename} = $rawfilename;
	print STDERR "IN FILE: $filename:\n" if ($localDebug);
	# my $dep = HeaderDoc::Dependency->new;
	my $curnoderef = HeaderDoc::Dependency->findname($filename);
	my $curnode = undef;
	my $force = 0;
	if ($curnoderef) {
		print STDERR "Node exists\n" if ($localDebug);
		$curnode = ${$curnoderef};
		bless($curnode, "HeaderDoc::Dependency");
		if ($curnode->{EXISTS}) {
			print STDERR "Node marked with EXISTS.  Setting force -> 1\n" if ($localDebug);
			warn "WARNING: Multiple files named \"$filename\" found in argument\n".
			     "list.  Dependencies may not work as expected.\n";
			$force = 1;
		}
	}
	if (!$curnoderef || $force) {
		print STDERR "CNR: $curnoderef FORCE: $force\n" if ($localDebug);
		$curnode = HeaderDoc::Dependency->new();
		if (!$treetop) {
			$treetop = $curnode;
			$curnode = HeaderDoc::Dependency->new();
		}
		$curnode->name($rawfilename);
		$curnode->depname($filename);
		$curnode->{EXISTS} = 1;
	} else {
		print STDERR "CNR: $curnoderef\n" if ($localDebug);
		$curnode = ${$curnoderef};
		bless($curnode, "HeaderDoc::Dependency");
		print STDERR "    CN: $curnode\n" if ($localDebug);
		$curnode->name($rawfilename);
		$curnode->{EXISTS} = 1;
		$foundcount ++;
	}
        foreach my $include (@{$HeaderDoc::perHeaderIncludes{$fullpath}}) {
                print STDERR "    COMPARE INCLUDE: $include\n" if ($localDebug);
                my $tempname = $include;
		# my @oldlist = ();
                $tempname =~ s/^\s*//s;
                $tempname =~ s/\s*$//s;
                if ($tempname !~ s/^\<(.*)\>$/$1/s) {
                        $tempname =~ s/^\"(.*)\"$/$1/s;
                }
		my $rawincname = $tempname;
                $tempname = basename($tempname);
                print STDERR "    TMPNM: $tempname\n" if ($localDebug);
		# if ($refhash{$tempname}) {
			# @oldlist = @{$refhash{$tempname}};
		# }
		# push(@oldlist, $filename);
		# $refhash{$tempname} = \@oldlist;

		my $noderef = HeaderDoc::Dependency->findname($tempname);
		my $node = undef;
		if (!$noderef) {
			print STDERR "No existing reference found.\n" if ($localDebug);
			$node = HeaderDoc::Dependency->new();
			$node->name($rawincname);
			$node->depname($tempname);
		} else {
			print STDERR "Existing reference found.\n" if ($localDebug);
			$node = HeaderDoc::Dependency->new();
			$node = ${$noderef};
			bless($node, "HeaderDoc::Dependency");
		}
		$curnode->addchild($node);
		# print STDERR "$curnode -> $node\n";
	}
	$treetop->addchild($curnode);
    }
print STDERR "foundcount: $foundcount\n" if ($localDebug);

    # $treetop->dbprint() if ($localDebug);

    print STDERR "doing depth-first traversal.\n" if ($localDebug || $debugging);
    my $ret = depthfirst($treetop);
    if ($localDebug) {
	foreach my $entry (@{$ret}) {
		print STDERR "$entry ";
	}
    }
    $treetop = undef;
    print STDERR "\ndone.\n" if ($localDebug || $debugging);
    return $ret;
}

my @deplevels = ();
sub set_node_depths
{
    my $node = shift;
    my $depth = shift;
    my $localDebug = 0;

    if ($depth <= 1) { print STDERR "Generating depth for ".$node->{NAME}."\n" if ($localDebug); }

    if ($node->{MARKED}) {
	# Avoid infinite recursion or reparenting nodes to deeper depth than things they include.
	return;
    }
    $node->{MARKED} = 1;

    if ($node->{DEPTH} <= $depth || !$node->{DEPTH}) {
	# print STDERR "NODE DEPTH NOW $depth\n" if ($localDebug);
	$node->{DEPTH} = $depth;
    } else {
	# Nothing to do.
	$node->{MARKED} = 0;
	return;
    }

    foreach my $childref (@{$node->{CHILDREN}}) {
	my $child = ${$childref};
	bless($child, "HeaderDoc::Dependency");
	set_node_depths($child, $depth + 1);
    }

    $node->{MARKED} = 0;
}

my $maxdependencydepth = 0;

sub generate_depth_levels
{
    my $node = shift;
    my $depth = shift;
    my $localDebug = 0;

    if ($node->{MARKED}) {
	# Avoid infinite recursion or reparenting nodes to deeper depth than things they include.
	return;
    }
    $node->{MARKED} = 1;

    print STDERR "NODE DEPTH: ".$node->{DEPTH}."\n" if ($localDebug);;

    my @levelarr = ();
    if ($deplevels[$node->{DEPTH}]) {
	@levelarr = @{$deplevels[$node->{DEPTH}]};
    }
    push(@levelarr, \$node);
    $deplevels[$node->{DEPTH}] = \@levelarr;
    if ($node->{DEPTH} > $maxdependencydepth) {
	print STDERR "MAX DEPTH: $maxdependencydepth -> " if ($localDebug);
	$maxdependencydepth = $node->{DEPTH};
	print STDERR "$maxdependencydepth\n" if ($localDebug);
    }

    foreach my $childref (@{$node->{CHILDREN}}) {
	my $child = ${$childref};
	bless($child, "HeaderDoc::Dependency");
	generate_depth_levels($child, $depth + 1);
    }
}

sub depthfirst
{
	my @rawfiles = ();
	my $treetop = shift;
	# my $debugging = 1;

	print STDERR "Doing recursive sort by depth\n" if ($debugging);
	# my $depth = depthfirst_rec(\$treetop, 0);

	set_node_depths($treetop, 0);
	print STDERR "Generating depth levels\n" if ($debugging);
	generate_depth_levels($treetop);
	my $depth = $maxdependencydepth;

	$treetop->dbprint() if ($debugging);

	print STDERR "Sweeping levels from depth $maxdependencydepth:\n" if ($debugging);

	my $level = $depth;
	while ($level >= 0) {
		print STDERR "Level $level\n" if ($debugging);
		my @array = ();
		if ($deplevels[$level]) {
			@array = @{$deplevels[$level]};
		} else {
			print STDERR "No entries at level $level.  How peculiar.\n" if ($debugging);
		}
		foreach my $dep (@array) {
			$dep = ${$dep};
			bless($dep, "HeaderDoc::Dependency");
			print STDERR "Adding ".$dep->name()."\n" if ($debugging);
			# if ($pathForInclude{$dep->depname}) {
			if ($dep->{EXISTS}) {
				# push(@rawfiles, $pathForInclude{$dep->depname});
				push(@rawfiles, $dep->name());
			}
			# } else {
				# warn("DNE: ".$dep->name()."\n");
			# }
		}
		$level--;
	}
	print STDERR "done sweeping.\n" if ($debugging);

	my @files = ();
	my %namehash = ();
	my $filename;
	foreach $filename (@inputFiles) {
		# my $bn = basename($filename);
		print STDERR "File: $filename\n" if ($debugging);
		$namehash{$filename} = 1;
	}
	foreach my $filename (@rawfiles) {
		if (length($filename)) {
			# my $bn = basename($filename);
			if ($namehash{$filename}) {
				print STDERR "pushing $filename\n" if ($debugging);
				push(@files, $filename);
				$namehash{$filename} = 0; # include once.
			} else {
				print STDERR "skipping $filename\n" if ($debugging);
			}
		}
	}

	return \@files;
}

my %pathparts = ();
sub depthfirst_rec
{
	my $noderef = shift;
	my $level = shift;
	my $maxlevel = $level;
	my $norecurse = 0;
	# print STDERR "Depth: $level\n";

	if (!$noderef) { return; }
	# print STDERR "NODEREF: $noderef\n";

	my $node = ${$noderef};
	bless($node, "HeaderDoc::Dependency");
	# print STDERR "NAME: ".$node->name()."\n";

	if ($node->{MARKED}) {
		$norecurse = 1;
	}
	if ($pathparts{$node->depname()}) {
		return;
	}

	if ($node->{MARKED} < $level+1) {
		$node->{MARKED} = $level + 1;
	}

	# print STDERR "NODE: $node\n";
	if (!$norecurse && $node->{CHILDREN}) {
		my $opp = $pathparts{$node->depname()};
		if ($opp == undef) { $opp = 0; }
		$pathparts{$node->depname()} = 1;
		foreach my $child (@{$node->{CHILDREN}}) {
			my $templevel = depthfirst_rec($child, $level + 1);
			if ($templevel > $maxlevel) { $maxlevel = $templevel; }
		}
		$pathparts{$node->depname()} = $opp;
	}
	my @oldarr = ();
	if ($deplevels[$level]) {
		@oldarr = @{$deplevels[$level]};
	}
	push(@oldarr, \$node);
	$deplevels[$level] = \@oldarr;

	return $maxlevel;
}

sub printVersionInfo {
    my $bp = $HeaderDoc::BlockParse::VERSION;
    my $av = $HeaderDoc::APIOwner::VERSION;
    my $hev = $HeaderDoc::HeaderElement::VERSION;
    my $hv = $HeaderDoc::Header::VERSION;
    my $cppv = $HeaderDoc::CPPClass::VERSION;
    my $objcclassv = $HeaderDoc::ObjCClass::VERSION;
    my $objccnv = $HeaderDoc::ObjCContainer::VERSION;
    my $objccatv = $HeaderDoc::ObjCCategory::VERSION;
    my $objcprotocolv = $HeaderDoc::ObjCProtocol::VERSION;
    my $fv = $HeaderDoc::Function::VERSION;
    my $mv = $HeaderDoc::Method::VERSION;
    my $depv = $HeaderDoc::Dependency::VERSION;
    my $lr = $HeaderDoc::LineRange::VERSION;
    my $ah = $HeaderDoc::AvailHelper::VERSION;
    my $tv = $HeaderDoc::Typedef::VERSION;
    my $sv = $HeaderDoc::Struct::VERSION;
    my $cv = $HeaderDoc::Constant::VERSION;
    my $vv = $HeaderDoc::Var::VERSION;
    my $ev = $HeaderDoc::Enum::VERSION;
    my $uv = $HeaderDoc::Utilities::VERSION;
    my $me = $HeaderDoc::MinorAPIElement::VERSION;
    my $pd = $HeaderDoc::PDefine::VERSION;
    my $pt = $HeaderDoc::ParseTree::VERSION;
    my $ps = $HeaderDoc::ParserState::VERSION;
    my $ih = $HeaderDoc::IncludeHash::VERSION;
    my $ca = $HeaderDoc::ClassArray::VERSION;
    my $rg = $HeaderDoc::Regen::VERSION;
    
	print STDERR "---------------------------------------------------------------------\n";
	print STDERR "\tHeaderDoc Version: ".$HeaderDoc_Version."\n\n";

	print STDERR "\theaderDoc2HTML - $VERSION\n";
	print STDERR "\tModules:\n";
	print STDERR "\t\tAPIOwner - $av\n";
	print STDERR "\t\tAvailHelper - $ah\n";
	print STDERR "\t\tBlockParse - $bp\n";
	print STDERR "\t\tCPPClass - $cppv\n";
	print STDERR "\t\tClassArray - $ca\n";
	print STDERR "\t\tConstant - $cv\n";
	print STDERR "\t\tDependency - $depv\n";
	print STDERR "\t\tEnum - $ev\n";
	print STDERR "\t\tFunction - $fv\n";
	print STDERR "\t\tHeader - $hv\n";
	print STDERR "\t\tHeaderElement - $hev\n";
	print STDERR "\t\tIncludeHash - $ih\n";
	print STDERR "\t\tLineRange - $lr\n";
	print STDERR "\t\tMethod - $mv\n";
	print STDERR "\t\tMinorAPIElement - $me\n";
	print STDERR "\t\tObjCCategory - $objccatv\n";
	print STDERR "\t\tObjCClass - $objcclassv\n";
	print STDERR "\t\tObjCContainer - $objccnv\n";
	print STDERR "\t\tObjCProtocol - $objcprotocolv\n";
	print STDERR "\t\tPDefine - $pd\n";
	print STDERR "\t\tParseTree - $pt\n";
	print STDERR "\t\tParserState - $ps\n";
	print STDERR "\t\tStruct - $sv\n";
	print STDERR "\t\tTypedef - $tv\n";
	print STDERR "\t\tUtilities - $uv\n";
	print STDERR "\t\tVar - $vv\n";
	print STDERR "---------------------------------------------------------------------\n";
}

sub SDump
{
    my $arg = shift;
    my $text = shift;

    print STDERR "At position $text:\n";
    Dump($arg);
    print STDERR "End dump\n";

}

sub newtest
{
    $/ = "\n";
    print STDERR "Enter name of test\n";
    my $name = <STDIN>;
    $name =~ s/\n$//s;

    my $lang = "";
    my $sublang = "";
    while ($lang !~ /^(C|java|javascript|pascal|php|perl|Csource|shell|csh|IDL|MIG)$/) {
	print STDERR "Enter language (C|java|javascript|pascal|php|perl|Csource|shell|csh|IDL|MIG)\n";
	$lang = <STDIN>;
	$lang =~ s/\n$//s;
	if ($lang eq "IDL") {
		$lang = "C";
		$sublang = "IDL";
	} elsif ($lang eq "MIG") {
		$lang = "C";
		$sublang = "MIG";
	} elsif ($lang eq "csh") {
		$lang = "shell";
		$sublang = "csh";
	} elsif ($lang eq "javascript") {
		$lang = "java";
		$sublang = "javascript";
	} else {
		$sublang = $lang;
	}
    }
    $HeaderDoc::lang = $lang;
    $HeaderDoc::sublang = $sublang;

    my $type = "";
    if ($lang eq "C") {
	while ($type !~ /(parser|cpp)/) {
		print STDERR "Enter type of test (parser|cpp)\n";
		$type = <STDIN>;
		$type =~ s/\n$//s;
	}
    } else  {
	$type = "parser";
    }

    $/ = undef;
    my $cppcode = "";
    my $comment = "";

    if ($type eq "parser") {
	print STDERR "Paste in HeaderDoc comment block.\nPress control-d on a new line when done.\n";
	$comment = <STDIN>;
	seek(STDIN,0,0);

	print STDERR "Paste in block of code.\nPress control-d on a new line when done.\n";
    } else {
	print STDERR "Paste in initial macros.\nPress control-d on a new line when done.\n";
	$cppcode = <STDIN>;
	seek(STDIN,0,0);

	print STDERR "Paste in block of code to permute with this macro.\nPress control-d on a new line when done.\n";
    }
    my $code = <STDIN>;
    seek(STDIN,0,0);

    print STDERR "Optionally paste or type in a message to be displayed if the test fails.\nPress control-d on a new line when done.\n";
    my $failmsg = <STDIN>;
    seek(STDIN,0,0);

    my $test = HeaderDoc::Test->new( "NAME" => $name, "COMMENT" => $comment, "CODE" => $code, "LANG" => $lang, "SUBLANG" => $sublang, "TYPE" => $type, "CPPCODE" => $cppcode, "FAILMSG" => $failmsg );

    # Don't check return value here.  The "utilities" test will always fail
    # because the file has not yet been written to disk.
    $test->runTest();

    $test->{EXPECTED_RESULT} = $test->{RESULT};
    $test->{EXPECTED_RESULT_ALLDECS} = $test->{RESULT_ALLDECS};
    my $filename = $test->{FILENAME};
    # $filename =~ s/[^a-zA-Z0-9_.,-]/_/sg;

    if (-d "testsuite") {
	if ($type eq "parser") {
    		$filename = "testsuite/parser_tests/$filename";
	} else {
    		$filename = "testsuite/c_preprocessor_tests/$filename";
	}
    } else {
    	$filename = "/tmp/$filename.test";
    }

    if (-f $filename) {
	print "You are about to overwrite an existing test case.  Continue? (yes|no)\n";
	$/ = "\n";
	my $reply = <STDIN>;
	$reply =~ s/\n$//s;
	if ($reply ne "yes") {
		print "Cancelled.\n";
		exit(-1);
	}
    }
    $test->writeToFile("$filename");
    print "Wrote test data to \"$filename\"\n";
    $test->dbprint();
}


sub runtests
{
    my $mode = shift;
    my $argref = shift;
    my @args = @{$argref};

    # my $filename = "testsuite/parser_tests/test.test";
    my $ok_count = 0;
    my $fail_count = 0;
    my @testlist = undef;

    my $update = 0;
    if ($mode eq "update") {
	$update = 1;
    }


    my %config = (
	cCompiler => "/usr/bin/gcc"
    );

    my $localConfigFileName = "headerDoc2HTML.config";
    my $preferencesConfigFileName = "com.apple.headerDoc2HTML.config";

    my $CWD = cwd();
    my @configFiles = ($systemPreferencesPath.$pathSeparator.$preferencesConfigFileName, $usersPreferencesPath.$pathSeparator.$preferencesConfigFileName, $Bin.$pathSeparator.$localConfigFileName, $CWD.$pathSeparator.$localConfigFileName);

    %config = &updateHashFromConfigFiles(\%config,\@configFiles);

    $HeaderDoc::c_compiler = $config{cCompiler};

    print STDERR "Using C compiler: ".$HeaderDoc::c_compiler."\n";

    if ($#args == -1) {
	print "-= Running parser tests =-\n\n";
	@testlist = <testsuite/parser_tests/*.test>;

	my $dump;
	if ($mode eq "dump" || $mode eq "dump_parser") {
		$dump = 1;
	} else {
		$dump = 0;
	}
	my ($newok, $newfail) = runtestlist(\@testlist, $dump, $update);
	$ok_count += $newok;
	$fail_count += $newfail;

	print "-= Running C preprocessor tests =-\n\n";
	@testlist = <testsuite/c_preprocessor_tests/*.test>;
	if ($mode eq "dump" || $mode eq "dump_cpp") {
		$dump = 1;
	} else {
		$dump = 0;
	}
	($newok, $newfail) = runtestlist(\@testlist, $dump, $update);
	$ok_count += $newok;
	$fail_count += $newfail;

    } else {
	my $dump;
	if ($mode eq "dump") {
		$dump = 1;
	} else {
		$dump = 0;
	}
	my ($newok, $newfail) = runtestlist($argref, $dump, $update);
	$ok_count += $newok;
	$fail_count += $newfail;
    }

    print "\n\n-= SUMMARY =-\n\n";
    print "Tests passed: $ok_count\n";
    print "Tests failed: $fail_count\n";

    print "Percent passed: ";
    if ($fail_count != 0) {
	print "\e[31m";
    } else {
	print "\e[32m";
    }
    print "".(($ok_count / ($fail_count + $ok_count)) * 100)."\%\n";

    print "\e[30m\n";

    if ($fail_count) { $HeaderDoc::exitstatus = -1; }
}

sub runtestlist
{
	my $testlistref = shift;
	my @testlist = @{$testlistref};
	my $dump = shift;
	my $update = shift;

	my $ok_count = 0;
	my $fail_count = 0;

	foreach my $filename (@testlist) {
		my $test = HeaderDoc::Test->new();
		$test->readFromFile($filename);
		print "Test \"".$test->{NAME}."\": ";

		my $coretestfail = $test->runTest();
		if ($coretestfail) {
			die("\nTest suite aborted.  Utilities tests failed.\n");
		}

		if ($dump) {
			print "RESULTS DUMP:\n".$test->{RESULT}."\n";
		}
		if (($test->{RESULT} eq $test->{EXPECTED_RESULT}) &&
		    ($test->{RESULT_ALLDECS} eq $test->{EXPECTED_RESULT_ALLDECS})) {
				print "\e[32mOK\e[30m\n";
				# if ($dump) {
					# $test->showresults(); 
					# $test->dbprint();
				# }
				$ok_count++;
			# $test->showresults();
		} else {
			if ($test->{RESULT} eq $test->{EXPECTED_RESULT}) {
				print "\e[31mFAILED (ALLDECS ONLY)\e[30m\n";
				if ($debugging || 1) {
					if ($test->{RESULT_ALLDECS} eq $test->{EXPECTED_RESULT}) {
						print STDERR "Results same as with alldecs off\n";
					} else {
						print STDERR "\@\@\@ ALLDECS RESULT:\@\@\@\n".$test->{RESULT_ALLDECS}."\n\n\@\@\@EXPECTED NON-ALLDECS RESULT:\@\@\@\n".$test->{EXPECTED_RESULT}."\n\n\@\@\@END OF RESULTS\@\@\@\n\n";
					}
				}
			} else {
				print "\e[31mFAILED\e[30m\n";
			}
			if ($dump || $update) { $test->showresults(); }

			if ($update) {
				my $continue_update = 1;
				while ($continue_update) {
					print "If these changes are expected, please type 'confirm' now.\n";
					print "For more information, type 'more' now.\n";
					print "To run 'diff' on the named objects, type 'less' now.\n";
					print "To skip, type 'skip' now.\n";
					$/ = "\n";
					my $temp = <STDIN>;
					if ($temp =~ /^\s*less\s*$/) {
						print "\nTest \"".$test->{NAME}."\":\n";
						$test->showresults(-1);
					} elsif ($temp =~ /^\s*more\s*$/) {
						print "\nTest \"".$test->{NAME}."\":\n";
						$test->showresults(1);
						$test->dbprint();
					} elsif ($temp =~ /^\s*confirm\s*$/) {
						$test->{EXPECTED_RESULT} = $test->{RESULT};
						$test->{EXPECTED_RESULT_ALLDECS} = $test->{RESULT_ALLDECS};
						$test->writeToFile($filename);
						$ok_count++; $continue_update = 0;
					} elsif ($temp =~ /^\s*skip\s*$/) {
						$fail_count++; $continue_update = 0;
					} else {
						$temp =~ s/\n$//s;
						print "Unknown response \"$temp\"\n";
					}
				}
			} else {
				$fail_count++;
			}
		}
	}

	return ($ok_count, $fail_count);
}


################################################################################
# Version Notes
# 1.61 (02/24/2000) Fixed getLineArrays to respect paragraph breaks in comments that 
#                   have an asterisk before each line.
################################################################################

