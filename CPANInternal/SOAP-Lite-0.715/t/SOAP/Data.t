use strict;
use warnings;
use Test::More tests => 33; #qw(no_plan);

use_ok qw(SOAP::Lite);

my @setter_from = qw(
        name value attr type actor encodingStyle
        prefix uri value signature
);

my @option_from = qw(
    root mustUnderstand
);

my $data = SOAP::Data->new();
isa_ok $data, 'SOAP::Data';

for my $method ( @option_from ) {
    no strict qw(refs);
    my $data = SOAP::Data->$method("foo_$method");
}

for my $method ( @setter_from ) {
    no strict qw(refs);
    my $data = SOAP::Data->$method("foo_$method");
    isa_ok $data, 'SOAP::Data';
    is $data->$method(), "foo_$method", "SOAP::Data->$method() value";
    $data = $data->$method("bar_$method");
    is $data->$method(), "bar_$method", "\$data->$method() value";
}

$data = SOAP::Data->set_value('foo');
is $data->value(), 'foo';
