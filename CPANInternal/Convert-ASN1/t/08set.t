#!/usr/local/bin/perl

#
# Test the use of sets
#

use Convert::ASN1;
BEGIN { require 't/funcs.pl' }

print "1..13\n";


btest 1, $asn  = Convert::ASN1->new or warn $asn->error;
btest 2, $asn->prepare(q(
  SET {
    integer INTEGER,
    bool BOOLEAN,
    str STRING
  }
)) or warn $asn->error;

my $result = pack("C*", 0x31, 0x10, 0x01, 0x01, 0x00, 0x02, 0x01, 0x09,
			0x04, 0x08, 0x41, 0x20, 0x73, 0x74, 0x72, 0x69,
			0x6E, 0x67
);
stest 3, $result, $asn->encode(integer => 9, bool => 0, str => "A string") or warn $asn->error;

btest 4, $ret = $asn->decode($result) or warn $asn->error;
ntest 5, 9, $ret->{integer};
ntest 6, 0, $ret->{bool};
stest 7, "A string", $ret->{str};

btest 8, $asn  = Convert::ASN1->new or warn $asn->error;
btest 9, $asn->prepare(q(
  SET {
    bool BOOLEAN,
    str STRING,
    integer INTEGER
  }
)) or warn $asn->error;

btest 10, $ret = $asn->decode($result) or warn $asn->error;
ntest 11, 9, $ret->{integer};
ntest 12, 0, $ret->{bool};
stest 13, "A string", $ret->{str};

