#!perl -T

use strict;
use warnings;

use Test::More tests => 35;

use Variable::Magic qw/wizard getdata cast dispell/;

my $c = 1;

my $wiz = eval {
 wizard data => sub { return { foo => $_[1] || 12, bar => $_[3] || 27 } },
         get => sub { $c += $_[1]->{foo}; $_[1]->{foo} = $c },
         set => sub { $c += $_[1]->{bar}; $_[1]->{bar} = $c }
};
is($@, '',             'wizard doesn\'t croak');
ok(defined $wiz,       'wizard is defined');
is(ref $wiz, 'SCALAR', 'wizard is a scalar ref');

my $a = 75;
my $res = eval { cast $a, $wiz };
is($@, '', 'cast doesn\'t croak');
ok($res,   'cast returns true');

my $data = eval { getdata my $b, $wiz };
is($@, '',       'getdata from non-magical scalar doesn\'t croak');
is($data, undef, 'getdata from non-magical scalar returns undef');

$data = eval { getdata $a, $wiz };
is($@, '', 'getdata from wizard doesn\'t croak');
ok($res,   'getdata from wizard returns true');
is_deeply($data, { foo => 12, bar => 27 },
           'getdata from wizard return value is ok');

my $b = $a;
is($c,           13, 'get magic : pass data');
is($data->{foo}, 13, 'get magic : data updated');

$a = 57;
is($c,           40, 'set magic : pass data');
is($data->{bar}, 40, 'set magic : pass data');

$data = eval { getdata $a, \"blargh" };
like($@, qr/Invalid\s+wizard\s+object\s+at\s+\Q$0\E/, 'getdata from invalid wizard croaks');
is($data, undef, 'getdata from invalid wizard returns undef');

$data = eval { getdata $a, undef };
like($@, qr/Invalid\s+wizard\s+object\s+at\s+\Q$0\E/, 'getdata from undef croaks');
is($data, undef, 'getdata from undef doesn\'t return anything');

$res = eval { dispell $a, $wiz };
is($@, '', 'dispell doesn\'t croak');
ok($res,   'dispell returns true');

$res = eval { cast $a, $wiz, qw/z j t/ };
is($@, '', 'cast with arguments doesn\'t croak');
ok($res,   'cast with arguments returns true');

$data = eval { getdata $a, $wiz };
is($@, '', 'getdata from wizard with arguments doesn\'t croak');
ok($res,   'getdata from wizard with arguments returns true');
is_deeply($data, { foo => 'z', bar => 't' },
           'getdata from wizard with arguments return value is ok');

dispell $a, $wiz;

$wiz = wizard get => sub { };
$a = 63;
$res = eval { cast $a, $wiz };
is($@, '', 'cast non-data wizard doesn\'t croak');
ok($res,   'cast non-data wizard returns true');

my @data = eval { getdata $a, $wiz };
is($@,            '',  'getdata from non-data wizard doesn\'t croak');
is_deeply(\@data, [ ], 'getdata from non-data wizard invalid returns undef');

$wiz = wizard data => sub { ++$_[1] };
my ($di, $ei) = (1, 10);
my ($d, $e);
cast $d, $wiz, $di;
cast $e, $wiz, $ei;
my $dd = getdata $d, $wiz;
my $ed = getdata $e, $wiz;
is($dd, 2,  'data from d is what we expected');
is($di, 2,  'cast arguments from d were passed by alias');
is($ed, 11, 'data from e is what we expected');
is($ei, 11, 'cast arguments from e were passed by alias');
$di *= 2;
$dd = getdata $d, $wiz;
$ed = getdata $e, $wiz;
is($dd, 2,  'data from d wasn\'t changed');
is($ed, 11, 'data from e wasn\'t changed');
