#!perl -T

use strict;
use warnings;

use Test::More tests => 2 * 27 + 13 + 1;

use Variable::Magic qw/cast dispell VMG_COMPAT_ARRAY_PUSH_NOLEN VMG_COMPAT_ARRAY_PUSH_NOLEN_VOID VMG_COMPAT_ARRAY_UNSHIFT_NOLEN_VOID VMG_COMPAT_ARRAY_UNDEF_CLEAR/;

use lib 't/lib';
use Variable::Magic::TestWatcher;

my $wiz = init_watcher
        [ qw/get set len clear free copy dup local fetch store exists delete/ ],
        'array';

my @n = map { int rand 1000 } 1 .. 5;
my @a = @n;

watch { cast @a, $wiz } { }, 'cast';

my $b = watch { $a[2] } { }, 'assign element to';
is $b, $n[2], 'array: assign element to correctly';

my @b = watch { @a } { len => 1 }, 'assign to';
is_deeply \@b, \@n, 'array: assign to correctly';

$b = watch { "X@{a}Y" } { len => 1 }, 'interpolate';
is $b, "X@{n}Y", 'array: interpolate correctly';

$b = watch { \@a } { }, 'reference';

@b = watch { @a[2 .. 4] } { }, 'slice';
is_deeply \@b, [ @n[2 .. 4] ], 'array: slice correctly';

watch { @a = qw/a b d/ } { set => 3, clear => 1 }, 'assign';

watch { $a[2] = 'c' } { }, 'assign old element';

watch { $a[4] = 'd' } { set => 1 }, 'assign new element';

$b = watch { exists $a[4] } { }, 'exists';
is $b, 1, 'array: exists correctly';

$b = watch { delete $a[4] } { set => 1 }, 'delete';
is $b, 'd', 'array: delete correctly';

$b = watch { @a } { len => 1 }, 'length @';
is $b, 3, 'array: length @ correctly';

# $b has to be set inside the block for the test to pass on 5.8.3 and lower
watch { $b = $#a } { len => 1 }, 'length $#';
is $b, 2, 'array: length $# correctly';

watch { push @a, 'x'; () }
                   { set => 1, (len => 1) x !VMG_COMPAT_ARRAY_PUSH_NOLEN_VOID },
                   'push (void)';

$b = watch { push @a, 'y' }
                        { set => 1, (len => 1) x !VMG_COMPAT_ARRAY_PUSH_NOLEN },
                        'push (scalar)';
is $b, 5, 'array: push (scalar) correctly';

$b = watch { pop @a } { set => 1, len => 1 }, 'pop';
is $b, 'y', 'array: pop correctly';

watch { unshift @a, 'z'; () }
                { set => 1, (len => 1) x !VMG_COMPAT_ARRAY_UNSHIFT_NOLEN_VOID },
                'unshift (void)';

$b = watch { unshift @a, 't' } { set => 1, len => 1 }, 'unshift (scalar)';
is $b, 6, 'unshift (scalar) correctly';

$b = watch { shift @a } { set => 1, len => 1 }, 'shift';
is $b, 't', 'array: shift correctly';

watch { my $i; @a = map ++$i, @a; () } { set => 5, len => 1, clear => 1}, 'map';

@b = watch { grep { $_ >= 4 } @a } { len => 1 }, 'grep';
is_deeply \@b, [ 4 .. 5 ], 'array: grep correctly';

watch { 1 for @a } { len => 5 + 1 }, 'for';

watch {
 my @b = @n;
 watch { cast @b, $wiz } { }, 'cast 2';
} { free => 1 }, 'scope end';

watch { undef @a } +{ (clear => 1) x VMG_COMPAT_ARRAY_UNDEF_CLEAR }, 'undef';

watch { dispell @a, $wiz } { }, 'dispell';
