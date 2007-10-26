use Test;
BEGIN { plan tests => 6 }
use XML::SAX::PurePerl;
use XML::SAX::PurePerl::DebugHandler;

my $handler = XML::SAX::PurePerl::DebugHandler->new();
ok($handler);

my $parser = XML::SAX::PurePerl->new(Handler => $handler,
        "http://xml.org/sax/handlers/LexicalHandler" => $handler);
ok($parser);

# check PIs and comments before document element parse ok
$parser->parse_uri("testfiles/03a.xml");
ok($handler->{seen}{processing_instruction}, 2);
ok($handler->{seen}{comment});

# check invalid version number
eval {
$parser->parse_uri("testfiles/03b.xml");
};
ok($@);
ok($@->{LineNumber}, 4);


