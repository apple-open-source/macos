use Test;
BEGIN { plan tests => 3 }

use XML::XPath;
ok(1);

my $parser = XML::XPath::Parser->new();
ok($parser);

my $path = $parser->parse('/foo[position() < 1]/bar[$variable = 3]');
ok($path);

# warn("Path: ", $path->as_xml(), "\n");
