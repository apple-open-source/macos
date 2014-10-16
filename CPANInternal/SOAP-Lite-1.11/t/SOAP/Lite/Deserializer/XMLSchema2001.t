use Test::More tests => 42;
use_ok qw(SOAP::Lite::Deserializer::XMLSchema2001);

is SOAP::Lite::Deserializer::XMLSchema2001->anyTypeValue(),
    'anyType',
    'anyTypeValue';

is SOAP::Lite::Deserializer::XMLSchema2001->as_boolean('false'),
    0, 'as_boolean("false")';

is ord SOAP::Lite::Deserializer::XMLSchema2001->as_hexBinary('FF'),
    255, 'as_hex(FF)';
is ord SOAP::Lite::Deserializer::XMLSchema2001->as_hexBinary('65'),
    101, 'as_hex(65)';

is SOAP::Lite::Deserializer::XMLSchema2001->as_hexBinary('4142'),
    'AB', 'as_hex(4142)';

#is SOAP::Lite::Deserializer::XMLSchema2001->as_ur_type('4242'),
#    '4242', 'as_ur_type(4242)';

is SOAP::Lite::Deserializer::XMLSchema2001->as_undef('true'),
    '1', 'as_undef("true")';

is SOAP::Lite::Deserializer::XMLSchema2001->as_undef('false'),
    '0', 'as_undef("false")';

for (qw(
    string
    anyType anySimpleType
    float double decimal dateTime timePeriod gMonth gYearMonth gYear 
    century gMonthDay gDay duration recurringDuration
    language integer nonPositiveInteger negativeInteger long int short 
    byte nonNegativeInteger unsignedLong unsignedInt unsignedShort 
    unsignedByte positiveInteger date time dateTime
    QName) ) {

    no strict qw(refs);
    my $method = "as_$_";
    is SOAP::Lite::Deserializer::XMLSchema2001->$method('something nice'),
    'something nice', "$method('something nice')";
    
}