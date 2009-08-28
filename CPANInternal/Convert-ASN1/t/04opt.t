#!/usr/local/bin/perl

#
# Test that the primitive operators are working
#

use Convert::ASN1;
BEGIN { require 't/funcs.pl' }

print "1..16\n"; # This testcase needs more tests

btest 1, $asn = Convert::ASN1->new or warn $asn->error;
btest 2, $asn->prepare(q(
 integer INTEGER OPTIONAL,
 str STRING
)) or warn $asn->error;

$result = pack("C*", 0x4, 0x3, ord('a'), ord('b'), ord('c'));
%input  = (str => "abc");
stest 3, $result, $asn->encode(%input) or warn $asn->error;
btest 4, $ret = $asn->decode($result) or warn $asn->error;
rtest 5, \%input, $ret;

$result = pack("C*", 0x2, 0x1, 0x9, 0x4, 0x3, ord('a'), ord('b'), ord('c'));
%input  = (integer => 9, str => "abc");
stest 6, $result, $asn->encode(%input) or warn $asn->error;
btest 7, $ret = $asn->decode($result) or warn $asn->error;
rtest 8, \%input, $ret;

btest 9, not( $asn->encode(integer => 9));

btest 10, $asn->prepare( q(
  SEQUENCE {
    bar [0] SET OF INTEGER OPTIONAL,
    str OCTET STRING
  }
)) or warn $asn->error;

%input = (str => 'Fred');
$result = pack "H*", "3006040446726564";
stest 11, $result, $asn->encode(%input);
btest 12, $ret = $asn->decode($result) or warn $asn->error;
rtest 13, \%input, $ret;

$result = pack "H*", "3011a009020101020105020103040446726564";
%input = (str => 'Fred', bar => [1,5,3]);
stest 14, $result, $asn->encode(%input);
btest 15, $ret = $asn->decode($result) or warn $asn->error;
rtest 16, \%input, $ret;
