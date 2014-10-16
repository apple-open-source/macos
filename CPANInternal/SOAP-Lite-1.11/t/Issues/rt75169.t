use strict;
use warnings;
use Test::More;
use SOAP::Lite;
use utf8;
use Encode;

my $data = "ü";
my $soap = SOAP::Serializer->new();
$soap->autotype(0);
my $xml = $soap->envelope( freeform => "$data" );
my ( $cycled ) = values %{ SOAP::Deserializer->deserialize( $xml )->body };
is( length( $data ), length( $cycled ), "UTF-8 string is the same after serializing" );


done_testing;
