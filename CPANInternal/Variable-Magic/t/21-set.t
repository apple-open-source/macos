#!perl -T

use strict;
use warnings;

use Test::More tests => (2 * 5 + 3) + (2 * 2 + 1);

use Variable::Magic qw/cast/;

use lib 't/lib';
use Variable::Magic::TestWatcher;
use Variable::Magic::TestValue;

my $wiz = init_watcher 'set', 'set';

my $a = 0;

watch { cast $a, $wiz } { }, 'cast';

my $n = int rand 1000;

watch { $a = $n } { set => 1 }, 'assign';
is $a, $n, 'set: assign correctly';

watch { ++$a } { set => 1 }, 'increment';
is $a, $n + 1, 'set: increment correctly';

watch { --$a } { set => 1 }, 'decrement';
is $a, $n, 'set: decrement correctly';

{
 my $val = 0;

 init_value $val, 'set', 'set';

 value { $val = 1 } \1;
}
