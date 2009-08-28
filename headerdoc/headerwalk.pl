#!/usr/bin/perl

# This tool walks a header and generates a bunch of very simple
# text content.  This is mostly just a fun example of using the
# block parser in a different way than it was intended.  :-)

# Comment out all but one of these.
my $language = "C";
# my $language = "ooc";
# my $language = "cpp";

use lib "./Modules";
use HeaderDoc::Utilities qw(quote linesFromFile);
use HeaderDoc::BlockParse qw(blockParse);
use HeaderDoc::ParserState;
use HeaderDoc::ParseTree;
use HeaderDoc::APIOwner;
use File::Basename qw(basename);
use strict;

%HeaderDoc::ignorePrefixes = ();
%HeaderDoc::perHeaderIgnorePrefixes = ();
%HeaderDoc::perHeaderIgnoreFuncMacros = ();

my $headerObj = HeaderDoc::APIOwner->new();
$headerObj->lang("C");
$headerObj->sublang($language);
my ($case_sensitive, $keywordhashref) = $headerObj->keywords();
$HeaderDoc::headerObject = $headerObj;

foreach my $header (@ARGV) {
	print "HEADER $header\n";

	my @inputLines = &linesFromFile($header);

	my $inputCounter = 0;
	my $nlines = $#inputLines;
	while ($inputCounter <= $nlines) {
		my ($newInputCounter, $dec, $type, $name, $pt, $value, $pplref, $returntype, $pridec, $parseTree, $simpleTDcontents, $bpavail, $blockOffset, $conformsToList) = &blockParse($header, 0, \@inputLines, $inputCounter, 0, \%HeaderDoc::ignorePrefixes, \%HeaderDoc::perHeaderIgnorePrefixes, \%HeaderDoc::perHeaderIgnoreFuncMacros, $keywordhashref, $case_sensitive);

		$inputCounter = $newInputCounter;

		print "GOT DEC:\n";
		print $parseTree->textTree();
		print "END DEC.\n\n";

	}

}

