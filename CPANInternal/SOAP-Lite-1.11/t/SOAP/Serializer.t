use strict;
use warnings;
use Test::More tests => 13; #qw(no_plan);

use_ok qw(SOAP::Lite);

my $serializer = SOAP::Serializer->new();

is $serializer->find_prefix('http://schemas.xmlsoap.org/soap/envelope/'), 'soap';

ok my $tag = $serializer->tag('fooxml', {}), 'serialize <fooxml/>';
ok $tag = $serializer->tag('_xml', {}), 'serialize <_xml/>';
eval {
    $tag = $serializer->tag('---', {});
};
like $@, qr{^Element \s '---' \s can't \s be \s allowed}x, 'error on <xml:lang/>';
undef $@;
eval {
	local $SIG{__WARN__} = sub {
		like $_[0], qr{^Element \s 'xmlfoo'}x;
	};

    $tag = $serializer->tag('xmlfoo', {});
};


my $xml = $serializer->envelope('fault', faultstring => '>>> foo <<<');
like $xml, qr{\&gt;\&gt;\&gt;}x, 'fault escaped OK';
unlike $xml, qr{\&amp;gt;}x, 'fault escaped OK';

$xml = $serializer->envelope('response', foo => '>>> bar <<<');
like $xml, qr{\&gt;\&gt;\&gt;}x, 'response escaped OK';
unlike $xml, qr{\&amp;gt;}x, 'response escaped OK';

$xml = $serializer->envelope('method', foo => '>>> bar <<<');
like $xml, qr{\&gt;\&gt;\&gt;}x, 'response escaped OK';
unlike $xml, qr{\&amp;gt;}x, 'response escaped OK';


SKIP: {
    eval "require Test::Differences"
        or skip 'Cannot test without Test::Differences', 1;

    my $serializer = SOAP::Serializer->new()->autotype(1);

    my @som_data = ( SOAP::Data->name('Spec'), SOAP::Data->name('Version')
    );
    my $complex_data =  SOAP::Data->name( 'complex' )
        ->attr( { attr => '123' } )
        ->value( [ SOAP::Data->name('Spec'), SOAP::Data->name('Version') ]
    );
    my $result = $serializer->encode_object($complex_data, 'test', undef, {});

    # remove attributes created by autotype
    delete $result->[1]->{'soapenc:arrayType'};
    delete $result->[1]->{'xsi:type'};

    # turn off autotyping
    $serializer->autotype(0);

    # now we are able to compare both kinds of serialization
    Test::Differences::eq_or_diff( $result,
                $serializer->encode_object($complex_data, 'test', undef, {}),
                'autotype off array serialization');
}
