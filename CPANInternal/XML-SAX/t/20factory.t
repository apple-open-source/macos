use Test;
BEGIN { plan tests => 16 }
use XML::SAX::ParserFactory;

# load SAX parsers (no ParserDetails.ini available at first in blib)
use XML::SAX qw(Namespaces Validation);
ok(@{XML::SAX->parsers}, 0);
ok(XML::SAX->add_parser(q(XML::SAX::PurePerl)));
ok(@{XML::SAX->parsers}, 1);

ok(XML::SAX::ParserFactory->parser); # test class method
my $factory = XML::SAX::ParserFactory->new();
ok($factory);
ok($factory->parser);

ok($factory->require_feature(Namespaces));
ok($factory->parser);

ok($factory->require_feature(Validation));
eval {
    my $parser = $factory->parser;
    # should never get here unless PurePerl starts providing validation
    ok(!$parser);
};
ok($@);
ok($@->isa('XML::SAX::Exception'));

$factory = XML::SAX::ParserFactory->new();
my $parser = $factory->parser;
ok($parser);
eval {
    $parser->parse_string('<widget/>');
    ok(1);
};
ok(!$@);

local $XML::SAX::ParserPackage = 'XML::SAX::PurePerl';
ok(XML::SAX::ParserFactory->parser);

local $XML::SAX::ParserPackage = 'XML::SAX::PurePerl (0.01)';
ok(XML::SAX::ParserFactory->parser);

