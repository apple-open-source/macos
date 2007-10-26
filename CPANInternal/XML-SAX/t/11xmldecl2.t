use Test;
BEGIN { plan tests => 10 }
use XML::SAX::PurePerl;
use XML::SAX::PurePerl::DebugHandler;
use IO::File;

my $handler = XML::SAX::PurePerl::DebugHandler->new();
ok($handler);

my $parser = XML::SAX::PurePerl->new(Handler => $handler);
ok($parser);

my $file = IO::File->new("testfiles/02a.xml");
ok($file);

# check invalid characters
eval {
$parser->parse_file($file);
};
ok($@);

print $@;

ok($@->{Message});
ok($@->{LineNumber}, 1);
ok($@->{ColumnNumber}, 15);

# check invalid version number
eval {
$parser->parse_uri("file:testfiles/02b.xml");
};
ok($@);

print $@;

ok($@->{LineNumber}, 1);
ok($@->{ColumnNumber}, 15);

