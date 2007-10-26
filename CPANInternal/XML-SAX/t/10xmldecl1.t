use Test;
BEGIN { plan tests => 5 }
use XML::SAX::PurePerl;
use XML::SAX::PurePerl::DebugHandler;
use IO::File;

my $handler = XML::SAX::PurePerl::DebugHandler->new();
ok($handler);

my $parser = XML::SAX::PurePerl->new(Handler => $handler);
ok($parser);

my $file = IO::File->new("testfiles/01.xml") || die $!;
ok($file);

$parser->parse_file($file);
ok($handler->{seen}{start_document});
ok($handler->{seen}{end_document});

