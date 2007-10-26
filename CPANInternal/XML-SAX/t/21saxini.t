use strict;
use Test;
plan tests => 8;

use File::Path;
use Fatal qw(open close chdir mkpath);
use XML::SAX;
use vars qw(*FILE);

ok(@{XML::SAX->parsers}, 0);
ok(XML::SAX->add_parser(q(XML::SAX::PurePerl)));
ok(@{XML::SAX->parsers}, 1);

rmtree('t/lib', 0, 0);
mkpath('t/lib', 0, 0777);

push @INC, 't/lib';
write_file('t/lib/TestParserPackage.pm', <<EOT);
package TestParserPackage;
use XML::SAX::Base;
\@ISA = qw(XML::SAX::Base);
sub new { 
    return bless {}, shift
}
sub supported_features {
    return ('http://axkit.org/sax/frobnosticating');
}
1;
EOT

my $parser;

# Test we can get TestParserPackage out
write_file('t/lib/SAX.ini', 'ParserPackage = TestParserPackage');
$parser = XML::SAX::ParserFactory->parser();
ok(ref($parser), "TestParserPackage", "Parser was not TestParserPackage");

# Test we can get XML::SAX::PurePerl out
write_file('t/lib/SAX.ini', 'ParserPackage = XML::SAX::PurePerl');
$parser = XML::SAX::ParserFactory->parser();
ok(ref($parser), "XML::SAX::PurePerl", "Parser was not XML::SAX::PurePerl");

# Test we can ask for a frobnosticating parser, but not get it
write_file('t/lib/SAX.ini', 'http://axkit.org/sax/frobnosticating = 1');
$parser = XML::SAX::ParserFactory->parser();
ok(ref($parser), "XML::SAX::PurePerl", "Parser was not PurePerl (frobnosticating)");

# Test we can ask for a frobnosticating parser, and get it
XML::SAX->add_parser('TestParserPackage');
$parser = XML::SAX::ParserFactory->parser();
ok(ref($parser), "TestParserPackage", "Parser was not TestParserPackage (frobnosticating)");

# Test we can get a namespaces parser
write_file('t/lib/SAX.ini', 'http://xml.org/sax/features/namespaces = 1');
$parser = XML::SAX::ParserFactory->parser();
ok(ref($parser), "XML::SAX::PurePerl", "Parser was not PurePerl (namespaces)");

sub write_file {
    my ($file, $data) = @_;
    open(FILE, ">$file");
    print FILE $data, "\n";
    close FILE;
}