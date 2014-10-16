use strict;
use warnings;
use Test;

use SOAP::Lite;

my @types1999 = qw(
   anyURI 
   string float double decimal timeDuration recurringDuration uriReference 
   integer nonPositiveInteger negativeInteger long int short byte
   nonNegativeInteger unsignedLong unsignedInt unsignedShort unsignedByte
   positiveInteger timeInstant time timePeriod date month year century 
   recurringDate recurringDay language
);

my @types2001 = qw(
    anyType anyURI
    string float double decimal dateTime timePeriod gMonth gYearMonth gYear
    century gMonthDay gDay duration recurringDuration anyURI
    language integer nonPositiveInteger negativeInteger long int short byte
    nonNegativeInteger unsignedLong unsignedInt unsignedShort unsignedByte
    positiveInteger date time dateTime
);

# types * 3 + extra tests + autotype tests
plan tests => 225;

test_serializer('SOAP::XMLSchema1999::Serializer', @types1999);
test_serializer('SOAP::XMLSchema2001::Serializer', @types2001);

sub test_serializer {
    my $package = shift;
    my @types = @_;

    print "# $package\n";

    for my $type (@types) {
        my $method = "as_$type";
	print "#   $method\n";
	use Data::Dumper;
        my $result = $package->$method('', 'test', $type , {});
	ok $result->[0] eq 'test';
	ok $result->[1]->{ 'xsi:type' };
	ok $result->[2] eq '';
    }

}

# additional tests

ok (SOAP::XMLSchema1999::Serializer->anyTypeValue eq 'ur-type');
my $enc = SOAP::XMLSchema1999::Serializer->as_hex('AA', 'test', 'hex', {});
ok $enc->[2] eq '4141';

$enc = SOAP::XMLSchema1999::Serializer->as_dateTime('AA', 'test', 'FOO', {});
ok $enc->[1]->{'xsi:type'} eq 'xsd:dateTime';
ok $enc->[2] eq 'AA';

$enc = SOAP::XMLSchema1999::Serializer->as_boolean(1, 'test', 'boolean', {});
ok $enc->[2] eq 'true';
$enc = SOAP::XMLSchema1999::Serializer->as_boolean(0, 'test', 'boolean', {});
ok $enc->[2] eq 'false';

$enc = SOAP::XMLSchema1999::Serializer->as_undef(1, 'test', 'boolean', {});
ok $enc eq '1';

$enc = SOAP::XMLSchema1999::Serializer->as_undef(0, 'test', 'boolean', {});
ok $enc eq '0';

$enc = SOAP::XMLSchema1999::Serializer->as_base64(0, 'test', 'string', {});
ok ($enc->[2] eq 'MA==');

print "# encoding/decoding Euro symbol in base64\n";
if ($] < 5.008) {
    print "# Skippng unicode test on perl <5.8 ($])\n";
    ok(1);
    ok(1);
} 
else {
    eval {
        # may fail on old perls
        my $str = chr(8364);
        utf8::encode($str);
        my $enc = SOAP::XMLSchema1999::Serializer->as_base64($str, 'test', 'string', {});
        my $enc2001 = SOAP::XMLSchema2001::Serializer->as_base64Binary($str, 'test', 'string', {});
        use MIME::Base64;
        for ($enc, $enc2001) {
            if ( decode_base64($_->[2]) eq $str ) {
                ok(1);
            }
            else {
                print "$str, ", decode_base64($enc->[2]), "\n";
                ok(0);
            }
        }
    }
}    



eval { SOAP::XMLSchema1999::Serializer->as_string([], 'test', 'string', {}) };
ok $@ =~m{ \A String \s value \s expected }xms;

eval { SOAP::XMLSchema1999::Serializer->as_anyURI([], 'test', 'string', {}) };
ok $@ =~m{ \A String \s value \s expected }xms;

ok ! SOAP::XMLSchema1999::Serializer->DESTROY();

my $serializer = SOAP::Serializer->new();
my $fault_envelope = $serializer->envelope( 
    fault => 'Code', 'string', 'Detail', 'Actor'
);

# Test fault serialization order
ok $fault_envelope =~m{ .+(faultcode).+(faultstring).+(faultactor).+(detail)}x;


$serializer = SOAP::Serializer->new();

print "# autotype tests\n";
$serializer->autotype(1);

my %type_of = (
    'true' => 'xsd:boolean',
    'false' => 'xsd:boolean',
    'rtue' => 'xsd:string',
    'aflse' => 'xsd:string',
    '012345' => 'xsd:string',
    'Hello World' => 'xsd:string',
    'http://example.org' => 'xsd:anyURI',
    '12345' => 'xsd:int',
    '-2147483648' => 'xsd:int',
    '2147483647' => 'xsd:int',
    '2147483648' => 'xsd:long',
    '5999927619709920' => 'xsd:long',
    'P' => 'xsd:string',
    'PT' => 'xsd:string',
    'PT01S' => 'xsd:duration',
);

while (my ($value, $type) = each %type_of) {
    my $result = $serializer->encode_scalar($value, 'test', undef, {});
    print "# $value => $type (result: $result->[1]->{'xsi:type'})\n";
    ok ( $result->[1]->{'xsi:type'} eq $type );
}