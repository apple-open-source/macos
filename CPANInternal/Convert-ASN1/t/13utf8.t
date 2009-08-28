#!/usr/local/bin/perl

#
# Test the use of utf8 strings
#

use Convert::ASN1;
BEGIN { require 't/funcs.pl' }

if ($] < 5.007) {
  print "1..0\n";
  exit;
}

print "1..12\n";

btest 1, $asn  = Convert::ASN1->new() or warn $asn->error;
btest 2, $asn->prepare(q(
    str STRING
)) or warn $asn->error;

my $result = pack("C*", 0x04, 0x07, 0x75, 0x74, 0x66, 0x38, 0x20, 0xc6, 0x81);
stest 3, $result, $asn->encode(str => "utf8 " . chr(0x181)) or warn $asn->error;

btest 4, $ret = $asn->decode($result) or warn $asn->error;
stest 5, "utf8 " . chr(0xc6) . chr(0x81), $ret->{str};


btest 6, $asn->prepare(q(
    str UTF8String
)) or warn $asn->error;
my $utf_str = "utf8 " . chr(0x181);

$result = pack("C*", 0x0c, 0x07, 0x75, 0x74, 0x66, 0x38, 0x20, 0xc6, 0x81);
stest 7, $result, $asn->encode(str => $utf_str) or warn $asn->error;

btest 8, $ret = $asn->decode($result) or warn $asn->error;
stest 9, $utf_str, $ret->{str};

# Test that UTF8String will upgrade on encoding
$result = pack("C*", 0x0c, 0x02, 0xc2, 0x81);
stest 10, $result, $asn->encode(str => chr(0x81)) or warn $asn->error;

btest 11, $ret = $asn->decode($result) or warn $asn->error;
stest 12, chr(0x81), $ret->{str};
