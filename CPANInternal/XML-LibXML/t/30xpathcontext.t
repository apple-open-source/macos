use Test;
BEGIN { plan tests => 54 };

use XML::LibXML;
use XML::LibXML::XPathContext;

my $doc = XML::LibXML->new->parse_string(<<'XML');
<foo><bar a="b"></bar></foo>
XML

# test findnodes() in list context
my @nodes = XML::LibXML::XPathContext->new($doc)->findnodes('/*');
ok(@nodes == 1);
ok($nodes[0]->nodeName eq 'foo');
ok((XML::LibXML::XPathContext->new($nodes[0])->findnodes('bar'))[0]->nodeName
   eq 'bar');

# test findnodes() in scalar context
my $nl = XML::LibXML::XPathContext->new($doc)->findnodes('/*');
ok($nl->pop->nodeName eq 'foo');
ok(!defined($nl->pop));

# test findvalue()
ok(XML::LibXML::XPathContext->new($doc)->findvalue('1+1') == 2);
ok(XML::LibXML::XPathContext->new($doc)->findvalue('1=2') eq 'false');

# test find()
ok(XML::LibXML::XPathContext->new($doc)->find('/foo/bar')->pop->nodeName
   eq 'bar');
ok(XML::LibXML::XPathContext->new($doc)->find('1*3')->value == '3');
ok(XML::LibXML::XPathContext->new($doc)->find('1=1')->to_literal eq 'true');

my $doc1 = XML::LibXML->new->parse_string(<<'XML');
<foo xmlns="http://example.com/foobar"><bar a="b"></bar></foo>
XML

# test registerNs()
my $xc = XML::LibXML::XPathContext->new($doc1);
$xc->registerNs('xxx', 'http://example.com/foobar');
ok($xc->findnodes('/xxx:foo')->pop->nodeName eq 'foo');
ok($xc->lookupNs('xxx') eq 'http://example.com/foobar');

# test unregisterNs()
$xc->unregisterNs('xxx');
eval { $xc->findnodes('/xxx:foo') };
ok($@);
ok(!defined($xc->lookupNs('xxx')));

# test getContextNode and setContextNode
ok($xc->getContextNode->isSameNode($doc1));
$xc->setContextNode($doc1->getDocumentElement);
ok($xc->getContextNode->isSameNode($doc1->getDocumentElement));
ok($xc->findnodes('.')->pop->isSameNode($doc1->getDocumentElement));

# test xpath context preserves the document
my $xc2 = XML::LibXML::XPathContext->new(
	  XML::LibXML->new->parse_string(<<'XML'));
<foo/>
XML
ok($xc2->findnodes('*')->pop->nodeName eq 'foo');

# test xpath context preserves context node
my $doc2 = XML::LibXML->new->parse_string(<<'XML');
<foo><bar/></foo>
XML
my $xc3 = XML::LibXML::XPathContext->new($doc2->getDocumentElement);
$xc3->find('/');
ok($xc3->getContextNode->toString() eq '<foo><bar/></foo>');

# check starting with empty context
my $xc4 = XML::LibXML::XPathContext->new();
ok(!defined($xc4->getContextNode));
eval { $xc4->find('/') };
ok($@);
my $cn=$doc2->getDocumentElement;
$xc4->setContextNode($cn);
ok($xc4->find('/'));
ok($xc4->getContextNode->isSameNode($doc2->getDocumentElement));
$cn=undef;
ok($xc4->getContextNode);
ok($xc4->getContextNode->isSameNode($doc2->getDocumentElement));

# check temporarily changed context node
my ($bar)=$xc4->findnodes('foo/bar',$doc2);
ok($bar->nodeName eq 'bar');
ok($xc4->getContextNode->isSameNode($doc2->getDocumentElement));

ok($xc4->findnodes('parent::*',$bar)->pop->nodeName eq 'foo');
ok($xc4->getContextNode->isSameNode($doc2->getDocumentElement));

# testcase for segfault found by Steve Hay
my $xc5 = XML::LibXML::XPathContext->new();
$xc5->registerNs('pfx', 'http://www.foo.com');
$doc = XML::LibXML->new->parse_string('<foo xmlns="http://www.foo.com" />');
$xc5->setContextNode($doc);
$xc5->findnodes('/');
$xc5->setContextNode(undef);
$xc5->getContextNode();
$xc5->setContextNode($doc);
$xc5->findnodes('/');
ok(1);

# check setting context position and size
ok($xc4->getContextPosition() == -1);
ok($xc4->getContextSize() == -1);
eval { $xc4->setContextPosition(4); };
ok($@);
eval { $xc4->setContextPosition(-4); };
ok($@);
eval { $xc4->setContextSize(-4); };
ok($@);
eval { $xc4->findvalue('position()') };
ok($@);
eval { $xc4->findvalue('last()') };
ok($@);

$xc4->setContextSize(0);
ok($xc4->getContextSize() == 0);
ok($xc4->getContextPosition() == 0);
ok($xc4->findvalue('position()')==0);
ok($xc4->findvalue('last()')==0);

$xc4->setContextSize(4);
ok($xc4->getContextSize() == 4);
ok($xc4->getContextPosition() == 1);
ok($xc4->findvalue('last()')==4);
ok($xc4->findvalue('position()')==1);
eval { $xc4->setContextPosition(5); };
ok($@);
ok($xc4->findvalue('position()')==1);
ok($xc4->getContextSize() == 4);
$xc4->setContextPosition(4);
ok($xc4->findvalue('position()')==4);
ok($xc4->findvalue('position()=last()'));

$xc4->setContextSize(-1);
ok($xc4->getContextPosition() == -1);
ok($xc4->getContextSize() == -1);
eval { $xc4->findvalue('position()') };
ok($@);
eval { $xc4->findvalue('last()') };
ok($@);


