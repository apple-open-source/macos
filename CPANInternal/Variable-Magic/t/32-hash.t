#!perl -T

use strict;
use warnings;

use Test::More tests => (2 * 21 + 7) + (2 * 5 + 4) + 1;

use Variable::Magic qw/cast dispell MGf_COPY VMG_UVAR/;

use lib 't/lib';
use Variable::Magic::TestWatcher;

my $wiz = init_watcher
        [ qw/get set len clear free copy dup local fetch store exists delete/ ],
        'hash';

my %n = map { $_ => int rand 1000 } qw/foo bar baz qux/;
my %h = %n;

watch { cast %h, $wiz } { }, 'cast';

my $s = watch { $h{foo} } +{ (fetch => 1) x VMG_UVAR },
                       'assign element to';
is $s, $n{foo}, 'hash: assign element to correctly';

for (1 .. 2) {
 $s = watch { exists $h{foo} } +{ (exists => 1) x VMG_UVAR }, "exists ($_)";
 ok $s, "hash: exists correctly ($_)";
}

my %b;
watch { %b = %h } { }, 'assign to';
is_deeply \%b, \%n, 'hash: assign to correctly';

$s = watch { \%h } { }, 'reference';

my @b = watch { @h{qw/bar qux/} }
                  +{ (fetch => 2) x VMG_UVAR }, 'slice';
is_deeply \@b, [ @n{qw/bar qux/} ], 'hash: slice correctly';

watch { %h = () } { clear => 1 }, 'empty in list context';

watch { %h = (a => 1, d => 3); () }
               +{ (store => 2, copy => 2) x VMG_UVAR, clear => 1 },
               'assign from list in void context';

watch { %h = map { $_ => 1 } qw/a b d/; }
               +{ (exists => 3, store => 3, copy => 3) x VMG_UVAR, clear => 1 },
               'assign from map in list context';

watch { $h{d} = 2; () } +{ (store => 1) x VMG_UVAR },
                    'assign old element';

watch { $h{c} = 3; () } +{ (store => 1, copy => 1) x VMG_UVAR },
                    'assign new element';

$s = watch { %h } { }, 'buckets';

@b = watch { keys %h } { }, 'keys';
is_deeply [ sort @b ], [ qw/a b c d/ ], 'hash: keys correctly';

@b = watch { values %h } { }, 'values';
is_deeply [ sort { $a <=> $b } @b ], [ 1, 1, 2, 3 ], 'hash: values correctly';

watch { while (my ($k, $v) = each %h) { } } { }, 'each';

watch {
 my %b = %n;
 watch { cast %b, $wiz } { }, 'cast 2';
} { free => 1 }, 'scope end';

watch { undef %h } { clear => 1 }, 'undef';

watch { dispell %h, $wiz } { }, 'dispell';

SKIP: {
 my $SKIP;

 unless (VMG_UVAR) {
  $SKIP = 'uvar magic';
 } else {
  eval "use B::Deparse";
  $SKIP = 'B::Deparse' if $@;
 }
 if ($SKIP) {
  $SKIP .= ' required to test uvar/clear interaction fix';
  skip $SKIP => 2 * 5 + 4;
 }

 my $bd = B::Deparse->new;

 my %h = (a => 13, b => 15);
 watch { cast %h, $wiz } { }, 'cast clear/uvar';

 my $code   = sub { my $x = $h{$_[0]}; ++$x; $x };
 my $before = $bd->coderef2text($code);
 my $res;

 watch { $res = $code->('a') } { fetch => 1 }, 'fixed fetch "a"';
 is $res, 14, 'uvar: fixed fetch "a" returned the right thing';

 my $after = $bd->coderef2text($code);
 is $before, $after, 'uvar: fixed fetch deparse correctly';

 watch { $res = $code->('b') } { fetch => 1 }, 'fixed fetch "b"';
 is $res, 16, 'uvar: fixed fetch "b" returned the right thing';

 $after = $bd->coderef2text($code);
 is $before, $after, 'uvar: fixed fetch deparse correctly';

 watch { %h = () } { clear => 1 }, 'fixed clear';

 watch { dispell %h, $wiz } { }, 'dispell clear/uvar';
}
