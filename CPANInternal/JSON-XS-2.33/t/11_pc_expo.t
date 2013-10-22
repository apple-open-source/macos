# copied over from JSON::PC and modified to use JSON::XS

use Test::More;
use strict;
BEGIN { plan tests => 8 };
use JSON::XS;

#########################
my ($js,$obj);
my $pc = new JSON::XS;

$js  = q|[-12.34]|;
$obj = $pc->decode($js);
is($obj->[0], -12.34, 'digit -12.34');
$js = $pc->encode($obj);
is($js,'[-12.34]', 'digit -12.34');

$js  = q|[-1.234e5]|;
$obj = $pc->decode($js);
is($obj->[0], -123400, 'digit -1.234e5');
$js = $pc->encode($obj);
is($js,'[-123400]', 'digit -1.234e5');

$js  = q|[1.23E-4]|;
$obj = $pc->decode($js);
is($obj->[0], 0.000123, 'digit 1.23E-4');
$js = $pc->encode($obj);
is($js,'[0.000123]', 'digit 1.23E-4');


$js  = q|[1.01e+30]|;
$obj = $pc->decode($js);
is($obj->[0], 1.01e+30, 'digit 1.01e+30');
$js = $pc->encode($obj);
like($js,qr/\[1.01[Ee]\+0?30\]/, 'digit 1.01e+30');

