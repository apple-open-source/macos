BEGIN { $| = 1; print "1..31\n"; }

use utf8;
use JSON::XS;
no warnings;

our $test;
sub ok($) {
   print $_[0] ? "" : "not ", "ok ", ++$test, "\n";
}

eval { JSON::XS->new->encode ([\-1]) }; ok $@ =~ /cannot encode reference/;
eval { JSON::XS->new->encode ([\undef]) }; ok $@ =~ /cannot encode reference/;
eval { JSON::XS->new->encode ([\2]) }; ok $@ =~ /cannot encode reference/;
eval { JSON::XS->new->encode ([\{}]) }; ok $@ =~ /cannot encode reference/;
eval { JSON::XS->new->encode ([\[]]) }; ok $@ =~ /cannot encode reference/;
eval { JSON::XS->new->encode ([\\1]) }; ok $@ =~ /cannot encode reference/;

eval { JSON::XS->new->allow_nonref (1)->decode ('"\u1234\udc00"') }; ok $@ =~ /missing high /;
eval { JSON::XS->new->allow_nonref->decode ('"\ud800"') }; ok $@ =~ /missing low /;
eval { JSON::XS->new->allow_nonref (1)->decode ('"\ud800\u1234"') }; ok $@ =~ /surrogate pair /;

eval { JSON::XS->new->decode ('null') }; ok $@ =~ /allow_nonref/;
eval { JSON::XS->new->allow_nonref (1)->decode ('+0') }; ok $@ =~ /malformed/;
eval { JSON::XS->new->allow_nonref->decode ('.2') }; ok $@ =~ /malformed/;
eval { JSON::XS->new->allow_nonref (1)->decode ('bare') }; ok $@ =~ /malformed/;
eval { JSON::XS->new->allow_nonref->decode ('naughty') }; ok $@ =~ /null/;
eval { JSON::XS->new->allow_nonref (1)->decode ('01') }; ok $@ =~ /leading zero/;
eval { JSON::XS->new->allow_nonref->decode ('00') }; ok $@ =~ /leading zero/;
eval { JSON::XS->new->allow_nonref (1)->decode ('-0.') }; ok $@ =~ /decimal point/;
eval { JSON::XS->new->allow_nonref->decode ('-0e') }; ok $@ =~ /exp sign/;
eval { JSON::XS->new->allow_nonref (1)->decode ('-e+1') }; ok $@ =~ /initial minus/;
eval { JSON::XS->new->allow_nonref->decode ("\"\n\"") }; ok $@ =~ /invalid character/;
eval { JSON::XS->new->allow_nonref (1)->decode ("\"\x01\"") }; ok $@ =~ /invalid character/;
eval { JSON::XS->new->decode ('[5') }; ok $@ =~ /parsing array/;
eval { JSON::XS->new->decode ('{"5"') }; ok $@ =~ /':' expected/;
eval { JSON::XS->new->decode ('{"5":null') }; ok $@ =~ /parsing object/;

eval { JSON::XS->new->decode (undef) }; ok $@ =~ /malformed/;
eval { JSON::XS->new->decode (\5) }; ok !!$@; # Can't coerce readonly
eval { JSON::XS->new->decode ([]) }; ok $@ =~ /malformed/;
eval { JSON::XS->new->decode (\*STDERR) }; ok $@ =~ /malformed/;
eval { JSON::XS->new->decode (*STDERR) }; ok !!$@; # cannot coerce GLOB

eval { decode_json ("\"\xa0") }; ok $@ =~ /malformed.*character/;
eval { decode_json ("\"\xa0\"") }; ok $@ =~ /malformed.*character/;

