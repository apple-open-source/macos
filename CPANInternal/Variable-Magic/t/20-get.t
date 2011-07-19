#!perl -T

use strict;
use warnings;

use Test::More tests => (2 * 4 + 2) + (2 * 2) + 1;

use Variable::Magic qw/cast/;

use lib 't/lib';
use Variable::Magic::TestWatcher;
use Variable::Magic::TestValue;

my $wiz = init_watcher 'get', 'get';

my $n = int rand 1000;
my $a = $n;

watch { cast $a, $wiz } { }, 'cast';

my $b;
# $b has to be set inside the block for the test to pass on 5.8.3 and lower
watch { $b = $a } { get => 1 }, 'assign to';
is $b, $n, 'get: assign to correctly';

$b = watch { "X${a}Y" } { get => 1 }, 'interpolate';
is $b, "X${n}Y", 'get: interpolate correctly';

{
 my $val = 0;

 init_value $val, 'get', 'get';

 value { my $x = $val } \0;
}
