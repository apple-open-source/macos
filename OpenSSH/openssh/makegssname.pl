#!/usr/bin/perl

use Convert::ASN1 qw(:tag);
use Digest::MD5 qw(md5);
use MIME::Base64;
use Data::Dumper;
 
$oid=shift;
my $asn=Convert::ASN1->new;
$asn->prepare("oid OBJECT IDENTIFIER");
$encoded=$asn->encode(oid => $oid);
Convert::ASN1::asn_dump($encoded);
print Dumper($asn->decode($encoded));

@entries=unpack("C*",$encoded);

print "DER representation: ";
foreach $entry (@entries) {
  print "\\x";
  printf "%02X",$entry;
}
print "\n";

$digest = md5($encoded);
# We only want the first 10 characters;
# Conversations with the authors suggest that we want to use all of the
# characters of the digest.
#$digest = substr($digest,0,10);
print "gsskeyex representation: ",encode_base64($digest),"\n";

sub encode_object_id {
  $string="";

  my @data = ($_[0] =~ /(\d+)/g);

  if(@data < 2) {
      @data = (0);
  }
  else {
      my $first = $data[1] + ($data[0] * 40);
      splice(@data,0,2,$first);
  }

#  my $l = length $string;
  $string .= pack("cw*", 0, @data);
#  substr($string,$l,1) = asn_encode_length(length($string) - $l - 1);
  return $string;
}


