#!/usr/local/bin/perl

#
# Test the use of choices
#

use Convert::ASN1;
BEGIN { require 't/funcs.pl' }

print "1..10\n";

btest 1, $asn  = Convert::ASN1->new;
btest 2, $asn->prepare( <<'[TheEnd]' ) or warn $asn->error;
  Natural  ::= CHOICE {
    prime   Prime,
    product Product
  }
  Prime    ::= [1] INTEGER
  Product  ::= CHOICE {
    perfect Perfect,
    plain   Plain
  }
  Perfect  ::= [2] INTEGER
  Plain    ::= [3] INTEGER
  Naturals ::= [4] SEQUENCE OF Natural
  List     ::= [5] SEQUENCE { list Naturals }
[TheEnd]

my $nl = $asn->find( 'List' );
my $buf = $nl->encode( list => [
                        { prime => 13 },
                        { product => { perfect => 28 } },
                        { product => { plain   => 42 } }, ] );
$result = pack( 'C*', 0xa5, 0x0b,  0xa4, 0x09,
                      0x81, 0x01, 0x0d,
                      0x82, 0x01, 0x1c,
                      0x83, 0x01, 0x2a, );
stest 3, $result, $buf;

my $seq = $nl->decode( $buf ) or warn $asn->error;
btest 4, defined( $seq ) && exists( $seq->{list} );
ntest 5, 13, $seq->{list}->[0]->{prime};
ntest 6, 28, $seq->{list}->[1]->{product}->{perfect};
ntest 7, 42, $seq->{list}->[2]->{product}->{plain};


btest 8, $asn->prepare( 'Foo ::= [1] EXPLICIT CHOICE { a  NULL }' ) or warn $asn->error;
$nl = $asn->find('Foo');
$buf = $nl->encode( a => 1 );
$result = pack 'C*', map hex, qw(A1 02 05 00);
stest 9, $result, $buf;
$seq = $nl->decode( $result )  or warn $asn->error;
btest 10, $seq->{a};
