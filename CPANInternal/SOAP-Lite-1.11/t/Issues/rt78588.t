use strict;
use warnings;

use Test::More;
use SOAP::Lite;
use utf8;

my $data = "mü\x{2013}";
my $serializer = SOAP::Serializer->new();
$serializer->typelookup()->{ base64Binary } = [ 10, sub { 0 }, undef];
my $xml = $serializer->envelope( freeform => $data );
my ( $cycled ) = values %{ SOAP::Deserializer->deserialize( $xml )->body };

is( $data, $cycled, "UTF-8 string is the same after serializing" );
done_testing;
