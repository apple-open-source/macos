#! /usr/bin/perl -w
#
# Class name: Test
# Synopsis: Test Harness
#
# Last Updated: $Date: 2009/03/30 19:38:52 $
#
# Copyright (c) 2008 Apple Computer, Inc.  All rights reserved.
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

package HeaderDoc::Test;

use HeaderDoc::Utilities qw(findRelativePath safeName getAPINameAndDisc printArray printHash unregisterUID registerUID quote html2xhtml sanitize parseTokens unregister_force_uid_clear dereferenceUIDObject filterHeaderDocTagContents validTag stringToFields processHeaderComment getLineArrays getAbsPath allow_everything getAvailabilityMacros);
use File::Basename;
use strict;
use vars qw($VERSION @ISA);
use Cwd;
use POSIX qw(strftime mktime localtime);
use Carp qw(cluck);
use HeaderDoc::Utilities qw(processTopLevel);
use HeaderDoc::BlockParse qw(blockParseOutside blockParse getAndClearCPPHash);

# print STDERR "Do we have FreezeThaw?  ".($HeaderDoc::FreezeThaw_available ? "yes" : "no")."\n";

if ($HeaderDoc::FreezeThaw_available) {
	eval "use FreezeThaw qw(freeze thaw)";
}

$HeaderDoc::Test::VERSION = '$Revision: 1.34 $';

sub new {
    my $param = shift;
    my $class = ref($param) || $param;
    my $self = {};
    
    bless($self, $class);
    $self->_initialize();
    # Now grab any key => value pairs passed in
    my (%attributeHash) = @_;
    foreach my $key (sort keys(%attributeHash)) {
	my $ucKey = uc($key);
	$self->{$ucKey} = $attributeHash{$key};
    }
    $self->{FILENAME} = $self->{NAME};
    $self->{FILENAME} =~ s/[^a-zA-Z0-9_.,-]/_/sg;
    $self->{FILENAME} .= ".test";

    # $self->dbprint();
    return $self;
}

sub _initialize {
    my $self = shift;

    $self->{FILENAME} = undef;
    $self->{FAILMSG} = undef;
    $self->{NAME} = undef;
    $self->{TYPE} = undef;
    $self->{LANG} = undef;
    $self->{SUBLANG} = undef;
    $self->{COMMENT} = undef;
    $self->{CPPCODE} = undef;
    $self->{CODE} = undef;
    $self->{RESULT} = undef;
    $self->{RESULT_ALLDECS} = undef;
    $self->{EXPECTED_RESULT} = undef;
    $self->{EXPECTED_RESULT_ALLDECS} = undef;
}

sub runTest {
    my $self = shift;

    my $coretestfail = $self->runtest_sub(0);
    if ($self->supportsAllDecs()) {
	my $coretestfail_b = $self->runtest_sub(1);
	$coretestfail = $coretestfail || $coretestfail_b;
    }
    return $coretestfail;
}

sub runtest_sub {
    my $self = shift;
    my $alldecs = shift;

    my $results = "";

    my $testDebug = 0;

    my $prevignore = $HeaderDoc::ignore_apiuid_errors;
    $HeaderDoc::ignore_apiuid_errors = 1;
    $HeaderDoc::test_mode = 1;
    $HeaderDoc::curParserState = undef;
    use strict;

    $HeaderDoc::globalGroup = "";
    $HeaderDoc::dumb_as_dirt = 0;
    $HeaderDoc::parse_javadoc = 1;
    $HeaderDoc::IncludeSuper = 0;
    $HeaderDoc::ClassAsComposite = 1;
    $HeaderDoc::process_everything = $alldecs;
    $HeaderDoc::align_columns = 0;
    $HeaderDoc::groupright = 1;
    $HeaderDoc::ignore_apiowner_names = 0;
    $HeaderDoc::add_link_requests = 1;
    $HeaderDoc::truncate_inline = 0;
    $HeaderDoc::sort_entries = 1;
    $HeaderDoc::enableParanoidWarnings = 0;
    $HeaderDoc::outerNamesOnly = 0;
    $HeaderDoc::AccessControlState = "";
    $HeaderDoc::idl_language = "idl";
    %HeaderDoc::availability_defs = ();
    %HeaderDoc::availability_has_args = ();

    # warn "MP: ".$HeaderDoc::modulesPath."Availability.list\n";
    getAvailabilityMacros($HeaderDoc::modulesPath."Availability.list", 1);

    my $basefilename = basename($self->{FILENAME});
    my $coretestfail = 0;

    my $fullpath=getAbsPath($self->{FILENAME});

    if (! -f $fullpath ) {
	$coretestfail = 1;
    }

    $fullpath=getAbsPath($self->{FILENAME});
    if (! -f $fullpath ) {
	$coretestfail = 1;
    }

    $fullpath="/test_suite_bogus_path/".$basefilename;

    my @temp = ();
    $HeaderDoc::perHeaderRanges{$self->{FILENAME}} = \@temp;

    my ($cpp_hash_ref, $cpp_arg_hash_ref) = getAndClearCPPHash();

    my @commentLines = split(/\n/, $self->{COMMENT});
    map(s/$/\n/gm, @commentLines);

    # Set up some stuff for the line array code to filter the comment.
    $HeaderDoc::nodec = 0;

    HeaderDoc::APIOwner->apiUIDPrefix("test_ref");

    $HeaderDoc::lang = $self->{LANG};
    $HeaderDoc::sublang = $self->{SUBLANG};
    my $apiOwner = HeaderDoc::Header->new();
    $apiOwner->apiOwner($apiOwner);
    my $headerObject = $apiOwner;
    $HeaderDoc::headerObject = $headerObject;
    $HeaderDoc::currentClass = undef;
    $apiOwner->filename($basefilename);
    $apiOwner->fullpath($fullpath);
    $apiOwner->name($self->{NAME});

    %HeaderDoc::ignorePrefixes = ();
    %HeaderDoc::perHeaderIgnorePrefixes = ();
    %HeaderDoc::perHeaderIgnoreFuncMacros = ();

    HeaderDoc::Utilities::loadhashes($alldecs);

    print STDERR "LANG: $self->{LANG} SUBLANG: $self->{SUBLANG}\n" if ($testDebug);

    print STDERR "Filtering comment\n" if ($testDebug);

    # Filter the comment.
    my @commentLineArray = &getLineArrays(\@commentLines, $self->{LANG}, $self->{SUBLANG});
    my $comment = "";
    foreach my $arr (@commentLineArray) {
	foreach my $item (@$arr) {
	    my $localDebug = 0;
	    if (($self->{LANG} ne "pascal" && (
                             ($self->{LANG} ne "perl" && $self->{LANG} ne "shell" && $item =~ /^\s*\/\*\!/o) ||
                             (($self->{LANG} eq "perl" || $self->{LANG} eq "shell") && ($item =~ /^\s*\#\s*\/\*\!/o)) ||
                             (($self->{LANG} eq "java" || $HeaderDoc::parse_javadoc) && ($item =~ /^\s*\/\*\*[^\*]/o)))) ||
                            (($self->{LANG} eq "pascal") && ($item =~ s/^\s*\{!/\/\*!/so))) {
		if (($self->{LANG} ne "pascal" && ($item =~ /\s*\*\//o)) ||
                                    ($self->{LANG} eq "pascal" && ($item =~ s/\s*\}/\*\//so))) { # closing comment marker on same line
                                       print STDERR "PASCAL\n" if ($localDebug);
			if ($self->{LANG} eq "perl" || $self->{LANG} eq "shell") {
                                                $item =~ s/^\s*\#//s;
                                                $item =~ s/\n( |\t)*\#/\n/sg;
                                                # print STDERR "NEWLINE: $item\n";
			}
		} else {
			$item =~ s/^ \*//o;
			if ($self->{LANG} eq "perl" || $self->{LANG} eq "shell") {
						    print STDERR "SHELL OR PERL\n" if ($localDebug);
                                                    $item =~ s/^\s*\#//o;
print STDERR "ITEM NOW $item\n" if ($localDebug);
                        }
		}
	    }
	    $comment .= $item; #$commentLineArray[0][0];
	}
    }


# print("COMMENT: $comment\n");


    if ($comment =~ /^\s*\/\*\*/s) {
	$comment =~ s/\s*\/\*\*/\/\*\!/s;
    }
    if ($comment =~ /^\s*\/\*!/s) {
	$comment =~ s/\*\/\s*$//s;
    }
    $comment =~ s/^\s*//s;
    # print STDERR "COM: $comment\n";

    # Try the top level comment parser code and see what we get.
    my ($inHeader, $inClass, $inInterface, $inCPPHeader, $inOCCHeader, $inPerlScript, $inShellScript, $inPHPScript, $inJavaSource, $inFunctionGroup, $inGroup, $inFunction, $inPDefine, $inTypedef, $inUnion, $inStruct, $inConstant, $inVar, $inEnum, $inMethod, $inAvailabilityMacro, $inUnknown, $classType, $line, $inputCounter, $blockOffset, $junkpath, $linenumdebug, $localDebug);
    if ($self->{TYPE} eq "parser") {
	print STDERR "Running top-level comment parser (case a)\n" if ($testDebug);

	($inHeader, $inClass, $inInterface, $inCPPHeader, $inOCCHeader, $inPerlScript, $inShellScript, $inPHPScript, $inJavaSource, $inFunctionGroup, $inGroup, $inFunction, $inPDefine, $inTypedef, $inUnion, $inStruct, $inConstant, $inVar, $inEnum, $inMethod, $inAvailabilityMacro, $inUnknown, $classType, $line, $inputCounter, $blockOffset, $junkpath, $linenumdebug, $localDebug) = processTopLevel(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "unknown", $comment, 0, 0, $fullpath, 0, 0);
    } else {
	print STDERR "Running top-level comment parser (case b)\n" if ($testDebug);

	($inHeader, $inClass, $inInterface, $inCPPHeader, $inOCCHeader, $inPerlScript, $inShellScript, $inPHPScript, $inJavaSource, $inFunctionGroup, $inGroup, $inFunction, $inPDefine, $inTypedef, $inUnion, $inStruct, $inConstant, $inVar, $inEnum, $inMethod, $inAvailabilityMacro, $inUnknown, $classType, $line, $inputCounter, $blockOffset, $junkpath, $linenumdebug, $localDebug) = processTopLevel(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "unknown", "/*! CPP only */", 0, 0, $fullpath, 0, 0);
    }

    if ($self->{TYPE} eq "parser") {
	$results .= "-=: TOP LEVEL COMMENT PARSE VALUES :=-\n";
	$results .= "inHeader: $inHeader\n";
	$results .= "inClass: $inClass\n";
	$results .= "inInterface: $inInterface\n";
	$results .= "inCPPHeader: $inCPPHeader\n";
	$results .= "inOCCHeader: $inOCCHeader\n";
	$results .= "inPerlScript: $inPerlScript\n";
	$results .= "inShellScript: $inShellScript\n";
	$results .= "inPHPScript: $inPHPScript\n";
	$results .= "inJavaSource: $inJavaSource\n";
	$results .= "inFunctionGroup: $inFunctionGroup\n";
	$results .= "inGroup: $inGroup\n";
	$results .= "inFunction: $inFunction\n";
	$results .= "inPDefine: $inPDefine\n";
	$results .= "inTypedef: $inTypedef\n";
	$results .= "inUnion: $inUnion\n";
	$results .= "inStruct: $inStruct\n";
	$results .= "inConstant: $inConstant\n";
	$results .= "inVar: $inVar\n";
	$results .= "inEnum: $inEnum\n";
	$results .= "inMethod: $inMethod\n";
	$results .= "inAvailabilityMacro: $inAvailabilityMacro\n";
	$results .= "inUnknown: $inUnknown\n";
	$results .= "classType: $classType\n";
	$results .= "inputCounter: $inputCounter\n";
	$results .= "blockOffset: $blockOffset\n";
	$results .= "fullpath: $junkpath\n";
    }

    if ($inGroup || $inFunctionGroup) {
	print STDERR "Processing group info.\n" if ($testDebug);

	my $debugging = 0;
	my $fieldref = stringToFields($comment, $fullpath, $inputCounter);
	my @fields = @{$fieldref};
	my $line = $comment;

	print STDERR "inGroup\n" if ($debugging);
	my $rawname = $line;
	my $type = "";
	if ($inGroup) {
		$rawname =~ s/.*\/\*!\s*\@(group|name)\s+//sio;
		$type = $1;
	} else {
		if (!($rawname =~ s/.*\/\*!\s+\@(functiongroup)\s+//io)) {
			$rawname =~ s/.*\/\*!\s+\@(methodgroup)\s+//io;
			print STDERR "inMethodGroup\n" if ($debugging);
		}
		$type = $1;
	}
	$rawname =~ s/\s*\*\/.*//o;
	my ($name, $desc, $is_nameline_disc) = getAPINameAndDisc($rawname);
	$name =~ s/^\s+//smgo;
	$name =~ s/\s+$//smgo;

	if ($is_nameline_disc) { $name .= " ".$desc; $desc = ""; }

	print STDERR "group name is $name\n" if ($debugging);
	my $group = $apiOwner->addGroup($name); #(, $desc);
	$group->processComment(\@fields);
	$HeaderDoc::globalGroup = $name;
	# $inputCounter--;
	# print STDERR "DECREMENTED INPUTCOUNTER [M6]\n" if ($HeaderDoc::inputCounterDebug);

	$results .= "-=: GROUP INFO :=-\n";
	$results .= "request type => ".$type."\n";
	$results .= "name => ".$group->name()."\n";
	$results .= "Description => ".$group->discussion."\n";
    } else {
	print STDERR "Getting line arrays.\n" if ($testDebug);

	my @perHeaderClassObjects = ();
	my @perHeaderCategoryObjects = ();
	my @fields = ();
	my $hangDebug = my $parmDebug = my $blockDebug = 0;
	my $allow_multi = 1;
	my $subparse = 0;
	my $subparseTree = undef;
	my $cppAccessControlState = "protected:"; # the default in C++
	my $objcAccessControlState = "private:"; # the default in Objective C
	my $functionGroup = "default_function_group";


	my @codeLines = split(/\n/, $self->{CODE});
	map(s/$/\n/gm, @codeLines);
	my @codeLineArray = &getLineArrays(\@codeLines, $self->{LANG}, $self->{SUBLANG});
	my $arrayRef = @codeLineArray[0];
	my @inputLines = @$arrayRef;

	my @cppLines = split(/\n/, $self->{CPPCODE});
	map(s/$/\n/gm, @cppLines);
	my @cppLineArray = &getLineArrays(\@cppLines, $self->{LANG}, $self->{SUBLANG});
	$arrayRef = @cppLineArray[0];
	my @cppInputLines = @$arrayRef;

	my $preAtPart = "";
	my $xml_output = 0;

	# Old code.
	# my @inputLines = split(/\n/, $self->{CODE});
	# map(s/$/\n/gm, @inputLines);
	# my @cppInputLines = split(/\n/, $self->{CPPCODE});
	# map(s/$/\n/gm, @cppInputLines);

	my ($case_sensitive, $keywordhashref) = $headerObject->keywords();

	my $inputCounter = 0;

	# print STDERR "TYPE: $self->{TYPE}\n";
	$HeaderDoc::enable_cpp = 1;
	if ($self->{TYPE} eq "cpp") {
		print STDERR "Running blockParse (CPP mode).\n" if ($testDebug);


		$results .= "-=: CPP MACROS PARSED :=-\n";
		while ($inputCounter <= $#cppInputLines) {
			my ($newcount, $declaration, $typelist, $namelist, $posstypes, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $blockOffset, $conformsToList, $functionContents, $parserState, $nameObjectsRef) = &blockParse($fullpath, $blockOffset, \@cppInputLines, $inputCounter, 0, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);
			if ($declaration !~ /\S/) { last; }
			$results .= "PARSED: $namelist\n";
			$inputCounter = $newcount;
		}
		$results .= "\n";
	}

	print STDERR "Running blockParse.\n" if ($testDebug);

	my $blockOffset = $inputCounter;
	$inputCounter = 0;
	my ($newcount, $declaration, $typelist, $namelist, $posstypes, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $blockOffset, $conformsToList, $functionContents, $parserState, $nameObjectsRef, $extendsClass, $implementsClass) = &blockParse($fullpath, $blockOffset, \@inputLines, $inputCounter, 0, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);

	$results .= "-=: BLOCKPARSE PARSER STATE KEYS :=-\n";
	my @pskeys = sort keys %{$parserState};
	foreach my $key (@pskeys) {
		if ($key !~ /(pplStack|hollow|lastTreeNode|freezeStack|parsedParamList)/) {
			$results .= "\$parserState->{$key} => ".$parserState->{$key}."\n";
		} else {
			my $temp = $parserState->{$key};
			$temp =~ s/0x[0-9a-f]+/OBJID/sg;
			$results .= "\$parserState->{$key} => ".$temp."\n";
		}
    	}
	$results .= "-=: BLOCKPARSE RETURN VALUES :=-\n";
	$results .= "newcount: $newcount\n";
	$results .= "typelist: $typelist\n";
	$results .= "namelist: $namelist\n";
	$results .= "posstypes: $posstypes\n";
	$results .= "value: $value\n";
	$results .= "returntype: $returntype\n";
	$results .= "pridec: $pridec\n";
	$results .= "simpleTDcontents: $simpleTDcontents\n";
	$results .= "bpavail: $bpavail\n";
	$results .= "blockOffset: $blockOffset\n";
	$results .= "conformsToList: $conformsToList\n";
	$results .= "functionContents: $functionContents\n";
	$results .= "extendsClass: $extendsClass\n";
	$results .= "implementsClass: $implementsClass\n";

	# Legacy declaration.
	# $results .= "declaration: $declaration\n";

	# Parsed parameters list (array)
	# $results .= "pplref: $pplref\n";
	my @parsedParamList = @{$pplref};

	$results .= "-=: LIST OF PARSED PARAMETERS :=-\n";
	my $count = 0;
	foreach my $ppl (@parsedParamList) {
		$results .= "Parsed Param $count => $ppl\n";
		$count++;
	}

	# Parse tree (data structure)
	# parseTree: $parseTree
	$results .= "-=: DUMP OF PARSE TREE :=-\n";
	$results .= $parseTree->test_output_dump();

	$results .= "-=: COMPUTED VALUE :=-\n";
	my ($success, $value) = $parseTree->getPTvalue();
	$results .= "SUCCESS: $success\n";
	$results .= "VALUE: $value\n";

	$results .= "-=: CPP CHANGES :=-\n";
	$results .= $self->cppTests();

# print STDERR "RESULTS: $results\n";

	if ($self->{TYPE} eq "parser") {
		# Only do this for parser tests.

		print STDERR "Running stringToFields.\n" if ($testDebug);

		my $fieldref = stringToFields($comment, $fullpath, $inputCounter);
		@fields = @{$fieldref};

		if ($inHeader) {
			my $rootOutputDir = "/tmp/bogus";
			my $debugging = 0;
			my $reprocess_input = 0;

			print STDERR "Running processHeaderComment.\n" if ($testDebug);

			processHeaderComment($apiOwner, $rootOutputDir, \@fields, $self->{LANG}, $debugging, $reprocess_input);
		}

		print STDERR "Running blockParseOutside.\n" if ($testDebug);

		# Reset any language changes that may have occurred.
		$HeaderDoc::sublang = $self->{SUBLANG};

		print STDERR "my (\$newInputCounter, \$cppAccessControlState, \$classType, \$classref, \$catref, \$blockOffset, \$numcurlybraces, \$foundMatch) =
		    blockParseOutside($apiOwner, $inFunction, $inUnknown,
		    $inTypedef, $inStruct, $inEnum, $inUnion,
		    $inConstant, $inVar, $inMethod, $inPDefine,
		    $inClass, $inInterface, $blockOffset, \@perHeaderCategoryObjects,
		    \@perHeaderClassObjects, $classType, $cppAccessControlState,
		    \@fields, $fullpath, $functionGroup,
		    $headerObject, $inputCounter, \@inputLines,
		    $self->{LANG}, $#inputLines, $preAtPart, $xml_output, $localDebug,
		    $hangDebug, $parmDebug, $blockDebug, $subparse,
		    $subparseTree, $HeaderDoc::nodec, $allow_multi);\n" if ($testDebug);
		print STDERR "FIELDS:\n" if ($testDebug);
		print "FIRSTLINE: ".$inputLines[$inputCounter]."\n" if ($testDebug);
		printArray(@fields) if ($testDebug);

		my ($newInputCounter, $cppAccessControlState, $classType, $classref, $catref, $blockOffset, $numcurlybraces, $foundMatch) =
		    blockParseOutside($apiOwner, $inFunction, $inUnknown,
		    $inTypedef, $inStruct, $inEnum, $inUnion,
		    $inConstant, $inVar, $inMethod, $inPDefine,
		    $inClass, $inInterface, $blockOffset, \@perHeaderCategoryObjects,
		    \@perHeaderClassObjects, $classType, $cppAccessControlState,
		    \@fields, $fullpath, $functionGroup,
		    $headerObject, $inputCounter, \@inputLines,
		    $self->{LANG}, $#inputLines, $preAtPart, $xml_output, $localDebug,
		    $hangDebug, $parmDebug, $blockDebug, $subparse,
		    $subparseTree, $HeaderDoc::nodec, $allow_multi);

		$results .= "-=: FOUND MATCH :=-\n";
		$results .= $foundMatch."\n";
		$results .= "-=: NAMED OBJECTS :=-\n";
		my ($newresults, @parseTrees) = $self->dumpObjNames($headerObject);
		$results .= $newresults;

		print STDERR "Running parse tree info dumps.\n" if ($testDebug);

		$results .= "-=: NAMED OBJECT PARSE TREES :=-\n";
		foreach my $tree (@parseTrees) {
			# print STDERR "DUMPING PARSE TREE $tree\n";
			my $owner = $tree->apiOwner();
			my $name = $owner->name();
			if ($owner->can("rawname")) {
				if (!$owner->{DISCUSSION} || !$owner->{NAMELINE_DISCUSSION}) {
					$name = $owner->rawname();
				}
			}
			my $class = ref($owner) || $owner;

			$results .= "OBJECT: $name ($class)\n";
			$results .= $tree->test_output_dump();
			$results .= "END OF OBJECT\n\n\n";
		}
		$results .= "\n";

		print STDERR "Running parse tree HTML dumps.\n" if ($testDebug);

		$results .= "-=: HTML OUTPUT OF PARSE TREES :=-\n";
		foreach my $tree (@parseTrees) {
			# print STDERR "DUMPING PARSE TREE $tree\n";
			my $owner = $tree->apiOwner();
			my $name = $owner->name();
			if ($owner->can("rawname")) {
				if (!$owner->{DISCUSSION} || !$owner->{NAMELINE_DISCUSSION}) {
					$name = $owner->rawname();
				}
			}
			my $class = ref($owner) || $owner;

			$results .= "OBJECT: $name ($class)\n";

			my $temp = $tree->htmlTree($owner->preserve_spaces(), $owner->hideContents());
			my @parts = split(/\n/, $temp);
    			foreach my $part (@parts) {
				$results .= "\t".$part."\n";
			}
			$results .= "END OF OBJECT\n\n\n";
		}
		$results .= "\n";
    	}
    }

    print STDERR "Done.\n" if ($testDebug);
    # my @lines = split(

	# print STDERR "TEST RESULTS: $results\n";

    $HeaderDoc::test_mode = 0;
    $HeaderDoc::ignore_apiuid_errors = $prevignore;

    if ($alldecs) {
	$self->{RESULT_ALLDECS} = $results;
    } else {
	$self->{RESULT} = $results;
    }
    HeaderDoc::Utilities::savehashes($alldecs);

    return $coretestfail;
}

sub readFromFile {
    my $self = shift;
    my $filename = shift;

    open(READFILE, "<$filename") or die("Could not read file \"$filename\"\n");
    my $temprecsep = $/;
    $/ = undef;
    my $string = <READFILE>;
    $/ = $temprecsep;
    close(READFILE);

    my ($obj, $rest) = thaw($string);

	# print STDERR "STRING: $string\n";
	# print STDERR "OBJ: $obj\n";
	# print STDERR "REST: $rest\n";
    my @objkeys = sort keys %{$obj};
    foreach my $key (@objkeys) {
	$self->{$key} = $obj->{$key};
    }
    # $self->{FILENAME} = $self->{NAME};
    # $self->{FILENAME} =~ s/[^a-zA-Z0-9_.,-]/_/sg;

    # Do the right thing if somebody renames a file.
    $self->{FILENAME} = $filename;

    if ($self->{TYPE} eq "") { $self->{TYPE} = "parser"; }
    if ($self->{SUBLANG} eq "") { $self->{SUBLANG} = $self->{LANG}; }
}

sub writeToFile {
    my $self = shift;
    my $filename = shift;

    my $string = freeze($self);
    open(WRITEFILE, ">$filename") or die("Could not write file \"$filename\"\n");

    print WRITEFILE $string;
    close(WRITEFILE);
}

sub isFramework
{
    return 0;
}

sub dbprint_expanded
{
    print STDERR "NOT IMPLEMENTED.\n";
}

sub dbprint
{
    my $self = shift;
    my $expanded = shift;
    my @keys = sort keys %{$self};

    print STDERR "Dumping object $self...\n";
    foreach my $key (@keys) {
        if ($expanded) {
                print STDERR "$key => ".dbprint_expanded($self->{$key})."\n";
        } else {
                print STDERR "$key => ".$self->{$key}."\n";
        }
    }
    print STDERR "End dump of object $self.\n";
}

sub starbox
{
    my $self = shift;
    my $string = shift;

    my $maxlen = 60;
    my @lines = split(/\n/s, $string);

    foreach my $line (@lines) {
	if (length($line) > $maxlen) {
		$maxlen = length($line);
	}
    }


    my $starline = "+-" . ("-" x $maxlen) . "-+";
    my $count = 0;

    $maxlen += 4;

    print "    ".$starline."\n";
    foreach my $line (@lines) {
	$line = "| $line";
	$line .= " " x (($maxlen - length($line)) - 1);
	$line .= "|";
	print "    $line\n";
    }
    print "    ".$starline."\n\n\n";
}

# Used to compare actual results with expected.
sub showresults
{
    my $self = shift;
    my $expanded = shift;

    $self->showresults_sub($expanded, 0);
    if ($self->supportsAllDecs()) {
	$self->showresults_sub($expanded, 1);
    }

}

sub showresults_sub
{
    my $self = shift;
    my $expanded = shift;
    my $alldecs = shift;

    if ($self->{FAILMSG} =~ /\S/) {
	print "\n";
	$self->starbox("FAILURE NOTES:\n\n".$self->{FAILMSG}."\n");
    }

    my @expected_part_arr = ();
    my @got_part_arr = ();
    if ($alldecs) {
	print STDERR "\nALLDECS RESULT:\n\n";
	@expected_part_arr = split(/((?:^|\n)-=:(?:.+?):=-)/s, $self->{EXPECTED_RESULT_ALLDECS});
	@got_part_arr = split(/((?:^|\n)-=:(?:.+?):=-)/s, $self->{RESULT_ALLDECS});
    } else {
	@expected_part_arr = split(/((?:^|\n)-=:(?:.+?):=-)/s, $self->{EXPECTED_RESULT});
	@got_part_arr = split(/((?:^|\n)-=:(?:.+?):=-)/s, $self->{RESULT});
    }

    my %expected_parts = %{$self->convertToHash(\@expected_part_arr)};
    my %got_parts = %{$self->convertToHash(\@got_part_arr)};

    foreach my $key (sort keys %expected_parts) {
	if ($expected_parts{$key} ne $got_parts{$key}) {
		print STDERR "\t$key does not match\n";
		if ($key eq "LIST OF PARSED PARAMETERS" ||
		    $key eq "TOP LEVEL COMMENT PARSE VALUES") {
			print STDERR $self->singlePrint($expected_parts{$key}, $got_parts{$key}, 0);
		} elsif ($key eq "BLOCKPARSE PARSER STATE KEYS" && $expanded != 1) {
			print STDERR $self->objCmp($expected_parts{$key}, $got_parts{$key}, $expanded);
		} elsif ($key eq "NAMED OBJECTS" && (!$expanded)) {
			my $part_a = $expected_parts{$key};
			my $part_b = $got_parts{$key};

			$part_a =~ s/\/\/test_ref.*?(\s|"|')//sg;
			$part_b =~ s/\/\/test_ref.*?(\s|"|')//sg;

			if ($part_a eq $part_b) {
				my @refs_a = split(/\/\/test_ref\//, $expected_parts{$key});
				my @refs_b = split(/\/\/test_ref\//, $got_parts{$key});

				my $pos = 1; # position 0 is before the first occurrence.
				while ($pos <= $#refs_a) {
					my $ref_a = $refs_a[$pos];
					my $ref_b = $refs_b[$pos];
					$ref_a =~ s/(\s|"|').*$//s;
					$ref_b =~ s/(\s|"|').*$//s;
					if ($ref_a ne $ref_b) {
						print STDERR "\t\tUID //test_ref/".$ref_a."\n\t\tNOW //test_ref/".$ref_b."\n\n";
					}
					$pos++;
				}
			} else {
				print STDERR $self->multiPrint($expected_parts{$key}, $got_parts{$key});
			}
		} elsif (($key eq "NAMED OBJECTS" || $key eq "NAMED OBJECT PARSE TREES" || $key eq "DUMP OF PARSE TREE" || $key eq "CPP CHANGES" || $key eq "BLOCKPARSE RETURN VALUES") && $expanded == -1) {
				open(WRITEFILE, ">/tmp/headerdoc-diff-expected") or die("Could not write file \"/tmp/headerdoc-diff-expected\"\n");
				print WRITEFILE $expected_parts{$key};
				close(WRITEFILE);
				open(WRITEFILE, ">/tmp/headerdoc-diff-got") or die("Could not write file \"/tmp/headerdoc-diff-got\"\n");
				print WRITEFILE $got_parts{$key};
				close(WRITEFILE);

				system("/usr/bin/diff -u /tmp/headerdoc-diff-expected /tmp/headerdoc-diff-got");

				unlink("/tmp/headerdoc-diff-expected");
				unlink("/tmp/headerdoc-diff-got");
		} else {
			print STDERR $self->multiPrint($expected_parts{$key}, $got_parts{$key});
		}
	} else {
		print STDERR "\t$key matches\n";
	}
    }
    foreach my $key (sort keys %got_parts) {
	# print STDERR "KEY $key\n";
	if (!defined($expected_parts{$key})) {
		print STDERR "\tUnexpected part $key\n";
		print STDERR $self->multiPrint($expected_parts{$key}, $got_parts{$key});
	}
    }

}

sub multiPrint
{
    my $self = shift;
    my $string1 = shift;
    my $string2 = shift;

    my @parts1 = split(/\n/, $string1);
    my @parts2 = split(/\n/, $string2);

    print STDERR "\t\tEXPECTED:\n";
    foreach my $line (@parts1) {
	print STDERR "\t\t\t$line\n";
    }
    print STDERR "\t\tGOT:\n";
    foreach my $line (@parts2) {
	print STDERR "\t\t\t$line\n";
    }

}

sub objCmp
{
    my $self = shift;
    my $string1 = shift;
    my $string2 = shift;
    my $all = shift;

    my $localDebug = 0;

    my @parts1 = split(/\n/, $string1);
    my @parts2 = split(/\n/, $string2);

    my %expected_keys = ();
    my %got_keys = ();
    my %keysall = ();

    my $retstring = "";

    my $continue = "";
    foreach my $part (@parts1) {
	if ($continue && $part !~ /^\s*\$parserState->.*?=>/) {
		print STDERR "APPENDING TO \"$continue\"\n" if ($localDebug);
		$expected_keys{$continue} .= "\n".$part;
		next;
	}
	my ($key, $value) = split(/=>/, $part, 2);
	$key =~ s/^.*{//s;
	$key =~ s/}.*$//s;

	$continue = $key;
	$expected_keys{$key} = $value;
	$keysall{$key} = 1;
	print STDERR "LINE: $part\n" if ($localDebug);
	print STDERR "KEY: $key\n" if ($localDebug);
    }

    $continue = "";
    foreach my $part (@parts2) {
	if ($continue && $part !~ /^\s*\$parserState->.*?=>/) {
		$got_keys{$continue} .= "\n".$part;
		print STDERR "APPENDING TO \"$continue\"\n" if ($localDebug);
		next;
	}
	my ($key, $value) = split(/=>/, $part, 2);
	$key =~ s/^.*{//s;
	$key =~ s/}.*$//s;

	$continue = $key;
	$got_keys{$key} = $value;
	if ($keysall{$key} == 1) {
		$keysall{$key} = 2;
	} else {
		$keysall{$key} = 3;
	}
	print STDERR "LINE: $part\n" if ($localDebug);
	print STDERR "KEY: $key\n" if ($localDebug);
    }

    my $found_difference = 0;
    foreach my $key (sort keys %keysall) {
	my $found = $keysall{$key};
	if ($found == 1) {
		$retstring .= "\t\tKEY $key is missing.  Value: ".$expected_keys{$key}."\n";
		$found_difference = 1;
	} elsif ($found == 3) {
		# print STDERR "X\t\tKEY $key is new.  Value was: ".$got_keys{$key}."\n";
		$retstring .= "\t\tKEY $key is new.  Value was: ".$got_keys{$key}."\n";
		$found_difference = 1;
	}
    }
    foreach my $key (sort keys %keysall) {
	if (($all == 1) || ($expected_keys{$key} ne $got_keys{$key})) {
		$retstring .= "\t\tEXPECTED: \$parserState->{$key} =>".$expected_keys{$key}."\n";
		$retstring .= "\t\tGOT:      \$parserState->{$key} =>".$got_keys{$key}."\n\n";
		$found_difference = 1;
	}
    }

    if (!$found_difference) {
	$retstring .= "\t\tNo changes found.\n";
	# $retstring .= "EXPECTED:\n$string1\n";
	# $retstring .= "GOT:\n$string2\n";
    }
    return $retstring;
}

sub singlePrint
{
    my $self = shift;
    my $string1 = shift;
    my $string2 = shift;
    my $sort = shift;

    my @parts1 = split(/\n/, $string1);
    my @parts2 = split(/\n/, $string2);

    my $pos = 0;
    my $count = scalar(@parts1);
    if ($count < scalar(@parts2)) {
	$count = scalar(@parts2);
    }

    while ($pos < $count) {
	my $part1 = $parts1[$pos];
	my $part2 = $parts2[$pos];

	if ($part1 ne $part2) {
		print STDERR "\t\tEXPECTED: $part1\n";
		print STDERR "\t\tGOT:      $part2\n\n";
	}

	$pos++;
    }

}

sub convertToHash
{
    my $self = shift;
    my $arrayRef = shift;
    my @arr = @{$arrayRef};
    my %retarr = ();

    my $pos = 0;
    my $key = "";
    foreach my $part (@arr) {
	# print STDERR "PART: $part\n";
	if ($pos) {
		$key = $part;
		$key =~ s/^\s*-=:\s*//s;
		$key =~ s/\s*:=-\s*$//s;
		$pos = 0;
		# print STDERR "KEY: $key\n";
	} else {
		$part =~ s/^\n*//s;
		$part =~ s/\n*$//s;
		if ($key) { $retarr{$key} = $part; }
		# print STDERR "SET $key to $part\n";
		$pos = 1;
	}
    }
    return \%retarr;
}

sub cppTests
{
    my $self = shift;
    my $retstring = "";

    my ($cpp_hash_ref, $cpp_arg_hash_ref) = getAndClearCPPHash();
    my %cpp_hash = %{$cpp_hash_ref};
    my %cpp_arg_hash = %{$cpp_arg_hash_ref};

    foreach my $key (sort keys %cpp_hash) {
	$retstring .= "\$CPP_HASH{$key} => ".$cpp_hash{$key}."\n";
    }
    foreach my $key (sort keys %cpp_arg_hash) {
	$retstring .= "\$CPP_ARG_HASH{$key} => ".$cpp_arg_hash{$key}."\n";
    }

    if ($retstring eq "") {
	$retstring = "NO CPP CHANGES\n";
    }

    return $retstring;
}

sub dumpObjNames
{
    my $self = shift;
    my $obj = shift;
    my $nest = 0;
    if (@_) {
	$nest = shift;
    }

    my @parseTrees = ();

    my $indent = '    ' x $nest;
    my $retstring = "";

    # print STDERR "OBJ: $obj\n";
    my $name = $obj->name();
    if ($obj->can("rawname")) {
	if (!$obj->{DISCUSSION} || !$obj->{NAMELINE_DISCUSSION}) {
		$name = $obj->rawname();
	}
    }

    my $treecount = 0;
    my $parseTree_ref = $obj->parseTree();
    my $mainTree = undef;
    if ($parseTree_ref) {
	$mainTree = ${$parseTree_ref};
	bless($mainTree, "HeaderDoc::ParseTree");
	push(@parseTrees, $mainTree);
	$treecount++;
    }

    my $ptlistref = $obj->parseTreeList();
    if ($ptlistref) {
	my @tree_refs = @{$ptlistref};
	foreach my $tree_ref (@tree_refs) {
		my $extratree = ${$tree_ref};
		bless($extratree,  "HeaderDoc::ParseTree");
		if ($extratree != $mainTree) {
			push(@parseTrees, $extratree);
			$treecount++;
		}
	}
    }

    $retstring .= $indent."TREE COUNT: $treecount\n";

    if ($obj->can("indexgroup")) {
	$retstring .= $indent."INDEX GROUP: ".$obj->indexgroup()."\n";
    }
    if ($obj->can("isProperty")) {
	$retstring .= $indent."IS PROPERTY: ".$obj->isProperty()."\n";
    }
    if ($obj->can("isBlock")) {
	$retstring .= $indent."IS BLOCK: ".$obj->isBlock()."\n";
    }
    if ($obj->can("isAvailabilityMacro")) {
	$retstring .= $indent."IS AVAILABILITY MACRO: ".$obj->isAvailabilityMacro()."\n";
    }
    if ($obj->can("parseOnly")) {
	$retstring .= $indent."PARSE ONLY: ".$obj->parseOnly()."\n";
    }
    $retstring .= $indent."OBJECT TYPE: ".$obj->class()."\n";
    $retstring .= $indent."NAME: ".$name."\n";
    # $retstring .= $indent."RAWNAME: ".$obj->rawname()."\n";
    my $class = ref($obj) || $obj;
    if ($class =~ /HeaderDoc::MinorAPIElement/) {
	$retstring .= $indent."TYPE: ".$obj->type()."\n";
    }
    # $retstring .= $indent."APIO: ".$obj->apiOwner()."\n";
    $obj->apirefSetup();
    $retstring .= $indent."APIUID: ".$obj->apiuid()."\n";
    $retstring .= $indent."ABSTRACT: \"".$obj->abstract()."\"\n";
    $retstring .= $indent."DISCUSSION: \"".$obj->discussion()."\"\n";
# print STDERR "RAWDISC:   ".$obj->{DISCUSSION}."\n";
# print STDERR "RAWNLDISC: ".$obj->{NAMELINE_DISCUSSION}."\n";
# print STDERR "DISC:      ".$obj->discussion()."\n";

    $retstring .= $indent."UPDATED: \"".$obj->{UPDATED}."\"\n";
    $retstring .= $indent."COPYRIGHT: \"".$obj->{COPYRIGHT}."\"\n";
    $retstring .= $indent."HTMLMETA: \"".$obj->{HTMLMETA}."\"\n";
    $retstring .= $indent."PRIVATEDECLARATION: \"".$obj->{PRIVATEDECLARATION}."\"\n";
    $retstring .= $indent."GROUP: \"".$obj->{GROUP}."\"\n";
    $retstring .= $indent."INDEXGROUP: \"".$obj->{INDEXGROUP}."\"\n";
    $retstring .= $indent."THROWS: \"".$obj->{THROWS}."\"\n";
    $retstring .= $indent."XMLTHROWS: \"".$obj->{XMLTHROWS}."\"\n";
    $retstring .= $indent."UPDATED: \"".$obj->{UPDATED}."\"\n";
    $retstring .= $indent."LINKAGESTATE: \"".$obj->{LINKAGESTATE}."\"\n";
    $retstring .= $indent."ACCESSCONTROL: \"".$obj->{ACCESSCONTROL}."\"\n";
    $retstring .= $indent."AVAILABILITY: \"".$obj->{AVAILABILITY}."\"\n";
    $retstring .= $indent."LINKUID: \"".$obj->{LINKUID}."\"\n";
    $retstring .= $indent."ORIGCLASS: \"".$obj->{ORIGCLASS}."\"\n";
    $retstring .= $indent."ISDEFINE: \"".$obj->{ISDEFINE}."\"\n";
    $retstring .= $indent."ISTEMPLATE: \"".$obj->{ISTEMPLATE}."\"\n";
    $retstring .= $indent."VALUE: \"".$obj->{VALUE}."\"\n";
    $retstring .= $indent."RETURNTYPE: \"".$obj->{RETURNTYPE}."\"\n";
    $retstring .= $indent."LINENUM: \"".$obj->{LINENUM}."\"\n";
    $retstring .= $indent."CLASS: \"".$obj->{CLASS}."\"\n";
    $retstring .= $indent."MASTERENUM: \"".$obj->{MASTERENUM}."\"\n";
    $retstring .= $indent."APIREFSETUPDONE: \"".$obj->{APIREFSETUPDONE}."\"\n";
    $retstring .= $indent."TPCDONE: \"".$obj->{TPCDONE}."\"\n";
    $retstring .= $indent."NOREGISTERUID: \"".$obj->{NOREGISTERUID}."\"\n";
    $retstring .= $indent."SUPPRESSCHILDREN: \"".$obj->{SUPPRESSCHILDREN}."\"\n";
    $retstring .= $indent."NAMELINE_DISCUSSION: \"".$obj->{NAMELINE_DISCUSSION}."\"\n";
    $retstring .= $indent."HIDEDOC: \"".$obj->{HIDEDOC}."\"\n";
    $retstring .= $indent."HIDESINGLETONS: \"".$obj->{HIDESINGLETONS}."\"\n";
    $retstring .= $indent."HIDECONTENTS: \"".$obj->{HIDECONTENTS}."\"\n";

    my $temp = $obj->{MAINOBJECT};
    $temp =~ s/0x[0-9a-f]+/OBJID/sg;
    $retstring .= $indent."MAINOBJECT: \"$temp\"\n";

    my $composite = 1;
    my $list_attributes = $obj->getAttributeLists($composite);
    my $short_attributes = $obj->getAttributes(0);
    my $long_attributes = $obj->getAttributes(1);

    $retstring .= $indent."LIST ATTRIBUTES: ".$list_attributes."\n";
    $retstring .= $indent."SHORT ATTRIBUTES: ".$short_attributes."\n";
    $retstring .= $indent."LONG ATTRIBUTES: ".$long_attributes."\n";

    if ($obj->can("userDictArray")) {
	my @userDictArray = $obj->userDictArray();
	foreach my $hashRef (@userDictArray) {
		$retstring .= $indent."USER DICTIONARY:\n";
		while (my ($param, $disc) = each %{$hashRef}) {
			$retstring .= $indent."    $param\t=>\t$disc\n";
		}
		$retstring .= $indent."END USER DICTIONARY\n";
	}
    }

    if ($obj->isAPIOwner()) {
	my @functions = $obj->functions();
	my @methods = $obj->methods();
	my @constants = $obj->constants();
	my @typedefs = $obj->typedefs();
	my @structs = $obj->structs();
	my @vars = $obj->vars();
	my @enums = $obj->enums();
	my @pDefines = $obj->pDefines();
	my @classes = $obj->classes();
	my @categories = $obj->categories();
	my @protocols = $obj->protocols();
	my @properties = $obj->props();

	if (@functions) {
		foreach my $obj (@functions) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
	if (@methods) {
		foreach my $obj (@methods) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
	if (@constants) {
		foreach my $obj (@constants) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
	if (@typedefs) {
		foreach my $obj (@typedefs) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
	if (@structs) {
		foreach my $obj (@structs) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
	if (@vars) {
		foreach my $obj (@vars) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
	if (@enums) {
		foreach my $obj (@enums) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
	if (@pDefines) {
		foreach my $obj (@pDefines) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
	if (@classes) {
		foreach my $obj (@classes) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
	if (@categories) {
		foreach my $obj (@categories) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
	if (@protocols) {
		foreach my $obj (@protocols) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
	if (@properties) {
		foreach my $obj (@properties) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
    } else {
	my @objects = $obj->parsedParameters();
	if (@objects) {
		$retstring .= $indent."PARSED PARAMETERS:\n";
		foreach my $obj (@objects) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}

	if ($obj->can("fields")) {
		my @objects = $obj->fields();
		if (@objects) {
			$retstring .= $indent."FIELDS:\n";
			foreach my $obj (@objects) {
				my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
				$retstring .= $newret;
				foreach my $copyobj (@newtrees) {
					push(@parseTrees, $copyobj);
				}
			}
		}
	}

	my @objects = $obj->taggedParameters();
	if (@objects) {
		$retstring .= $indent."TAGGED PARAMETERS:\n";
		foreach my $obj (@objects) {
			my ($newret, @newtrees) = $self->dumpObjNames($obj, $nest + 1);
			$retstring .= $newret;
			foreach my $copyobj (@newtrees) {
				push(@parseTrees, $copyobj);
			}
		}
	}
    }

    return ($retstring, @parseTrees);
}

sub supportsAllDecs
{
    my $self = shift;
    my $lang = $self->{LANG};
    my $sublang = $self->{SUBLANG};

    return allow_everything($lang, $sublang);

    ## if ($lang eq "C") { return 1; };
    ## if ($lang eq "java") {
	## if ($sublang ne "javascript") {
		## return 1;
	## }
    ## }
    ## if ($lang eq "pascal") { return 1; } # Maybe
    ## if ($lang eq "perl") { return 1; } # Maybe

    return 0;
}

1;

