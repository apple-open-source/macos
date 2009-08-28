#!/usr/local/bin/perl

#
# Test that indefinite length encodings can be decoded
#

BEGIN { require 't/funcs.pl' }

use Convert::ASN1;
my @zz = ( 0, 0 );

print "1..7\n";

btest 1, $asn = Convert::ASN1->new or warn $asn->error;
btest 2, $asn->prepare(q(
  GroupOfThis ::= [1] OCTET STRING
  GroupOfThat ::= [2] OCTET STRING
  Item        ::= [3] SEQUENCE {
     aGroup GroupOfThis OPTIONAL,
     bGroup GroupOfThat OPTIONAL
  }
  Items       ::= [4] SEQUENCE OF Item
  List        ::= [5] SEQUENCE { list Items }
)) or warn $asn->error;

my $buf = pack( 'C*',
   0xa5, 0x80,
     0xa4, 0x80,
       0xa3, 0x80,
         0x81, 0x03, ( ord('A') ) x 3,
       @zz,
       0xa3, 0x80,
         0x82, 0x03, ( ord('B') ) x 3,
       @zz,
       0xa3, 0x80,
         0x81, 0x03, ( ord('C') ) x 3,
         0x82, 0x03, ( ord('D') ) x 3,
       @zz,
     @zz,
   @zz, 
);

my $nl = $asn->find( 'List' );
my $seq = $nl->decode( $buf ) or warn $asn->error;
btest 3, defined( $seq ) && exists( $seq->{list} );
stest 4, 'AAA', $seq->{list}->[0]->{aGroup};
stest 5, 'BBB', $seq->{list}->[1]->{bGroup};
stest 6, 'CCC', $seq->{list}->[2]->{aGroup};
stest 7, 'DDD', $seq->{list}->[2]->{bGroup};
