#!/usr/local/bin/perl

#
# Check whether the ANY DEFINED BY syntax is working
#

BEGIN { require 't/funcs.pl'}

use Convert::ASN1;

print "1..15\n";

btest 1, $asn_str=Convert::ASN1->new or warn $asn->error;
btest 2, $asn_str->prepare("string STRING") or warn $asn->error;
btest 3, $asn_seq=Convert::ASN1->new or warn $asn->error;
btest 4, $asn_seq->prepare(q(
  SEQUENCE { 
    integer INTEGER,
    str STRING
  }
)) or warn $asn_seq->error;

btest 5, $asn = Convert::ASN1->new or warn $asn->error;
btest 6, $asn->prepare(q(
	type OBJECT IDENTIFIER,
	content ANY DEFINED BY type
)) or warn $asn->error;

# Bogus OIDs - testing only!
btest 7, $asn->registeroid("1.1.1.1",$asn_str);
btest 8, $asn->registeroid("1.1.1.2",$asn_seq);

# Encode the first type
my $result = pack("C*", 0x06, 0x03, 0x29, 0x01, 0x01, 0x04, 0x0d, 0x4a, 0x75,
		        0x73, 0x74, 0x20, 0x61, 0x20, 0x73, 0x74, 0x72, 0x69,
                        0x6e, 0x67);

stest 9, $result, $asn->encode(type => "1.1.1.1", content => {string=>"Just a string"});
btest 10, $ret = $asn->decode($result) or warn $asn->error;
stest 11, "Just a string", $ret->{content}->{string};

# Now check the second

$result = pack("C*", 0x06, 0x03, 0x29, 0x01, 0x02, 0x30, 0x11, 0x02,
		     0x01, 0x01, 0x04, 0x0c, 0x61, 0x6e, 0x64, 0x20,
		     0x61, 0x20, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67);

stest 12, $result, $asn->encode(type => "1.1.1.2", 
			        content => {integer=>1, str=>"and a string"});
btest 13, $ret = $asn->decode($result) or warn $asn->error;
ntest 14, 1, $ret->{content}->{integer};
stest 15, "and a string", $ret->{content}->{str};
