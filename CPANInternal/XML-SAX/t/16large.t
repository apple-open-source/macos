use Test;
BEGIN { plan tests => 3 }
use XML::SAX::PurePerl;
use XML::SAX::PurePerl::DebugHandler;

my $handler = XML::SAX::PurePerl::DebugHandler->new();
ok($handler);

my $parser = XML::SAX::PurePerl->new(Handler => $handler);
ok($parser);

my $time = time;
$parser->parse_uri("testfiles/xmltest.xml");
warn("parsed ", -s "testfiles/xmltest.xml", " bytes in ", time - $time, " seconds\n");
ok(1);

