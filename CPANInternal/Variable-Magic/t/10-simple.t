#!perl -T

use strict;
use warnings;

use Test::More tests => 43;

use Variable::Magic qw/wizard cast dispell MGf_COPY MGf_DUP MGf_LOCAL VMG_UVAR/;

my $inv_wiz_obj = qr/Invalid\s+wizard\s+object\s+at\s+\Q$0\E/;

my $args = 7;
++$args if MGf_COPY;
++$args if MGf_DUP;
++$args if MGf_LOCAL;
$args += 5 if VMG_UVAR;
for (0 .. 20) {
 next if $_ == $args;
 eval { Variable::Magic::_wizard(('hlagh') x $_) };
 like($@, qr/Wrong\s+number\s+of\s+arguments\s+at\s+\Q$0\E/, '_wizard called directly with a wrong number of arguments croaks');
}

for (0 .. 3) {
 eval { wizard(('dong') x (2 * $_ + 1)) };
 like($@, qr/Wrong\s+number\s+of\s+arguments\s+for\s+&?wizard\(\)\s+at\s+\Q$0\E/, 'wizard called with an odd number of arguments croaks');
}

my $wiz = eval { wizard };
is($@, '',             'wizard doesn\'t croak');
ok(defined $wiz,       'wizard is defined');
is(ref $wiz, 'SCALAR', 'wizard is a scalar ref');

my $res = eval { cast $a, $wiz };
is($@, '', 'cast doesn\'t croak');
ok($res,   'cast is valid');

$res = eval { dispell $a, $wiz };
is($@, '', 'dispell from wizard doesn\'t croak');
ok($res,   'dispell from wizard is valid');

$res = eval { cast $a, $wiz };
is($@, '', 're-cast doesn\'t croak');
ok($res,   're-cast is valid');

$res = eval { dispell $a, \"blargh" };
like($@, $inv_wiz_obj, 're-dispell from wrong wizard croaks');
is($res, undef,        're-dispell from wrong wizard doesn\'t return anything');

$res = eval { dispell $a, undef };
like($@, $inv_wiz_obj, 're-dispell from undef croaks');
is($res, undef,        're-dispell from undef doesn\'t return anything');

$res = eval { dispell $a, $wiz };
is($@, '', 're-dispell from good wizard doesn\'t croak');
ok($res,   're-dispell from good wizard is valid');

$res = eval { dispell my $b, $wiz };
is($@, '',  'dispell non-magic object doesn\'t croak');
is($res, 0, 'dispell non-magic object returns 0');

my $c = 3;
$res = eval { cast $c, undef };
like($@, $inv_wiz_obj, 'cast from undef croaks');
is($res, undef,        'cast from undef doesn\'t return anything');
