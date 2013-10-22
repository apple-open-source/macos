#!/usr/bin/perl -w

use strict;
use Test::More tests => 17;
use Term::ANSIColor qw(:constants);
use File::Spec::Functions qw(catfile);
use IO::File;

BEGIN {
    use_ok 'Text::WordDiff'            or die;
    use_ok 'Text::WordDiff::ANSIColor' or die;
}

use constant STRIKETHROUGH => Text::WordDiff::ANSIColor::STRIKETHROUGH();

my $string1 = 'This is a test';
my $string2 = 'That was a test';
my $term_diff = BOLD . RED . STRIKETHROUGH . 'This is ' . RESET
              . BOLD . GREEN . UNDERLINE . 'That was ' . RESET
              . 'a test';

# Test scalar refs.
is word_diff(\$string1, \$string2), $term_diff,
    'Should get a term diff by default';

# Try code refs.
is word_diff( sub { \$string1 }, sub { \$string2 } ), $term_diff,
    'Should get same result for code refs';

# Try array refs.
my $BEGIN_WORD = qr/(?<!\w)(?=\w)/msx;
my @string1 = split $BEGIN_WORD, $string1;
my @string2 = split $BEGIN_WORD, $string2;
is word_diff( \@string1, \@string2 ), $term_diff,
    'Should get same result for array refs';

# Mix and match.
is word_diff( \$string1, \@string2 ), $term_diff,
    'Should get same result for a scalar ref and an array ref';

# Try file names.
my $filename1 = catfile qw(t data left.txt);
my $filename2 = catfile qw(t data right.txt);
my $time1     = localtime( (stat $filename1)[9] );
my $time2     = localtime( (stat $filename2)[9] );
my $header    = "--- $filename1\t$time1\n+++ $filename2\t$time2\n";

my $file_diff = 'This is a ' . BOLD . RED . STRIKETHROUGH . "tst;"
              . RESET . BOLD . GREEN . UNDERLINE . "test." . RESET . "\n"
              . BOLD . RED . STRIKETHROUGH . "it " . RESET 
              . BOLD . GREEN . UNDERLINE . "It " . RESET . "is only a\n"
              . 'test. Had ' . BOLD . RED . STRIKETHROUGH . 'it ' . RESET
              . BOLD . GREEN . UNDERLINE . 'this ' . RESET . "been an\n"
              . "actual diff, the results would\n"
              . 'have been output to ' . BOLD . RED . STRIKETHROUGH . "HTML"
              . RESET . BOLD . GREEN . UNDERLINE . "the terminal" . RESET . ".\n\n"
              . 'Some string with ' . BOLD . RED . STRIKETHROUGH . 'funny $'
              . RESET . BOLD . GREEN . UNDERLINE . 'funny @' . RESET . "\n"
              . 'chars in the end' . BOLD . RED . STRIKETHROUGH . '*'
              . RESET . BOLD . GREEN . UNDERLINE . '?' . RESET . "\n";

is word_diff($filename1, $filename2), $header . $file_diff,
    'Diff by file name should include a header';

# Try globs.
local (*FILE1, *FILE2);
open *FILE1, "<$filename1" or die qq{Cannot open "$filename1": $!};
open *FILE2, "<$filename2" or die qq{Cannot open "$filename2": $!};
is word_diff(\*FILE1, \*FILE2), $file_diff,
    'Diff by glob file handles should work';
close *FILE1;
close *FILE2;

# Try file handles.
my $fh1 = IO::File->new($filename1, '<')
    or die qq{Cannot open "$filename1": $!};
my $fh2 = IO::File->new($filename2, '<')
    or die qq{Cannot open "$filename2": $!};
is word_diff($fh1, $fh2), $file_diff,
    'Diff by IO::File objects should work';
$fh1->close;
$fh2->close;

# Try a code refence output handler.
my $output = '';
is word_diff(\$string1, \$string2, { OUTPUT => sub { $output .= shift } } ),
    2, 'Should get a count of 2 hunks with code ref output';
is $output, $term_diff, 'The code ref should have been called';

# Try a scalar ref output handler.
$output = '';
is word_diff(\$string1, \$string2, { OUTPUT => \$output } ),
    2, 'Should get a count of 2 hunks with scalar ref output';
is $output, $term_diff, 'The scalar ref should have been appended to';

# Try an array ref output handler.
my $hunks = [];
is word_diff(\$string1, \$string2, { OUTPUT => $hunks } ),
    2, 'Should get a count of 2 hunks with array ref output';
is join('', @$hunks), $term_diff,
    'The array ref should have been appended to';

# Try a file handle output handler.
my $fh = IO::File->new_tmpfile;
SKIP: {
    skip 'Cannot create temp filehandle', 2 unless $fh;
    is word_diff(\$string1, \$string2, { OUTPUT => $fh } ),
        2, 'Should get a count of 2 hunks with file handle output';
    $fh->seek(0, 0);
    is do { local $/; <$fh>; }, $term_diff,
        'The file handle should have been written to';
}
