#!perl -T

use strict;
use warnings;

use Config qw/%Config/;

use Test::More tests => (2 * 14 + 2) + 2 * (2 * 8 + 4) + 3 + 1;

use Variable::Magic qw/wizard cast dispell MGf_COPY/;

use lib 't/lib';
use Variable::Magic::TestWatcher;

my $is_5130_release = ($] == 5.013 && !$Config{git_describe}) ? 1 : 0;

my $wiz = init_watcher
        [ qw/get set len clear free copy dup local fetch store exists delete/ ],
        'scalar';

my $n = int rand 1000;
my $a = $n;

watch { cast $a, $wiz } { }, 'cast';

my $b;
# $b has to be set inside the block for the test to pass on 5.8.3 and lower
watch { $b = $a } { get => 1 }, 'assign to';
is $b, $n, 'scalar: assign to correctly';

$b = watch { "X${a}Y" } { get => 1 }, 'interpolate';
is $b, "X${n}Y", 'scalar: interpolate correctly';

$b = watch { \$a } { }, 'reference';

watch { $a = 123; () } { set => 1 }, 'assign to';

watch { ++$a; () } { get => 1, set => 1 }, 'increment';

watch { --$a; () } { get => 1, set => 1 }, 'decrement';

watch { $a *= 1.5; () } { get => 1, set => 1 }, 'multiply in place';

watch { $a /= 1.5; () } { get => 1, set => 1 }, 'divide in place';

watch {
 my $b = $n;
 watch { cast $b, $wiz } { }, 'cast 2';
} { free => 1 }, 'scope end';

watch { undef $a } { set => 1 }, 'undef';

watch { dispell $a, $wiz } { }, 'dispell';

# Array element

my @a = (7, 8, 9);

watch { cast $a[1], $wiz } { }, 'array element: cast';

watch { $a[1] = 6; () } { set => 1 }, 'array element: set';

$b = watch { $a[1] } { get => ($is_5130_release ? 2 : 1) },'array element: get';
is $b, 6, 'scalar: array element: get correctly';

watch { $a[0] = 5 } { }, 'array element: set other';

$b = watch { $a[2] } { }, 'array element: get other';
is $b, 9, 'scalar: array element: get other correctly';

$b = watch { exists $a[1] } { }, 'array element: exists';
is $b, 1, 'scalar: array element: exists correctly';

# $b has to be set inside the block for the test to pass on 5.8.3 and lower
watch { $b = delete $a[1] } { get => 1, free => ($] > 5.008005 ? 1 : 0) }, 'array element: delete';
is $b, 6, 'scalar: array element: delete correctly';

watch { $a[1] = 4 } { }, 'array element: set after delete';

# Hash element

my %h = (a => 7, b => 8);

watch { cast $h{b}, $wiz } { }, 'hash element: cast';

watch { $h{b} = 6; () } { set => 1 }, 'hash element: set';

$b = watch { $h{b} } { get => ($is_5130_release ? 2 : 1) }, 'hash element: get';
is $b, 6, 'scalar: hash element: get correctly';

watch { $h{a} = 5 } { }, 'hash element: set other';

$b = watch { $h{a} } { }, 'hash element: get other';
is $b, 5, 'scalar: hash element: get other correctly';

$b = watch { exists $h{b} } { }, 'hash element: exists';
is $b, 1, 'scalar: hash element: exists correctly';

$b = watch { delete $h{b} } { get => 1, free => 1 }, 'hash element: delete';
is $b, 6, 'scalar: hash element: delete correctly';

watch { $h{b} = 4 } { }, 'hash element: set after delete';

SKIP: {
 my $SKIP;

 unless (MGf_COPY) {
  $SKIP = 'No copy magic for this perl';
 } else {
  eval "use Tie::Array";
  $SKIP = 'Tie::Array required to test clear magic on tied array values' if $@;
 }

 skip $SKIP => 3 if $SKIP;
 diag "Using Tie::Array $Tie::Array::VERSION" if defined $Tie::Array::VERSION;

 tie my @a, 'Tie::StdArray';
 $a[0] = $$;

 eval {
  cast @a, wizard copy => sub { cast $_[3], $wiz; () };
 };
 is $@, '', 'cast copy magic on tied array';

 watch { delete $a[0] } [ qw/get clear free/ ], 'delete from tied array';
}
