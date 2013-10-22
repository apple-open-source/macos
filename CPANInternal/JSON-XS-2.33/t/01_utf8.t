BEGIN { $| = 1; print "1..9\n"; }

use utf8;
use JSON::XS;

our $test;
sub ok($) {
   print $_[0] ? "" : "not ", "ok ", ++$test, "\n";
}

ok (JSON::XS->new->allow_nonref (1)->utf8 (1)->encode ("ü") eq "\"\xc3\xbc\"");
ok (JSON::XS->new->allow_nonref (1)->encode ("ü") eq "\"ü\"");
ok (JSON::XS->new->allow_nonref (1)->ascii (1)->utf8 (1)->encode (chr 0x8000) eq '"\u8000"');
ok (JSON::XS->new->allow_nonref (1)->ascii (1)->utf8 (1)->pretty (1)->encode (chr 0x10402) eq "\"\\ud801\\udc02\"\n");

eval { JSON::XS->new->allow_nonref (1)->utf8 (1)->decode ('"ü"') };
ok $@ =~ /malformed UTF-8/;

ok (JSON::XS->new->allow_nonref (1)->decode ('"ü"') eq "ü");
ok (JSON::XS->new->allow_nonref (1)->decode ('"\u00fc"') eq "ü");
ok (JSON::XS->new->allow_nonref (1)->decode ('"\ud801\udc02' . "\x{10204}\"") eq "\x{10402}\x{10204}");
ok (JSON::XS->new->allow_nonref (1)->decode ('"\"\n\\\\\r\t\f\b"') eq "\"\012\\\015\011\014\010");

