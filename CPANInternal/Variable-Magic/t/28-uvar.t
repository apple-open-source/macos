#!perl -T

use strict;
use warnings;

use Test::More;

use Variable::Magic qw/wizard cast dispell VMG_UVAR/;

if (VMG_UVAR) {
 plan tests => 2 * 15 + 12 + 14 + (4 * 2 * 2 + 1 + 1) + 1;
} else {
 plan skip_all => 'No nice uvar magic for this perl';
}

use lib 't/lib';
use Variable::Magic::TestWatcher;
use Variable::Magic::TestValue;

my $wiz = init_watcher [ qw/fetch store exists delete/ ], 'uvar';

my %h = (a => 1, b => 2, c => 3);

my $res = watch { cast %h, $wiz } { }, 'cast';
ok $res, 'uvar: cast succeeded';

my $x = watch { $h{a} } { fetch => 1 }, 'fetch directly';
is $x, 1, 'uvar: fetch directly correctly';

$x = watch { "$h{b}" } { fetch => 1 }, 'fetch by interpolation';
is $x, 2, 'uvar: fetch by interpolation correctly';

watch { $h{c} = 4 } { store => 1 }, 'store directly';

$x = watch { $h{c} = 5 } { store => 1 }, 'fetch and store';
is $x, 5, 'uvar: fetch and store correctly';

$x = watch { exists $h{c} } { exists => 1 }, 'exists';
ok $x, 'uvar: exists correctly';

$x = watch { delete $h{c} } { delete => 1 }, 'delete existing key';
is $x, 5, 'uvar: delete existing key correctly';

$x = watch { delete $h{z} } { delete => 1 }, 'delete non-existing key';
ok !defined $x, 'uvar: delete non-existing key correctly';

my $wiz2 = wizard get => sub { 0 };
cast %h, $wiz2;

$x = watch { $h{a} } { fetch => 1 }, 'fetch directly with also non uvar magic';
is $x, 1, 'uvar: fetch directly with also non uvar magic correctly';

SKIP: {
 eval "use Tie::Hash";
 skip 'Tie::Hash required to test uvar magic on tied hashes' => 2 * 5 + 4 if $@;
 diag "Using Tie::Hash $Tie::Hash::VERSION" if defined $Tie::Hash::VERSION;

 tie my %h, 'Tie::StdHash';
 %h = (x => 7, y => 8);

 $res = watch { cast %h, $wiz } { }, 'cast on tied hash';
 ok $res, 'uvar: cast on tied hash succeeded';

 $x = watch { $h{x} } { fetch => 1 }, 'fetch on tied hash';
 is $x, 7, 'uvar: fetch on tied hash succeeded';

 watch { $h{x} = 9 } { store => 1 }, 'store on tied hash';

 $x = watch { exists $h{x} } { exists => 1 }, 'exists on tied hash';
 ok $x, 'uvar: exists on tied hash succeeded';

 $x = watch { delete $h{x} } { delete => 1 }, 'delete on tied hash';
 is $x, 9, 'uvar: delete on tied hash succeeded';
}

$wiz2 = wizard fetch => sub { 0 };
my %h2 = (a => 37, b => 2, c => 3);
cast %h2, $wiz2;

$x = eval {
 local $SIG{__WARN__} = sub { die };
 $h2{a};
};
is $@, '', 'uvar: fetch with incomplete magic doesn\'t croak';
is $x, 37, 'uvar: fetch with incomplete magic correctly';

eval {
 local $SIG{__WARN__} = sub { die };
 $h2{a} = 73;
};
is $@, '',     'uvar: store with incomplete magic doesn\'t croak';
is $h2{a}, 73, 'uvar: store with incomplete magic correctly';

my $wiz3 = wizard store => sub { ++$_[2]; 0 }, copy_key => 1;
my %h3 = (a => 3);
cast %h3, $wiz3;

for my $i (1 .. 2) {
 my $key = 'a';
 eval { $h3{$key} = 3 + $i };
 is        $@,   '',  "uvar: change key in store doesn't croak ($i)";
 is        $key, 'a', "uvar: change key didn't clobber \$key ($i)";
 is_deeply \%h3, { a => 3, b => 3 + $i },
                      "uvar: change key in store correcty ($i)";
}

for my $i (1 .. 2) {
 eval { $h3{b} = 5 + $i };
 is        $@,   '',    "uvar: change readonly key in store doesn't croak ($i)";
 is_deeply \%h3, { a => 3, b => 5, c => 5 + $i },
                        "uvar: change readonly key in store correcty ($i)";
}

{
 my %val = (apple => 1);

 init_value %val, 'fetch', 'uvar';

 value { my $x = $val{apple} } { apple => 1 }, 'value store';
}

{
 my %val = (apple => 1);

 my $wv = init_value %val, 'store', 'uvar';

 value { $val{apple} = 2 } { apple => 1 }, 'value store';

 dispell %val, $wv;
 is_deeply \%val, { apple => 2 }, 'uvar: value after store';
}

{
 my %val = (apple => 1);

 init_value %val, 'exists', 'uvar';

 value { my $x = exists $val{apple} } { apple => 1 }, 'value exists';
}

{
 my %val = (apple => 1, banana => 2);

 my $wv = init_value %val, 'delete', 'uvar';

 value { delete $val{apple} } { apple => 1, banana => 2 }, 'value delete';

 dispell %val, $wv;
 is_deeply \%val, { banana => 2 }, 'uvar: value after delete';
}
