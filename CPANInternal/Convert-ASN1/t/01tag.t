#!/usr/local/bin/perl

#
# Test that the primitive operators are working
#

use Convert::ASN1;
BEGIN { require 't/funcs.pl' }

print "1..21\n";

btest 1, $asn = Convert::ASN1->new or warn $asn->error;
btest 2, $asn->prepare(q(
  integer [0] INTEGER
)) or warn $asn->error;

$result = pack("C*", 0x80, 0x01, 0x08);
stest 3, $result, $asn->encode(integer => 8) or warn $asn->error;
btest 4, $ret = $asn->decode($result) or warn $asn->error;
ntest 5, 8, $ret->{integer};

btest 6, $asn->prepare(q(
  integer [APPLICATION 1] INTEGER
)) or warn $asn->error;

$result = pack("C*", 0x41, 0x01, 0x08);
stest 7, $result, $asn->encode(integer => 8) or warn $asn->error;
btest 8, $ret = $asn->decode($result) or warn $asn->error;
ntest 9, 8, $ret->{integer};

btest 10, $asn->prepare(q(
  integer [CONTEXT 2] INTEGER
)) or warn $asn->error;

$result = pack("C*", 0x82, 0x01, 0x08);
stest 11, $result, $asn->encode(integer => 8) or warn $asn->error;
btest 12, $ret = $asn->decode($result) or warn $asn->error;
ntest 13, 8, $ret->{integer};

btest 14, $asn->prepare(q(
  integer [UNIVERSAL 3] INTEGER
)) or warn $asn->error;

$result = pack("C*", 0x03, 0x01, 0x08);
stest 15, $result, $asn->encode(integer => 8) or warn $asn->error;
btest 16, $ret = $asn->decode($result) or warn $asn->error;
ntest 17, 8, $ret->{integer};

btest 18, $asn->prepare(q(
  integer [PRIVATE 4] INTEGER
)) or warn $asn->error;

$result = pack("C*", 0xc4, 0x01, 0x08);
stest 19, $result, $asn->encode(integer => 8) or warn $asn->error;
btest 20, $ret = $asn->decode($result) or warn $asn->error;
ntest 21, 8, $ret->{integer};

