#!/usr/local/bin/perl

#
# Test that the primitive operators are working
#

BEGIN { require 't/funcs.pl' }

use Convert::ASN1;

print "1..35\n";

btest 1, $asn = Convert::ASN1->new or warn $asn->error;
btest 2, $asn->prepare(' ints SEQUENCE OF INTEGER ') or warn $asn->error;

$result = pack("C*", 0x30, 0x0C, 0x02, 0x01, 0x09, 0x02, 0x01, 0x05,
		     0x02, 0x01, 0x03, 0x02, 0x01, 0x01);

stest 3, $result, $asn->encode(ints => [9,5,3,1]) or warn $asn->error;
btest 4, $ret = $asn->decode($result) or warn $asn->error;
btest 5, exists $ret->{'ints'};
stest 6, "9:5:3:1", join(":", @{$ret->{'ints'}});

##
##
##

$result = pack("C*",
  0x30, 0x25,
    0x30, 0x11,
      0x04, 0x04, ord('f'), ord('r'), ord('e'), ord('d'),
      0x30, 0x09,
	0x04, 0x01, ord('a'),
	0x04, 0x01, ord('b'),
	0x04, 0x01, ord('c'),
    0x30, 0x10,
      0x04, 0x03, ord('j'), ord('o'), ord('e'),
      0x30, 0x09,
	0x04, 0x01, ord('q'),
	0x04, 0x01, ord('w'),
	0x04, 0x01, ord('e'),
);

btest 7, $asn->prepare(' seq SEQUENCE OF SEQUENCE { str STRING, val SEQUENCE OF STRING } ')
  or warn $asn->error;
stest 8, $result, $asn->encode(
		seq => [
		  { str => 'fred', val => [qw(a b c)] },
		  { str => 'joe',  val => [qw(q w e)] }
		]) or warn $asn->error;

btest 9, $ret = $asn->decode($result) or warn $asn->error;
ntest 10, 1, scalar keys %$ret;
btest 11, exists $ret->{'seq'};
ntest 12, 2, scalar @{$ret->{'seq'}};
stest 13, 'fred', $ret->{'seq'}[0]{'str'};
stest 14, 'joe', $ret->{'seq'}[1]{'str'};
stest 15, "a:b:c", join(":", @{$ret->{'seq'}[0]{'val'}});
stest 16, "q:w:e", join(":", @{$ret->{'seq'}[1]{'val'}});

btest 17, $asn = Convert::ASN1->new or warn $asn->error;
btest 18, $asn->prepare(<<'EOS') or warn $asn->error;

AttributeTypeAndValue ::= SEQUENCE {
	type    STRING,
	value   STRING }

RelativeDistinguishedName ::= SET OF AttributeTypeAndValue

RDNSequence ::= SEQUENCE OF RelativeDistinguishedName

Name  ::= CHOICE { -- only one possibility for now --
	rdnSequence  RDNSequence }

Issuer ::= SEQUENCE { issuer Name }

EOS

btest 19, $asn = $asn->find('Issuer') or warn $asn->error;

$result = pack("C*",
 0x30, 0x26, 0x30, 0x24, 0x31, 0x10, 0x30, 0x06,
 0x04, 0x01, 0x31, 0x04, 0x01, 0x61, 0x30, 0x06,
 0x04, 0x01, 0x32, 0x04, 0x01, 0x62, 0x31, 0x10,
 0x30, 0x06, 0x04, 0x01, 0x33, 0x04, 0x01, 0x63,
 0x30, 0x06, 0x04, 0x01, 0x34, 0x04, 0x01, 0x64
);

stest 20, $result, $asn->encode(
  issuer => {
    rdnSequence => [
      [{ type => "1", value => "a" }, { type => "2", value => "b" }],
      [{ type => "3", value => "c" }, { type => "4", value => "d" }],
    ]
  }
) or warn $asn->error;

btest 21, $ret = $asn->decode($result) or warn $asn->error;

ntest 22, 1, $ret->{issuer}{rdnSequence}[0][0]{type};
ntest 23, 2, $ret->{issuer}{rdnSequence}[0][1]{type};
ntest 24, 3, $ret->{issuer}{rdnSequence}[1][0]{type};
ntest 25, 4, $ret->{issuer}{rdnSequence}[1][1]{type};

stest 26, 'a', $ret->{issuer}{rdnSequence}[0][0]{value};
stest 27, 'b', $ret->{issuer}{rdnSequence}[0][1]{value};
stest 28, 'c', $ret->{issuer}{rdnSequence}[1][0]{value};
stest 29, 'd', $ret->{issuer}{rdnSequence}[1][1]{value};


btest 30, $asn = Convert::ASN1->new or warn $asn->error;
btest 31, $asn->prepare('test ::= SEQUENCE OF INTEGER ') or warn $asn->error;

$result = pack("C*", 0x30, 0x0C, 0x02, 0x01, 0x09, 0x02, 0x01, 0x05,
		     0x02, 0x01, 0x03, 0x02, 0x01, 0x01);

stest 32, $result, $asn->encode([9,5,3,1]) or warn $asn->error;
btest 33, $ret = $asn->decode($result) or warn $asn->error;
btest 34, ref($ret) eq 'ARRAY';
stest 35, "9:5:3:1", join(":", @{$ret});
