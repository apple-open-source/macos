use Test::More tests => 37;
use_ok qw(SOAP::Lite::Deserializer::XMLSchemaSOAP1_2);

is SOAP::Lite::Deserializer::XMLSchemaSOAP1_2->anyTypeValue(),
    'anyType',
    'anyTypeValue';


is SOAP::Lite::Deserializer::XMLSchemaSOAP1_2->as_boolean('true'),
    1, 'as_boolean("true")';

is SOAP::Lite::Deserializer::XMLSchemaSOAP1_2->as_boolean('false'),
    0, 'as_boolean("false")';

is SOAP::Lite::Deserializer::XMLSchemaSOAP1_2->as_anyType('4242'),
    '4242', 'as_anyType(4242)';

for (qw(
    string float double decimal dateTime timePeriod gMonth gYearMonth gYear
        century gMonthDay gDay duration recurringDuration anyURI
        language integer nonPositiveInteger negativeInteger long int short byte
        nonNegativeInteger unsignedLong unsignedInt unsignedShort unsignedByte
        positiveInteger date time dateTime
    ) ) {

    no strict qw(refs);
    my $method = "as_$_";
    is SOAP::Lite::Deserializer::XMLSchemaSOAP1_2->$method('something nice'),
    'something nice', "$method('something nice')";
    
}