#!perl -T

use strict;
use warnings;

use Test::More tests => 33 + 41;

use Variable::Magic qw/wizard cast dispell VMG_UVAR/;

my $n = 3;
my @w;
my @c = (0) x $n;

sub multi {
 my ($cb, $tests) = @_;
 for (my $i = 0; $i < $n; ++$i) {
  my $res = eval { $cb->($i) };
  $tests->($i, $res, $@);
 }
}

eval { $w[0] = wizard get => sub { ++$c[0] }, set => sub { --$c[0] } };
is($@, '', 'wizard 0 creation doesn\'t croak');
eval { $w[1] = wizard get => sub { ++$c[1] }, set => sub { --$c[1] } };
is($@, '', 'wizard 1 creation doesn\'t croak');
eval { $w[2] = wizard get => sub { ++$c[2] }, set => sub { --$c[2] } };
is($@, '', 'wizard 2 creation doesn\'t croak');

multi sub {
 my ($i) = @_;
 $w[$i]
}, sub {
 my ($i, $res, $err) = @_;
 ok(defined $res,         "wizard $i is defined");
 is(ref $w[$i], 'SCALAR', "wizard $i is a scalar ref");
};

my $a = 0;

multi sub {
 my ($i) = @_;
 cast $a, $w[$i];
}, sub {
 my ($i, $res, $err) = @_;
 is($err, '', "cast magic $i doesn't croak");
 ok($res,     "cast magic $i is valid");
};

my $b = $a;
for (0 .. $n - 1) { is($c[$_], 1, "get magic $_"); }

$a = 1;
for (0 .. $n - 1) { is($c[$_], 0, "set magic $_"); }

my $res = eval { dispell $a, $w[1] };
is($@, '', 'dispell magic 1 doesn\'t croak');
ok($res,   'dispell magic 1 is valid');

$b = $a;
for (0, 2) { is($c[$_], 1, "get magic $_ after dispelled 1"); }

$a = 2;
for (0, 2) { is($c[$_], 0, "set magic $_ after dispelled 1"); }

$res = eval { dispell $a, $w[0] };
is($@, '', 'dispell magic 0 doesn\'t croak');
ok($res,   'dispell magic 0 is valid');

$b = $a;
is($c[2], 1, 'get magic 2 after dispelled 1 & 0');

$a = 3;
is($c[2], 0, 'set magic 2 after dispelled 1 & 0');

$res = eval { dispell $a, $w[2] };
is($@, '', 'dispell magic 2 doesn\'t croak');
ok($res,   'dispell magic 2 is valid');

SKIP: {
 skip 'No nice uvar magic for this perl' => 41 unless VMG_UVAR;

 $n = 3;
 @c = (0) x $n;

 eval { $w[0] = wizard fetch => sub { ++$c[0] }, store => sub { --$c[0] } };
 is($@, '', 'wizard with uvar 0 doesn\'t croak');
 eval { $w[1] = wizard fetch => sub { ++$c[1] }, store => sub { --$c[1] } };
 is($@, '', 'wizard with uvar 1 doesn\'t croak');
 eval { $w[2] = wizard fetch => sub { ++$c[2] }, store => sub { --$c[2] } };
 is($@, '', 'wizard with uvar 2 doesn\'t croak');

 multi sub {
  my ($i) = @_;
  $w[$i]
 }, sub {
  my ($i, $res, $err) = @_;
  ok(defined $res,         "wizard with uvar $i is defined");
  is(ref $w[$i], 'SCALAR', "wizard with uvar $i is a scalar ref");
 };

 my %h = (a => 1, b => 2);

 multi sub {
  my ($i) = @_;
  cast %h, $w[$i];
 }, sub {
  my ($i, $res, $err) = @_;
  is($err, '', "cast uvar magic $i doesn't croak");
  ok($res,     "cast uvar magic $i is valid");
 };

 my $s = $h{a};
 is($s, 1, 'fetch magic doesn\'t clobber');
 for (0 .. $n - 1) { is($c[$_], 1, "fetch magic $_"); }

 $h{a} = 3;
 for (0 .. $n - 1) { is($c[$_], 0, "store magic $_"); }
 is($h{a}, 3, 'store magic doesn\'t clobber');
 # $c[$_] == 1 for 0 .. 2

 my $res = eval { dispell %h, $w[1] };
 is($@, '', 'dispell uvar magic 1 doesn\'t croak');
 ok($res,   'dispell uvar magic 1 is valid');

 $s = $h{b};
 is($s, 2, 'fetch magic after dispelled 1 doesn\'t clobber');
 for (0, 2) { is($c[$_], 2, "fetch magic $_ after dispelled 1"); }
 
 $h{b} = 4;
 for (0, 2) { is($c[$_], 1, "store magic $_ after dispelled 1"); }
 is($h{b}, 4, 'store magic after dispelled 1 doesn\'t clobber');
 # $c[$_] == 2 for 0, 2

 $res = eval { dispell %h, $w[2] };
 is($@, '', 'dispell uvar magic 2 doesn\'t croak');
 ok($res,   'dispell uvar magic 2 is valid');

 $s = $h{b};
 is($s, 4, 'fetch magic after dispelled 1,2 doesn\'t clobber');
 for (0) { is($c[$_], 3, "fetch magic $_ after dispelled 1,2"); }

 $h{b} = 6;
 for (0) { is($c[$_], 2, "store magic $_ after dispelled 1,2"); }
 is($h{b}, 6, 'store magic after dispelled 1,2 doesn\'t clobber');
 # $c[$_] == 3 for 0

 $res = eval { dispell %h, $w[0] };
 is($@, '', 'dispell uvar magic 0 doesn\'t croak');
 ok($res,   'dispell uvar magic 0 is valid');

 $s = $h{b};
 is($s, 6, 'fetch magic after dispelled 1,2,0 doesn\'t clobber');
 $h{b} = 8;
 is($h{b}, 8, 'store magic after dispelled 1,2,0 doesn\'t clobber');
}
