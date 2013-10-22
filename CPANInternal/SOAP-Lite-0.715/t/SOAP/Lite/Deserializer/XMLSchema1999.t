use Test::More;
plan tests => 42;
use_ok qw(SOAP::Lite::Deserializer::XMLSchema1999);

is SOAP::Lite::Deserializer::XMLSchema1999->anyTypeValue(),
    'ur-type',
    'anyTypeValue';

is SOAP::Lite::Deserializer::XMLSchema1999->as_boolean('false'),
    0, 'as_boolean("false")';
is SOAP::Lite::Deserializer::XMLSchema1999->as_boolean('true'),
    1, 'as_boolean("false")';
eval {SOAP::Lite::Deserializer::XMLSchema1999->as_boolean('foobar') };
ok $@, 'died on illegal boolean value';
undef $@;

is ord SOAP::Lite::Deserializer::XMLSchema1999->as_hex('FF'),
    255, 'as_hex(FF)';
is ord SOAP::Lite::Deserializer::XMLSchema1999->as_hex('65'),
    101, 'as_hex(65)';

is SOAP::Lite::Deserializer::XMLSchema1999->as_hex('4142'),
    'AB', 'as_hex(4142)';

is SOAP::Lite::Deserializer::XMLSchema1999->as_ur_type('4242'),
    '4242', 'as_ur_type(4242)';

is SOAP::Lite::Deserializer::XMLSchema1999->as_undef('true'),
    '1', 'as_undef("true")';

is SOAP::Lite::Deserializer::XMLSchema1999->as_undef('false'),
    '0', 'as_undef("false")';

eval {SOAP::XMLSchema1999::Deserializer->as_undef('ZUMSL')};
ok $@, 'died on illegal nil value';
undef $@;

for (qw(
    string
    float double decimal 
    timeDuration recurringDuration uriReference
    integer nonPositiveInteger negativeInteger long int short byte
    nonNegativeInteger unsignedLong unsignedInt unsignedShort unsignedByte
    positiveInteger timeInstant time timePeriod date month year century
    recurringDate recurringDay language) ) {

    no strict qw(refs);
    my $method = "as_$_";
    is SOAP::Lite::Deserializer::XMLSchema1999->$method('something nice'),
    'something nice', "$method('something nice')";
    
}

