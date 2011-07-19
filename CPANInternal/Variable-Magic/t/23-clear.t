#!perl -T

use strict;
use warnings;

use Test::More tests => (2 * 5 + 2) + (2 * 2 + 1) + 1;

use Variable::Magic qw/cast dispell/;

use lib 't/lib';
use Variable::Magic::TestWatcher;
use Variable::Magic::TestValue;

my $wiz = init_watcher 'clear', 'clear';

my @a = qw/a b c/;

watch { cast @a, $wiz } { }, 'cast array';

watch { @a = () } { clear => 1 }, 'clear array';
is_deeply \@a, [ ], 'clear: clear array correctly';

my %h = (foo => 1, bar => 2);

watch { cast %h, $wiz } { }, 'cast hash';

watch { %h = () } { clear => 1 }, 'clear hash';
is_deeply \%h, { }, 'clear: clear hash correctly';

{
 my @val = (4 .. 6);

 my $wv = init_value @val, 'clear', 'clear';

 value { @val = () } [ 4 .. 6 ];

 dispell @val, $wv;
 is_deeply \@val, [ ], 'clear: value after';
}
