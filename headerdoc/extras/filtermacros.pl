#!/usr/bin/perl

#insert start
my $language = "IDL";
my $sublanguage = "IDL";

# use lib "/System/Library/Perl/Extras/5.8.6/";
use HeaderDoc::MacroFilter qw(filterFileString run_macro_filter_tests);
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
$headerObj->lang($language);
$headerObj->sublang($sublanguage);
my ($case_sensitive, $keywordhashref) = $headerObj->keywords();
$HeaderDoc::headerObject = $headerObj;

my %symbolarray = ();
my $debug = 0;
my $matchdebug = 0;
my $debug_hrb = 0;


#insert end

$/ = undef;

if ((scalar(@ARGV) < 1) || (scalar(@ARGV) > 2)) {
	print STDERR "Usage: filtermacros.pl input_file [ouput_file]\n";
	exit(-1);
}

my $inputfile = $ARGV[0];
my $outputfile = undef;

# print "COUNT: ".scalar(@ARGV)."\n";
if (scalar(@ARGV) == 2) {
	$outputfile = $ARGV[1];
}

print STDERR "IN: $inputfile OUT: $outputfile\n" if ($debug);

my $testmode = 0;
if ($inputfile eq "-t") {
	$testmode = 1;
}

# print STDERR "$data";

# Set to 1 for value we want defined to a particular value.
# Set to -1 for value we want to be explicitly undefined.
# The implicit value 0 means "don't care".
%HeaderDoc::filter_macro_definition_state = (
	"LANGUAGE_OBJECTIVE_C" => -1,
	"LANGUAGE_JAVASCRIPT" => 1
);

# Values for ignore_expressions
# Only used if HeaderDoc::filter_macro_definition_state value is 1.
%HeaderDoc::filter_macro_definition_value = (
	"LANGUAGE_JAVASCRIPT" => 1
);

if ($testmode) {
	print STDERR "Test mode.\n";
	run_macro_filter_tests();
	die("Tests done.\n");
}

open(INPUTFILE, "<$inputfile") || die("Could not open input file \"$inputfile\"\n");
my $data = <INPUTFILE>;
my $output = filterFileString($data);

if ($outputfile) {
	open(OUTPUTFILE, ">$outputfile");
	print OUTPUTFILE $output;
	close(OUTPUTFILE);
} else {
	print $output;
}

