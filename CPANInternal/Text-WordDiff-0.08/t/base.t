#!/usr/bin/perl -w

use strict;
use Test::More tests => 4;

BEGIN {
    package My::Test::Format;
    @My::Test::Format::ISA = ('Text::WordDiff::Base');
    sub file_header  { $_[0]->filename_a . ' <=> ' .$_[0]->filename_b }
    sub hunk_header  { ' hunk_header'                                 }
    sub same_items   { ' same_items ' . shift->foo                    }
    sub insert_items { ' insert_items'                                }
    sub delete_items { ' delete_items'                                }
    sub hunk_footer  { ' hunk_footer'                                 }
    sub file_footer  { ' ' . $_[0]->mtime_a . ' <=> ' .$_[0]->mtime_b }
    sub filename_a   { shift->{FILENAME_A}                            }
    sub filename_b   { shift->{FILENAME_B}                            }
    sub mtime_a      { shift->{MTIME_A}                               }
    sub mtime_b      { shift->{MTIME_B}                               }
    sub foo          { shift->{FOO}                                   }
}

BEGIN {
    use_ok 'Text::WordDiff' or die;
}

ok defined &word_diff, 'word_diff() should be exported';

my $time_a = time;
my $time_b = $time_a + 1;
my $localtime_a = localtime $time_a;
my $localtime_b = localtime $time_b;

my %opts = (
    STYLE      => 'Text::WordDiff::Base',
    FILENAME_A => 'foo',
    FILENAME_B => 'bar',
    MTIME_A    => $time_a,
    MTIME_B    => $time_b,
    FOO        => 'fooo',
);

my $header = "--- foo\t$localtime_a\n"
           . "+++ bar\t$localtime_b\n";
# Test base format class.
is word_diff(\'this is this', \'this is that', \%opts), $header,
    'The base format class should output the header only';

# Test formatting methods.
$opts{STYLE} = 'My::Test::Format';
my $result = 'foo <=> bar hunk_header same_items fooo hunk_footer hunk_header'
           . " delete_items insert_items hunk_footer $time_a <=> $time_b";

is word_diff( \'this is this', \'this is that', \%opts), $result,
    'Each formatting method should be called';
