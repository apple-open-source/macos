use Test;
BEGIN { $tests = 0;
    if ($] >= 5.007002) { $tests = 7 }
    plan tests => $tests;
}
if ($tests) {
use XML::SAX::PurePerl;
use XML::SAX::PurePerl::DebugHandler;

my $handler = XML::SAX::PurePerl::DebugHandler->new();
ok($handler);

my $parser = XML::SAX::PurePerl->new(Handler => $handler);
ok($parser);

# warn("utf-16\n");
$parser->parse_uri("testfiles/utf-16.xml");
ok(1);

# warn("utf-16le\n");
$parser->parse_uri("testfiles/utf-16le.xml");
ok(1);

# warn("koi8_r\n");
$parser->parse_uri("testfiles/koi8_r.xml");
ok(1);

# warn("8859-1\n");
$parser->parse_uri("testfiles/iso8859_1.xml");
ok(1);

# warn("8859-2\n");
$parser->parse_uri("testfiles/iso8859_2.xml");
ok(1);
}
