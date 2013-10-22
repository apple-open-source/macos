use Test::More tests => 35;
use_ok qw(SOAP::Lite::Deserializer::XMLSchemaSOAP1_1);

is SOAP::Lite::Deserializer::XMLSchemaSOAP1_1->anyTypeValue(),
    'ur-type',
    'anyTypeValue';

is SOAP::Lite::Deserializer::XMLSchemaSOAP1_1->as_boolean('false'),
    0, 'as_boolean("false")';

is SOAP::Lite::Deserializer::XMLSchemaSOAP1_1->as_ur_type('4242'),
    '4242', 'as_ur_type(4242)';

for (qw(
    string float double decimal timeDuration recurringDuration uriReference
    integer nonPositiveInteger negativeInteger long int short byte
    nonNegativeInteger unsignedLong unsignedInt unsignedShort unsignedByte
    positiveInteger timeInstant time timePeriod date month year century 
    recurringDate recurringDay language
    anyURI
    ) ) {

    no strict qw(refs);
    my $method = "as_$_";
    is SOAP::Lite::Deserializer::XMLSchemaSOAP1_1->$method('something nice'),
    'something nice', "$method('something nice')";
    
}