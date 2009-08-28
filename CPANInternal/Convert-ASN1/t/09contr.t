#!/usr/local/bin/perl

#
# Test the decode on constructed values
#

use Convert::ASN1;
BEGIN { require 't/funcs.pl' }

print "1..4\n";


btest 1, $asn  = Convert::ASN1->new or warn $asn->error;
btest 2, $asn->prepare(q(
    str STRING
)) or warn $asn->error;

my $buf = pack "C*", 0x24, 0x80,
			0x04, 0x03, 0x61, 0x62, 0x63,
			0x04, 0x03, 0x44, 0x45, 0x46,
			0x00, 0x00;


btest 3, $ret = $asn->decode($buf) or warn $asn->error;
stest 4, "abcDEF", $ret->{str};

