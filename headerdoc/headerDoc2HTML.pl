#!/usr/bin/perl
#
# Script name: headerDoc2HTML
# Synopsis: Scans a file for headerDoc comments and generates an HTML
#           file from the comments it finds.
#
# Author: Matt Morse (matt@apple.com)
# Last Updated: $Date: 2003/08/20 17:50:30 $
#
# ObjC additions by SKoT McDonald <skot@tomandandy.com> Aug 2001 
#
# Copyright (c) 1999-2002 Apple Computer, Inc.  All Rights Reserved.
# The contents of this file constitute Original Code as defined in and are
# subject to the Apple Public Source License Version 1.1 (the "License").
# You may not use this file except in compliance with the License.  Please
# obtain a copy of the License at http://www.apple.com/publicsource and
# read it before using this file.
#
# This Original Code and all software distributed under the License are
# distributed on an AS IS basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the License for
# the specific language governing rights and limitations under the
# License.
#
# $Revision: 1.28.2.16 $
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
my $write_control_file;
#################### Locations #####################################
# Look-up tables are used when exporting API and doc to tab-delimited
# data files, which can be used for import to a database.  
# The look-up tables supply uniqueID-to-APIName mappings.

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
my @headerObjects;	# holds finished objects, ready for printing
					# we defer printing until all header objects are ready
					# so that we can merge ObjC category methods into the 
					# headerObject that holds the class, if it exists.
my @categoryObjects;	    # holds finished objects that represent ObjC categories
my %objCClassNameToObject;	# makes it easy to find the class object to add category methods to
					
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
	
    my %options = ();
    $lookupTableDirName = "LookupTables";
    $functionFilename = "functions.tab";;
    $typesFilename = "types.tab";
    $enumsFilename = "enumConstants.tab";

    $scriptDir = cwd();

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

    &getopts("HXhdvxquo:", \%options);
    if ($options{v}) {
    	print "Getting version information for all modules.  Please wait...\n";
		$printVersion = 1;
		return;
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
    
    sub getHeaders {
        my $filePath = $File::Find::name;
        my $fileName = $_;
        
        if ($fileName =~ /\.h$/) {
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
                            printHash updateHashFromConfigFiles getHashFromConfigFile);
use HeaderDoc::Header;
use HeaderDoc::CClass;
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
    htmlHeader => ""
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
my $inCPPHeader     = 0;
my $inClass         = 0; #includes CPPClass, ObjCClass ObjCProtocol
my $inFunction      = 0;
my $inFunctionGroup = 0;
my $inTypedef       = 0;
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

foreach my $inputFile (@inputFiles) {
	my $constantObj;
	my $enumObj;
	my $funcObj;
        my $methObj;
	my $pDefineObj;
	my $structObj;
	my $typedefObj;
	my $varObj;
	my $cppAccessControlState = "protected:"; # the default in C++
	my $objcAccessControlState = "private:"; # the default in Objective C
	
    my @path = split (/$pathSeparator/, $inputFile);
    my $filename = pop (@path);
    if ($quietLevel eq "0") {
	print "\nProcessing $filename\n";
    }
    @perHeaderIgnorePrefixes = ();
    $reprocess_input = 0;
    
    my $headerDir = join("$pathSeparator", @path);
    ($rootFileName = $filename) =~ s/\.(h|i)$//;
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
    if (!@headerDocCommentLines) {
	if ($quietLevel eq "0") {
            print "    Skipping. No HeaderDoc comments found.\n";
	}
        next;
    }
    
    $headerObject = HeaderDoc::Header->new();
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
    
    my $localDebug = 0;

    foreach my $arrayRef (@lineArrays) {

        my @inputLines = @$arrayRef;
	    # look for /*! comments and collect all comment fields into a the appropriate objects
        my $apiOwner = $headerObject;  # switches to a class/protocol/category object, when within a those declarations
	    print "inHeader\n" if ($localDebug);
	    my $inputCounter = 0;
	    my $classType = "unknown";
	    while ($inputCounter <= $#inputLines) {
			my $line = "";           
	        
	        	print "Input line number: $inputCounter\n" if ($localDebug);
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


	        	if ($inputLines[$inputCounter] =~ /^\s*\/\*\!/) {  # entering headerDoc comment
				# slurp up comment as line
				if ($inputLines[$inputCounter] =~ /\s*\*\//) { # closing comment marker on same line
					$line .= $inputLines[$inputCounter++];
	        			print "Input line number: $inputCounter\n" if ($localDebug);
				} else {                                       # multi-line comment
					my $in_textblock = 0;
					do {
						my $templine = $inputLines[$inputCounter];
						while ($templine =~ s/\@textblock//) { $in_textblock++; }  
						while ($templine =~ s/\@\/textblock//) { $in_textblock--; }
						if (!$in_textblock) {
							$inputLines[$inputCounter] =~ s/^[\t ]*[*]?[\t ]+(.*)$/$1/; # remove leading whitespace, and any leading asterisks
						}
						my $newline = $inputLines[$inputCounter++];
						$newline =~ s/^ \*//;
						$line .= $newline;
	        				print "Input line number: $inputCounter\n" if ($localDebug);
					} while (($inputLines[$inputCounter] !~ /\*\//) && ($inputCounter <= $#inputLines));
					my $newline = $inputLines[$inputCounter++];
					if ($newline !~ /^ \*\//) {
						$newline =~ s/^ \*//;
					}
					$line .= $newline;              # get the closing comment marker
	        		print "Input line number: $inputCounter\n" if ($localDebug);
			    }
			    $line =~ s/^\s+//;              # trim leading whitespace
			    $line =~ s/^(.*)\*\/\s*$/$1/s;  # remove closing comment marker
	           
				SWITCH: { # determine which type of comment we're in
					($line =~ /^\/\*!\s+\@header\s*/i) && do {$inHeader = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@template\s*/i) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@class\s*/i) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@protocol\s*/i) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@category\s*/i) && do {$inClass = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@language\s+.*c\+\+\s*/i) && do {$inCPPHeader = 1; last SWITCH;};
					($line =~ /^\/\*!\s+\@functiongroup\s*/i) && do {$inFunctionGroup = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@function\s*/i) && do {$inFunction = 1;last SWITCH;};
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
					($line =~ /^\/\*!\s+\@define(d)?\s*/i) && do {$inPDefine = 1;last SWITCH;};
					($line =~ /^\/\*!\s+\@enum\s*/i) && do {$inEnum = 1;last SWITCH;};
					my $linenum = $inputCounter - 1;
					print "$filename:$linenum:HeaderDoc comment is not of known type. Comment text is:\n";
					print "    $line\n";
				}
				my $linenum = $inputCounter - 1;
				$line =~ s/\n\n/\n<br><br>\n/g; # change newline pairs into HTML breaks, for para formatting
				my @fields = split(/\@/, $line);
				my @newfields = ();
				my $lastappend = "";
				my $in_textblock = 0;
				foreach my $field (@fields) {
				  print "processing $field\n" if ($localDebug);
				  if ($in_textblock) {
				    if ($field =~ /^\/textblock/) {
					print "out of textblock\n" if ($localDebug);
					if ($in_textblock == 1) {
					    my $cleanfield = $field;
					    $cleanfield =~ s/^\/textblock//;
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
				    $field =~ s/^\/link\s*/<\/hd_link>/;
				    if ($field =~ s/^link\s+//) {
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
					$field =~ s/^$target//g;
					$field =~ s/\\$/\@/;
					$lastappend .= "<hd_link $target>";
					$lastappend .= "$field";
				    } elsif ($field =~ /^textblock\s/) {
					if ($lastappend eq "") {
					    $in_textblock = 1;
					    print "in textblock\n" if ($localDebug);
					    $lastappend = pop(@newfields);
					} else {
					    $in_textblock = 2;
					    print "in textblock (continuation)\n" if ($localDebug);
					}
					$field =~ s/^textblock\s+//;
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
				@fields = @newfields;
				if ($inCPPHeader) {print "inCPPHeader\n" if ($debugging); &processCPPHeaderComment();};
				if ($inClass) {
					my $classdec = "";
					my $pos=$inputCounter;
					do {
						$classdec .= $inputLines[$pos];
					} while (($pos < $#inputLines) && ($inputLines[$pos++] !~ /{/));
					$classType = &determineClassType($inputCounter, $apiOwner, \@inputLines);
					print "inClass 1 - $classType \n" if ($debugging); 
					$apiOwner = &processClassComment($apiOwner, $rootOutputDir, \@fields, $classType);
					my $superclass = &get_super($classType, $classdec);
					if (length($superclass)) {
					    $apiOwner->attribute("Superclass", $superclass, 0);
					}
					print "inClass 2\n" if ($debugging); 
				};
				if ($inHeader) {
					print "inHeader\n" if ($debugging); 
					$apiOwner = &processHeaderComment($apiOwner, $rootOutputDir, \@fields);
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
				if ($inFunctionGroup) {
					print "inFunctionGroup\n" if ($debugging); 
					my $name = $line;
					$name =~ s/.*\/\*!\s+\@functiongroup\s*//i;
					$name =~ s/\s*\*\/.*//;
					$name =~ s/\n//smg;
					$name =~ s/^\s+//smg;
					$name =~ s/\s+$//smg;
					# print "group name is $name\n";
					$functionGroup = $name;
				};
				if ($inFunction) {
					print "inFunction $line\n" if ($localDebug);
					$funcObj = HeaderDoc::Function->new;
					if ($xml_output) {
					    $funcObj->outputformat("hdxml");
					} else { 
					    $funcObj->outputformat("html");
					}
\					$funcObj->group($functionGroup);
					$funcObj->processFunctionComment(\@fields);
	 				while ((($inputLines[$inputCounter] =~ /^%CPassThru/) || ($inputLines[$inputCounter] !~ /\w/))  && ($inputCounter <= $#inputLines)){ 
	 					$inputCounter++;
	 					print "Input line number: $inputCounter\n" if ($localDebug);
					}; # move beyond blank lines
					
					if ($inputLines[$inputCounter] =~ /^#define/) { # if function-like macro
					print "macro\n" if ($localDebug);
						my $declaration = $inputLines[$inputCounter];
						while (($declaration =~ /\\\n$/)  && ($inputCounter <= $#inputLines)){ 
							$inputCounter++;
							$declaration .= $inputLines[$inputCounter];
							print "Input line number: $inputCounter\n" if ($localDebug);
						}; # get escaped-newline lines
						$funcObj->setFunctionDeclaration($declaration);
					} else { # if regular function
						my $declaration = $inputLines[$inputCounter];
						if ($declaration !~ /;[^;]*$/) { # search for semicolon end, even with trailing comment
							do { 
								$inputCounter++;
								print "Input line number: $inputCounter\n" if ($localDebug);
								$declaration .= $inputLines[$inputCounter];
							} while (($declaration !~ /;[^;]*$/)  && ($inputCounter <= $#inputLines))
						}
						$declaration =~ s/^\s+//g;				# trim leading spaces.
						$declaration =~ s/([^;]*;).*$/$1/s;		# trim anything following the final semicolon, 
                                                                # including comments.
						$declaration =~ s/([^{]+){.*;$/$1;/s;   # handle inline functions [#2423551]
                                                                # by removing opening brace up to semicolon
						$declaration =~ s/\s+;/;/;				# trim spaces before semicolon.
						if ($declaration =~ /^virtual.*/) {
						    $funcObj->linkageState("virtual");
						} elsif ($declaration =~ /^static.*/) {
						    $funcObj->linkageState("static");
						} else {
						    $funcObj->linkageState("other");
						}
						$funcObj->setFunctionDeclaration($declaration);
					}

					if (length($funcObj->name())) {
						if (ref($apiOwner) ne "HeaderDoc::Header") {
						    if ($classType eq "occ" ||
							$classType eq "intf" ||
							$classType eq "occCat") {
							    $funcObj->accessControl($objcAccessControlState);
						    } else {
							    $funcObj->accessControl($cppAccessControlState);
						    }
						}
						$apiOwner->addToFunctions($funcObj);
					}
				}

				if ($inMethod) {
				    my $methodDebug = 0;
					print "inMethod $line\n" if ($methodDebug);
					$methObj = HeaderDoc::Method->new;
					if ($xml_output) {
					    $methObj->outputformat("hdxml");
					} else { 
					    $methObj->outputformat("html");
					}
					$methObj->processMethodComment(\@fields);
	 				while (($inputLines[$inputCounter] !~ /\w/)  && ($inputCounter <= $#inputLines)){ 
	 					$inputCounter++;
	 					print "Input line number: $inputCounter\n" if ($localDebug);
					}; # move beyond blank lines

					my $declaration = $inputLines[$inputCounter];
					if ($declaration !~ /;[^;]*$/) { # search for semicolon end, even with trailing comment
						do { 
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
						$methObj->owner($apiOwner); # methods need to know the class/protocol they belong to
						print "added method $declaration\n" if ($localDebug);
					}
				}

				if ($inTypedef) {
					$typedefObj = HeaderDoc::Typedef->new;
					if ($xml_output) {
					    $typedefObj->outputformat("hdxml");
					} else { 
					    $typedefObj->outputformat("html");
					}
					$typedefObj->processTypedefComment(\@fields);
					my $typedefName = $typedefObj->name();
					
	 				while (($inputLines[$inputCounter] !~ /\w/)  && ($inputCounter <= $#inputLines)){
	 					$inputCounter++;
						print "Input line number: $inputCounter\n" if ($localDebug);
	 				};
					my  $declaration = $inputLines[$inputCounter];
					# if a struct declaration precedes the typedef, suck it up
					if ($declaration !~ /typedef/) {
						while (($inputLines[$inputCounter] !~ /typedef/) && ($inputCounter <= $#inputLines)){
							$declaration .= $inputLines[++$inputCounter];
							print "Input line number: $inputCounter\n" if ($localDebug);
						};
					} else {
					    if ($declaration =~ /{/) { # taking care of a typedef'd block
							print "Entered case for $declaration, $typedefName\n" if ($localDebug);
							my $bracecount=0;
							my $test = $inputLines[$inputCounter];
							$bracecount += ($test =~ tr/{//);
# while ($test =~ /\{/gs) { $bracecount++; };
							$test = $inputLines[$inputCounter];
							$bracecount -= ($test =~ tr/}//);
# while ($test =~ /\}/gs) { $bracecount--; };
							print "Entered with count $bracecount\n" if ($localDebug);
							while ((($inputLines[$inputCounter] !~ /}/) 
							        || ($bracecount > 0)) 
							       && ($inputCounter <= $#inputLines)) {
# print "bc3 $bracecount";
							    $declaration .= $inputLines[++$inputCounter];
							    $test = $inputLines[$inputCounter];
							    $bracecount += ($test =~ tr/{//);
# while ($test =~ /\{/gs) { $bracecount++; };
							    $test = $inputLines[$inputCounter];
							    $bracecount -= ($test =~ tr/}//);
# while ($test =~ /\}/gs) { $bracecount--; };
								print "count $bracecount\n" if ($localDebug);
							    print "Input line number: $inputCounter\n" if ($localDebug);
							};
							print "count $bracecount left with declaration $declaration\n" if ($localDebug);
						} else {
						    # if ($declaration !~ /$typedefName/) { # find type name at end of declaration
								# while (($inputLines[$inputCounter] !~ /$typedefName/) && ($inputCounter <= $#inputLines)){
								    # $declaration .= $inputLines[++$inputCounter]; 
								    # print "Input line number: $inputCounter\n" if ($localDebug);
								# }
						    # } else { # find final semicolon
								while (($inputLines[$inputCounter] !~ /;/) && ($inputCounter <= $#inputLines)){
								    $declaration .= $inputLines[++$inputCounter]; 
								    print "Input line number: $inputCounter\n" if ($localDebug);
								}
						    # }
						}
					}
	                if (length($declaration)) {
			# DAG FIXME
			#$declaration .= "\n{";
                        $typedefObj->setTypedefDeclaration($declaration);
					} else {
						print "$filename:$linenum:Couldn't find a declaration for typedef\n";
					}
					if (length($typedefObj->name())) {
						if (ref($apiOwner) ne "HeaderDoc::Header") {
							$typedefObj->accessControl($cppAccessControlState);
							$apiOwner->addToVars($typedefObj);
						} else { # headers group by type
							$apiOwner->addToTypedefs($typedefObj);
					    }
					}
				}  ## end inTypedef
				
				if ($inStruct) { 
					$structObj = HeaderDoc::Struct->new;
					if ($inUnion) {
					    $structObj->isUnion(1);
					} else {
					    $structObj->isUnion(0);
					}
					if ($xml_output) {
					    $structObj->outputformat("hdxml");
					} else { 
					    $structObj->outputformat("html");
					}
					$structObj->processStructComment(\@fields);
	 				while (($inputLines[$inputCounter] !~ /\w/)  && ($inputCounter <= $#inputLines)){ $inputCounter++; print "Input line number: $inputCounter\n" if ($localDebug);}; # move beyond blank lines
					my  $declaration = $inputLines[$inputCounter];
					while ($inputLines[$inputCounter] !~ /}/ && ($inputCounter <= $#inputLines)) {$declaration .= $inputLines[++$inputCounter]; print "Input line number: $inputCounter\n" if ($localDebug);}; # simplistic
	                if (length($declaration)) {
						$structObj->setStructDeclaration($declaration);
					} else {
						warn "$filename:$linenum:Couldn't find a declaration for struct\n";
					}
					if (length($structObj->name())) {
						if (ref($apiOwner) ne "HeaderDoc::Header") {
							$structObj->accessControl($cppAccessControlState);
							$apiOwner->addToVars($structObj);
						} else { # headers group by type
							$apiOwner->addToStructs($structObj);
					    }
					}
				}  ## end inStruct
		       	       
				if ($inConstant) {
					$constantObj = HeaderDoc::Constant->new;
					if ($xml_output) {
					    $constantObj->outputformat("hdxml");
					} else { 
					    $constantObj->outputformat("html");
					}
					$constantObj->processConstantComment(\@fields);
					while (($inputLines[$inputCounter] !~ /\w/)  && ($inputCounter <= $#inputLines)){$inputCounter++; print "Input line number: $inputCounter\n" if ($localDebug);};
			        if ($inputLines[$inputCounter] =~ /^\s*(const|extern|CF_EXPORT)/) {
			           my $declaration = $inputLines[$inputCounter];
			           if ($declaration !~ /;\s*$/) {
			               $inputCounter++;
			               print "Input line number: $inputCounter\n" if ($localDebug);
				           while (($declaration !~ /\s*;\s*$/) && ($inputCounter <= $#inputLines)){$declaration .= $inputLines[$inputCounter++];print "Input line number: $inputCounter\n" if ($localDebug);};
				       }
				       $declaration =~ s/CF_EXPORT\s+//;
			           $constantObj->setConstantDeclaration($declaration);
			        }
					if (length($constantObj->name())) {
						if (ref($apiOwner) ne "HeaderDoc::Header") {
							$constantObj->accessControl($cppAccessControlState);
							$apiOwner->addToVars($constantObj);
						} else { # headers group by type
							$apiOwner->addToConstants($constantObj);
					    }
					}
				} ## end inConstant
				
				if ($inVar) {
					$varObj = HeaderDoc::Var->new;
					if ($xml_output) {
					    $varObj->outputformat("hdxml");
					} else { 
					    $varObj->outputformat("html");
					}
					$varObj->processVarComment(\@fields);
					while (($inputLines[$inputCounter] !~ /\w/)  && ($inputCounter <= $#inputLines)){  $inputCounter++; print "Input line number: $inputCounter\n" if ($localDebug);};
					my $declaration = &removeSlashSlashComment($inputLines[$inputCounter]);
					if ($declaration !~ /;\s*$/) {
						$inputCounter++;
						print "Input line number: $inputCounter\n" if ($localDebug);
						while (($declaration !~ /}\s*;\s*$/)  && ($inputCounter <= $#inputLines)){$declaration .= &removeSlashSlashComment($inputLines[$inputCounter++]); print "Input line number: $inputCounter\n" if ($localDebug);};
					}
					$varObj->setVarDeclaration($declaration);
					if (length($varObj->name())) {
						if (ref($apiOwner) ne "HeaderDoc::Header") {
							# $varObj->accessControl($cppAccessControlState);
						    if ($classType eq "occ" ||
							$classType eq "intf" ||
							$classType eq "occCat") {
							    $varObj->accessControl($objcAccessControlState);
						    } else {
							    $varObj->accessControl($cppAccessControlState);
						    }
						    $apiOwner->addToVars($varObj);
						} else { # headers group by type
						    # warn "### \@var tag found outside of a class declaration. \n";
						    warn "$filename:$linenum:\@var tag found outside of a class declaration. \n";
						    $varObj->printObject();
							$apiOwner->addToVars($varObj);  # we add it anyway
					    }
					}
				} ## end inVar

				if ($inPDefine) {
					$pDefineObj = HeaderDoc::PDefine->new;
					if ($xml_output) {
					    $pDefineObj->outputformat("hdxml");
					} else { 
					    $pDefineObj->outputformat("html");
					}
					$pDefineObj->processPDefineComment(\@fields);
					while (($inputLines[$inputCounter] !~ /\w/) && ($inputCounter <= $#inputLines)){  $inputCounter++;print "Input line number: $inputCounter\n" if ($localDebug);};
					my $declaration;
					if ($inputLines[$inputCounter] =~ /^\s*#define/) {
					    while (($inputLines[$inputCounter] =~ /^\s*#define/) && ($inputCounter <= $#inputLines)){
    						$declaration .= $inputLines[$inputCounter];
    						if ($declaration =~ /\\\n$/) {  # escaped newlines
								while (($declaration =~ /\\\n$/) && ($inputCounter <= $#inputLines)){ $inputCounter++; $declaration .= $inputLines[$inputCounter];print "Input line number: $inputCounter\n" if ($localDebug);};
    						}
    						$inputCounter++;
							print "Input line number: $inputCounter\n" if ($localDebug);
					    }
					    # We want to point to the last line
					    # copied, as this is incremented at
					    # the bottom of this function,
					    # and otherwise, we skip a line
					    # of parsing after each define.
					    $inputCounter--;
                    } else { 
			warn "$filename:$linenum:Can't find declaration for \@define comment with name:\n";
                    	my $name = $pDefineObj->name();
                    	warn "$filename:$linenum:$name\n\n";
                    }
					$pDefineObj->setPDefineDeclaration($declaration);
					
					if (length($pDefineObj->name())) {
						if (ref($apiOwner) ne "HeaderDoc::Header") {
							$pDefineObj->accessControl($cppAccessControlState);
							$apiOwner->addToVars($pDefineObj);
						} else { # headers group by type
							$apiOwner->addToPDefines($pDefineObj);
					    }
					}					
				} ## end inPDefine
				
				if ($inEnum) {
					$enumObj = HeaderDoc::Enum->new;
					if ($xml_output) {
					    $enumObj->outputformat("hdxml");
					} else { 
					    $enumObj->outputformat("html");
					}
					$enumObj->processEnumComment(\@fields);
	 				while (($inputLines[$inputCounter] !~ /\w/) && ($inputCounter <= $#inputLines)){ $inputCounter++;print "Input line number: $inputCounter\n" if ($localDebug);};  # move beyond blank lines
					my  $declaration = $inputLines[$inputCounter];
					while (($inputLines[$inputCounter] !~ /}/) && ($inputCounter <= $#inputLines)){$declaration .= $inputLines[++$inputCounter];print "Input line number: $inputCounter\n" if ($localDebug);}; # simplistic
					
	                if (length($declaration)) {
						$enumObj->declarationInHTML($enumObj->getEnumDeclaration($declaration));
					} else {
                    	warn "$filename:$linenum:Couldn't find a declaration for enum near line: $inputCounter\n";
					}
					if (length($enumObj->name())) {
						if (ref($apiOwner) ne "HeaderDoc::Header") {
							$enumObj->accessControl($cppAccessControlState);
							$apiOwner->addToVars($enumObj);
						} else { # headers group by type
							$apiOwner->addToEnums($enumObj);
					    }
					}
				}  ## end inEnum
	        }
			$inHeader = $inFunction = $inFunctionGroup = $inTypedef = $inUnion = $inStruct = $inConstant = $inVar = $inPDefine = $inEnum = $inMethod = $inClass = 0;
	        $inputCounter++;
		print "Input line number: $inputCounter\n" if ($localDebug);
	    } # end processing individual line array
	    
	    if (ref($apiOwner) ne "HeaderDoc::Header") { # if we've been filling a class/protocol/category object, add it to the header
	        my $name = $apiOwner->name();
	        my $refName = ref($apiOwner);

			# print "$classType : ";
			SWITCH: {
				($classType eq "cpp" ) && do { 
					$headerObject->addToClasses($apiOwner); 
					last SWITCH; };
				($classType eq "cppt" ) && do { 
					$headerObject->addToClasses($apiOwner); 
					last SWITCH; };
				($classType eq "occ") && do { 
					$headerObject->addToClasses($apiOwner); 
					$objCClassNameToObject{$apiOwner->name()} = $apiOwner;
					last SWITCH; };           
				($classType eq "intf") && do { 
					$headerObject->addToProtocols($apiOwner); 
					last SWITCH; 
				};           
				($classType eq "occCat") && do {
					push (@categoryObjects, $apiOwner);
					$headerObject->addToCategories($apiOwner); 
					last SWITCH; 
				};           
				($classType eq "C") && do {
					$cppAccessControlState = "public:";
					$headerObject->addToClasses($apiOwner);
					last SWITCH;
				};
			my $linenum = $inputCounter - 1;
                    	print "$filename:$linenum:Unknown class type '$classType' (known: cpp, objC, intf, occCat)\n";		
			}
	    }
    } # end processing array of line arrays
    push (@headerObjects, $headerObject);
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
			    $owner->removeFromCategories($obj);
			    my $numCatsAfter = $owner->categories();
				print "Number of categories before: $numCatsBefore after:$numCatsAfter\n" if ($localDebug);
			    
			} else {
				my $filename = $HeaderDoc::headerObject->filename();
                    		print "$filename:0:Couldn't find Header object that owns the category with name $categoryName.\n";
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

sub getLineArrays {
    my $rawLineArrayRef = shift;
    my @arrayOfLineArrays = ();
    my @generalHeaderLines = ();
	
    my $inputCounter = 0;
    my $lastArrayIndex = @{$rawLineArrayRef};
    my $line = "";
    my $className = "";
    
    while ($inputCounter <= $lastArrayIndex) {
        $line = ${$rawLineArrayRef}[$inputCounter];
        
        # we're entering a headerdoc comment--look ahead for @class tag
        if (($line =~ /^\/\*\!/)) {
			my $headerDocComment = "";
			my $classType = "";
			{
				local $^W = 0;  # turn off warnings since -w is overly sensitive here
				my $in_textblock = 0;
				while (($line !~ /\*\//) && ($inputCounter <= $lastArrayIndex)) {
				    my $templine = $line;
				    while ($templine =~ s/\@textblock//) { $in_textblock++; }
				    while ($templine =~ s/\@\/textblock//) { $in_textblock--; }
				    if (!$in_textblock) {
					$line =~ s/^[ \t]*//; # remove leading whitespace
				    }
				    $line =~ s/^[*]\s*$/\n/; # replace sole asterisk with paragraph divider
				    $line =~ s/^[*]\s+(.*)/$1/; # remove asterisks that precede text
				    $headerDocComment .= $line;
			            $line = ${$rawLineArrayRef}[++$inputCounter];
				}
				$headerDocComment .= $line
			}

			# test for @class, @protocol, or @category comment
			# here is where we create an array of class-specific lines
			# first, get the class name
			if ($headerDocComment =~ /^\/\*!\s+\@class|\@protocol|\@category\s*/i) {
				my @classLines;
			   
				($className = $headerDocComment) =~ s/.*\@class|\@protocol|\@category\s+(\w+)\s+.*/$1/s;
				push (@classLines, $headerDocComment);

				while (($line !~ /class\s|\@interface\s|\@protocol\s|typedef\s+struct\s/) && ($inputCounter <= $lastArrayIndex)) {
					$line = ${$rawLineArrayRef}[$inputCounter];
					push (@classLines, $line);  
					$inputCounter++;
				}
				my $initial_bracecount = ($line =~ tr/{//) - ($line =~ tr/}//);

				SWITCH: {
					($line =~ s/^\s*\@protocol\s+// ) && 
						do { 
							$classType = "objCProtocol";  
							# print "FOUND OBJCPROTOCOL\n"; 
							last SWITCH; 
						};
					($line =~ s/^\s*typedef\s+struct\s+// ) && 
						do { 
							$classType = "C";  
							# print "FOUND C CLASS\n"; 
							last SWITCH; 
						};
					($line =~ s/^\s*template\s+// ) && 
						do { 
							$classType = "cppt";  
							# print "FOUND CPP TEMPLATE CLASS\n"; 
							last SWITCH; 
						};
					($line =~ s/^\s*class\s+// ) && 
						do { 
							$classType = "cpp";  
							# print "FOUND CPP CLASS\n"; 
							last SWITCH; 
						};
					($line =~ s/^\s*\@interface\s+// ) && 
						do { 
						    # it's either an ObjC class or category
						    if ($line =~ /\(.*\)/) {
								$classType = "objCCategory"; 
								# print "FOUND OBJC CATEGORY\n"; 
						    } else {
								$classType = "objC"; 
								# print "FOUND OBJC CLASS\n"; 
						    }
							last SWITCH; 
						};
					print "Unknown class type (known: cpp, cppt, objCCategory, objCProtocol, C,)\nline=\"$line\"";		
				}

				# now we're at the opening line of the class declaration
				# push it into the array
				# print "INCLASS! (line: $inputCounter $line)\n";
				my $inClassBraces = $initial_bracecount;
				my $leftBraces = 0;
				my $rightBraces = 0;

				# make sure we've seen at least one left brace
				# at the start of the class

				do {
					$line = ${$rawLineArrayRef}[$inputCounter];
					push (@classLines, $line);
					$inputCounter++;
                                        $leftBraces = $line =~ tr/{//;
                                        $rightBraces = $line =~ tr/}//;
                                        $inClassBraces += $leftBraces;
                                        $inClassBraces -= $rightBraces;
				} while (($inputCounter <= $lastArrayIndex)
					    && (!($leftBraces)));
			   
				# now collect class lines until the closing
				# curly brace

				if (($classType =~ s/cpp//) || ($classType =~ s/^C//) || ($classType =~ s/cppt//)) {
		           	while ($inClassBraces && ($inputCounter <= $lastArrayIndex)) {
						$line = ${$rawLineArrayRef}[$inputCounter];
						push (@classLines, $line);
						$leftBraces = $line =~ tr/{//;
						$rightBraces = $line =~ tr/}//;
						$inClassBraces += $leftBraces;
						$inClassBraces -= $rightBraces;
						$inputCounter++;
					}
				}
				if (($classType =~ s/objC//) || ($classType =~ s/objCProtocol//) || ($classType =~ s/objCCategory//)) {
					while (($line !~ /\@end/) && ($inputCounter <= $lastArrayIndex)) {
						$line = ${$rawLineArrayRef}[$inputCounter];
						push (@classLines, $line);
						$inputCounter++;
					}
				}
				push (@arrayOfLineArrays, \@classLines);
			# print "OUT OF CLASS! (line: $inputCounter $line)\n";
				$inputCounter--;
			} else {
				push (@generalHeaderLines, $headerDocComment);
			}
		}
		push (@generalHeaderLines, $line);
		$inputCounter++;
	}
    push (@arrayOfLineArrays, \@generalHeaderLines);
    return @arrayOfLineArrays;
}

sub processCPPHeaderComment {
# 	for now, we do nothing with this comment
    return;
}

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

    $dec =~ s/\n/ /smg;

    if ($classType =~ /^occ/) {
	if ($dec !~ s/^\s*\@interface\s*//s) {
	    $dec =~ s/^\s*\@protocol\s*//s;
	}
        if (!($dec =~ s/.*?://s)) {
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

sub determineClassType {
	my $lineCounter   = shift;
	my $apiOwner      = shift;
	my $inputLinesRef = shift;
	my @inputLines    = @$inputLinesRef;
	my $classType = "unknown";
	my $tempLine = "";

 	do {
	# print "inc\n";
		$tempLine = $inputLines[$lineCounter];
		$lineCounter++;
	} while (($tempLine !~ /class|\@interface|\@protocol|typedef\s+struct/) && ($lineCounter <= $#inputLines));

	if ($tempLine =~ s/class\s//) {
	 	$classType = "cpp";  
	}
	if ($tempLine =~ s/typedef\s+struct\s//) {
	    # print "===>Cat: $tempLine\n";
	    $classType = "C"; # standard C "class", such as a
		                       # COM interface
	}
	if ($tempLine =~ s/\@interface\s//) { 
	    if ($tempLine =~ /\(.*\)/) {
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
	return $classType;
}

sub processClassComment {
	my $apiOwner = shift;
	my $headerObj = $apiOwner;
	my $rootOutputDir = shift;
	my $fieldArrayRef = shift;
	my @fields = @$fieldArrayRef;
	my $classType = shift;
	my $filename = $HeaderDoc::headerObject->filename();
	
	SWITCH: {
		($classType eq "cpp" ) && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); last SWITCH; };
		($classType eq "cppt" ) && do { $apiOwner = HeaderDoc::CPPClass->new; $apiOwner->filename($filename); last SWITCH; };
		($classType eq "occ") && do { $apiOwner = HeaderDoc::ObjCClass->new; $apiOwner->filename($filename); last SWITCH; };           
		($classType eq "occCat") && do { $apiOwner = HeaderDoc::ObjCCategory->new; $apiOwner->filename($filename); last SWITCH; };           
		($classType eq "intf") && do { $apiOwner = HeaderDoc::ObjCProtocol->new; $apiOwner->filename($filename); last SWITCH; };           
		($classType eq "C") && do { $apiOwner = HeaderDoc::CClass->new; $apiOwner->filename($filename); last SWITCH; };
		print "Unknown type (known: classes (ObjC and C++), ObjC categories and protocols)\n";		
	}
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
			($field =~ s/^class\s+//) && 
				do {
					my ($name, $disc);
					($name, $disc) = &getAPINameAndDisc($field);
					my $classID = ref($apiOwner);
					if (length($name)) {
						$apiOwner->name($name);
					} else {
						my $filename = $HeaderDoc::headerObject->filename();
                    				print "$filename:0:Did not find class name following \@class tag!\n";
					}
					if (length($disc)) {$apiOwner->discussion($disc);};
                	last SWITCH;
            	};
			($field =~ s/^protocol\s+//) && 
				do {
					my ($name, $disc);
					($name, $disc) = &getAPINameAndDisc($field); 
					if (length($name)) {
						$apiOwner->name($name);
					} else {
						my $filename = $HeaderDoc::headerObject->filename();
                    				warn "$filename:0:Did not find protocol name following \@protocol tag!\n";
					}
					if (length($disc)) {$apiOwner->discussion($disc);};
					last SWITCH;
				};
			($field =~ s/^category\s+//) && 
				do {
					my ($name, $disc);
					($name, $disc) = &getAPINameAndDisc($field); 
					if (length($name)) {
						$apiOwner->name($name);
					} else {
						my $filename = $HeaderDoc::headerObject->filename();
                    				warn "$filename:0:Did not find category name following \@protocol tag!\n";
					}
					if (length($disc)) {$apiOwner->discussion($disc);};
					last SWITCH;
				};
            			($field =~ s/^templatefield\s+//) && do {     
                                	$field =~ s/^\s+|\s+$//g;
                    			$field =~ /(\w*)\s*(.*)/s;
                    			my $fName = $1;
                    			my $fDesc = $2;
                    			my $fObj = HeaderDoc::MinorAPIElement->new();
                    			$fObj->outputformat($apiOwner->outputformat);
                    			$fObj->name($fName);
                    			$fObj->discussion($fDesc);
                    			$apiOwner->addToFields($fObj);
# print "inserted field $fName : $fDesc";
                                	last SWITCH;
                        	};
			($field =~ s/^throws\s+//) && do {$apiOwner->throws($field); last SWITCH;};
			($field =~ s/^exception\s+//) && do {$apiOwner->throws($field); last SWITCH;};
			($field =~ s/^abstract\s+//) && do {$apiOwner->abstract($field); last SWITCH;};
			($field =~ s/^discussion\s+//) && do {$apiOwner->discussion($field); last SWITCH;};
			($field =~ s/^availability\s+//) && do {$apiOwner->availability($field); last SWITCH;};
			($field =~ s/^updated\s+//) && do {$apiOwner->updated($field); last SWITCH;};
			($field =~ s/^namespace\s+//) && do {$apiOwner->namespace($field); last SWITCH;};
			($field =~ s/^instancesize\s+//) && do {$apiOwner->attribute("Instance Size", $field, 0); last SWITCH;};
			($field =~ s/^performance\s+//) && do {$apiOwner->attribute("Performance", $field, 1); last SWITCH;};
			# ($field =~ s/^subclass\s+//) && do {$apiOwner->attributelist("Subclasses", $field); last SWITCH;};
			($field =~ s/^nestedclass\s+//) && do {$apiOwner->attributelist("Nested Classes", $field); last SWITCH;};
			($field =~ s/^coclass\s+//) && do {$apiOwner->attributelist("Co-Classes", $field); last SWITCH;};
			($field =~ s/^helper(class|)\s+//) && do {$apiOwner->attributelist("Helper Classes", $field); last SWITCH;};
			($field =~ s/^helps\s+//) && do {$apiOwner->attribute("Helps", $field, 0); last SWITCH;};
			($field =~ s/^classdesign\s+//) && do {$apiOwner->attribute("Class Design", $field, 1); last SWITCH;};
			($field =~ s/^dependency\s+//) && do {$apiOwner->attributelist("Dependencies", $field); last SWITCH;};
			($field =~ s/^ownership\s+//) && do {$apiOwner->attribute("Ownership Model", $field, 1); last SWITCH;};
			($field =~ s/^security\s+//) && do {$apiOwner->attribute("Security", $field, 1); last SWITCH;};
			($field =~ s/^whysubclass\s+//) && do {$apiOwner->attribute("Reason to Subclass", $field, 1); last SWITCH;};
			print "Unknown field in class comment: $field\n";
		}
	}
	return $apiOwner;
}


sub processHeaderComment {
    my $apiOwner = shift;
    my $rootOutputDir = shift;
    my $fieldArrayRef = shift;
    my @fields = @$fieldArrayRef;
    my $localDebug = 0;

	foreach my $field (@fields) {
	    # print "header field: |$field|\n";
		SWITCH: {
			($field =~ /^\/\*\!/)&& do {last SWITCH;}; # ignore opening /*!
			($field =~ s/^header\s+//) && 
			do {
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
            ($field =~ s/^updated\s+//) && do {$apiOwner->updated($field); last SWITCH;};
            ($field =~ s/^abstract\s+//) && do {$apiOwner->abstract($field); last SWITCH;};
            ($field =~ s/^discussion\s+//) && do {$apiOwner->discussion($field); last SWITCH;};
            ($field =~ s/^copyright\s+//) && do { $apiOwner->headerCopyrightOwner($field); last SWITCH;};
            ($field =~ s/^meta\s+//) && do {$apiOwner->HTMLmeta($field); last SWITCH;};
            ($field =~ s/^CFBundleIdentifier\s+//i) && do {$apiOwner->attribute("CFBundleIdentifier", $field, 0); last SWITCH;};
            ($field =~ s/^related\s+//i) && do {$apiOwner->attributelist("Related Headers", $field); last SWITCH;};
            ($field =~ s/^(compiler|)flag\s+//) && do {$apiOwner->attributelist("Compiler Flags", $field); last SWITCH;};
            ($field =~ s/^preprocinfo\s+//) && do {$apiOwner->attribute("Preprocessor Behavior", $field, 1); last SWITCH;};
	    ($field =~ s/^whyinclude\s+//) && do {$apiOwner->attribute("Reason to Include", $field, 1); last SWITCH;};
            ($field =~ s/^ignore\s+//) && do { $field =~ s/\n//smg; push(@perHeaderIgnorePrefixes, $field); if (!($reprocess_input)) {$reprocess_input = 1;} print "ignoring $field" if ($localDebug); last SWITCH;};
            print "Unknown field in header comment: $field\n";
		}
	}


	return $apiOwner;
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

