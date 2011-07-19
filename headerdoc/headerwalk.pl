#!/usr/bin/perl
#
# Script name:  headerwalk.pl
# Synopsis: 	Walks a header and creates some simple text content.
#
# Last Updated: $Date: 2011/02/16 13:42:00 $
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
# $Revision: 1297892520 $
######################################################################

# /*!
#     @header
#         The headerwalk tool walks a header and generates a
#         bunch of very simple text content.  This is mostly
#         just a fun example of using the block parser in a
#         different way than it was intended.  This experiment
#         led to changes that eventually resulted in the -E
#         flag to headerDoc2HTML.pl (headerdoc2html).
#
#         This document provides API-level documentation
#         on the tool's internals.  This tool is not installed
#         and no user-level documentation is provided because
#         it is intended solely as demonstration code.
#     @indexgroup HeaderDoc Tools
#  */

# Comment out all but one of these.
my $language = "C";
# my $language = "ooc";
# my $language = "cpp";

use lib "./Modules";
use HeaderDoc::Utilities qw(linesFromFile);
use HeaderDoc::BlockParse qw(blockParse);
use HeaderDoc::ParserState;
use HeaderDoc::ParseTree;
use HeaderDoc::APIOwner;
use File::Basename qw(basename);
use strict;

%HeaderDoc::ignorePrefixes = ();
%HeaderDoc::perHeaderIgnorePrefixes = ();
%HeaderDoc::perHeaderIgnoreFuncMacros = ();

my $lang = "C";
my $sublang = $language;

my $headerObj = HeaderDoc::APIOwner->new("lang" => $lang, "sublang" => $sublang);

my ($case_sensitive, $keywordhashref) = $headerObj->keywords();
$HeaderDoc::headerObject = $headerObj;

foreach my $header (@ARGV) {
	print "HEADER $header\n";

	my @inputLines = ();
	my ($encoding, $linesref) = linesFromFile($header);

	@inputLines = @{$linesref};

	my $inputCounter = 0;
	my $nlines = $#inputLines;
	while ($inputCounter <= $nlines) {
		my ($newInputCounter, $dec, $type, $name, $pt, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $blockOffset, $conformsToList, $functionContents, $returnedParserState, $nameObjectsRef, $extendsClass, $implementsClass, $propertyAttributes, $memberOfClass, $lang, $newsublang) = &blockParse($header, 0, \@inputLines, $inputCounter, 0, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive, $lang, $sublang);

		$inputCounter = $newInputCounter;
		$sublang = $newsublang;

		# $parseTree->dbprint();

		print "GOT DEC:\n";
		print $parseTree->textTree();
		print "END DEC.\n\n";

	}

}

