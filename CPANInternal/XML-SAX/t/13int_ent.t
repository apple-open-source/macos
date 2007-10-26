use Test;
BEGIN { plan tests => 3 }
use XML::SAX::PurePerl;
use XML::SAX::PurePerl::DebugHandler;

my $handler = XML::SAX::PurePerl::DebugHandler->new();
ok($handler);

my $parser = XML::SAX::PurePerl->new(Handler => $handler);
ok($parser);

$parser->parse_uri("testfiles/04a.xml");
ok(1);

