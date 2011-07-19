BEGIN { $| = 1; print "1..76\n"; }

use utf8;
use JSON::XS;

our $test;
sub ok($) {
   print $_[0] ? "" : "not ", "ok ", ++$test, "\n";
}

ok (!defined JSON::XS->new->allow_nonref (1)->decode ('null'));
ok (JSON::XS->new->allow_nonref (1)->decode ('true') == 1);
ok (JSON::XS->new->allow_nonref (1)->decode ('false') == 0);

my $true  = JSON::XS->new->allow_nonref (1)->decode ('true');
ok ($true eq 1);
ok (JSON::XS::is_bool $true);
my $false = JSON::XS->new->allow_nonref (1)->decode ('false');
ok ($false == !$true);
ok (JSON::XS::is_bool $false);
ok (++$false == 1);
ok (!JSON::XS::is_bool $false);

ok (JSON::XS->new->allow_nonref (1)->decode ('5') == 5);
ok (JSON::XS->new->allow_nonref (1)->decode ('-5') == -5);
ok (JSON::XS->new->allow_nonref (1)->decode ('5e1') == 50);
ok (JSON::XS->new->allow_nonref (1)->decode ('-333e+0') == -333);
ok (JSON::XS->new->allow_nonref (1)->decode ('2.5') == 2.5);

ok (JSON::XS->new->allow_nonref (1)->decode ('""') eq "");
ok ('[1,2,3,4]' eq encode_json decode_json ('[1,2, 3,4]'));
ok ('[{},[],[],{}]' eq encode_json decode_json ('[{},[], [ ] ,{ }]'));
ok ('[{"1":[5]}]' eq encode_json [{1 => [5]}]);
ok ('{"1":2,"3":4}' eq JSON::XS->new->canonical (1)->encode (decode_json '{ "1" : 2, "3" : 4 }'));
ok ('{"1":2,"3":1.2}' eq JSON::XS->new->canonical (1)->encode (decode_json '{ "1" : 2, "3" : 1.2 }'));

ok ('[true]'  eq encode_json [JSON::XS::true]);
ok ('[false]' eq encode_json [JSON::XS::false]);
ok ('[true]'  eq encode_json [\1]);
ok ('[false]' eq encode_json [\0]);
ok ('[null]'  eq encode_json [undef]);
ok ('[true]'  eq encode_json [JSON::XS::true]);
ok ('[false]' eq encode_json [JSON::XS::false]);

for $v (1, 2, 3, 5, -1, -2, -3, -4, 100, 1000, 10000, -999, -88, -7, 7, 88, 999, -1e5, 1e6, 1e7, 1e8) {
   ok ($v == ((decode_json "[$v]")->[0]));
   ok ($v == ((decode_json encode_json [$v])->[0]));
}

ok (30123 == ((decode_json encode_json [30123])->[0]));
ok (32123 == ((decode_json encode_json [32123])->[0]));
ok (32456 == ((decode_json encode_json [32456])->[0]));
ok (32789 == ((decode_json encode_json [32789])->[0]));
ok (32767 == ((decode_json encode_json [32767])->[0]));
ok (32768 == ((decode_json encode_json [32768])->[0]));

my @sparse; @sparse[0,3] = (1, 4);
ok ("[1,null,null,4]" eq encode_json \@sparse);

