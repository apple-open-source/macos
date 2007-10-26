use Test;
BEGIN { plan tests => 11 }
use XML::LibXML;
use IO::Handle;
ok(1);

my $dom = XML::LibXML->new->parse_fh(*DATA);
ok($dom);

my @nodelist = $dom->findnodes('//BBB');
ok(scalar(@nodelist), 5);

my $nodelist = $dom->findnodes('//BBB');
ok($nodelist->size, 5);

ok($nodelist->string_value, "OK"); # first node in set

ok($nodelist->to_literal, "OKNOT OK");

ok($dom->findvalue("//BBB"), "OKNOT OK");

ok(ref($dom->find("1 and 2")), "XML::LibXML::Boolean");

ok(ref($dom->find("'Hello World'")), "XML::LibXML::Literal");

ok(ref($dom->find("32 + 13")), "XML::LibXML::Number");

ok(ref($dom->find("//CCC")), "XML::LibXML::NodeList");

__DATA__
<AAA>
<BBB>OK</BBB>
<CCC/>
<BBB/>
<DDD><BBB/></DDD>
<CCC><DDD><BBB/><BBB>NOT OK</BBB></DDD></CCC>
</AAA>
