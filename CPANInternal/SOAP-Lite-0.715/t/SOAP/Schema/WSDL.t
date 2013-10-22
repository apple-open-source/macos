use Test::More qw(no_plan);
use SOAP::Lite;

ok my $schema = SOAP::Schema::WSDL->new();

my $element = SOAP::Custom::XML::Data
        -> SOAP::Data::name('schema')
        -> set_value(
            SOAP::Custom::XML::Data
                -> SOAP::Data::name('complexType')
                ->attr({ name => 'test' })
);

my @result = SOAP::Schema::WSDL::parse_schema_element( $element );
is @result, 0, 'empty elements on empty complexType'

