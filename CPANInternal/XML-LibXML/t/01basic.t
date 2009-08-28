use Test;
BEGIN { plan tests => 3}
END { ok(0) unless $loaded }
use XML::LibXML;

$loaded = 1;
ok(1);

my $p = XML::LibXML->new();
ok($p);

ok(XML::LibXML::LIBXML_VERSION, XML::LibXML::LIBXML_RUNTIME_VERSION);

warn "\n\nCompiled against libxml2 version: ",XML::LibXML::LIBXML_VERSION,
     "\nRunning libxml2 version:          ",XML::LibXML::LIBXML_RUNTIME_VERSION,
     "\n\n";

if (XML::LibXML::LIBXML_VERSION != XML::LibXML::LIBXML_RUNTIME_VERSION) {
   warn "DO NOT REPORT THIS FAILURE: Your setup of library paths is incorrect!\n\n";
}
