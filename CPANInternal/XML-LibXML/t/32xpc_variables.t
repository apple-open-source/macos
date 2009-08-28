# -*- cperl -*-
use Test;
BEGIN { plan tests => 35 };

use XML::LibXML;
use XML::LibXML::XPathContext;

my $doc = XML::LibXML->new->parse_string(<<'XML');
<foo><bar a="b">Bla</bar><bar/></foo>
XML

my %variables = (
	'a' => XML::LibXML::Number->new(2),
	'b' => "b",
	);

sub get_variable {
  my ($data, $name, $uri)=@_;
  return exists($data->{$name}) ? $data->{$name} : undef;
}

# $c: nodelist
$variables{c} = XML::LibXML::XPathContext->new($doc)->findnodes('//bar');
ok($variables{c}->isa('XML::LibXML::NodeList'));
ok($variables{c}->size() == 2);
ok($variables{c}->get_node(1)->nodeName eq 'bar');

# $d: a single element node
$variables{d} = XML::LibXML::XPathContext->new($doc)->findnodes('/*')->pop;
ok($variables{d}->nodeName() eq 'foo');

# $e: a single text node
$variables{e} = XML::LibXML::XPathContext->new($doc)->findnodes('//text()');
ok($variables{e}->get_node(1)->data() eq 'Bla');

# $f: a single attribute node
$variables{f} = XML::LibXML::XPathContext->new($doc)->findnodes('//@*')->pop;
ok($variables{f}->nodeName() eq 'a');
ok($variables{f}->value() eq 'b');

# $f: a single document node
$variables{g} = XML::LibXML::XPathContext->new($doc)->findnodes('/')->pop;
ok($variables{g}->nodeType() == XML::LibXML::XML_DOCUMENT_NODE);

# test registerVarLookupFunc() and getVarLookupData()
my $xc = XML::LibXML::XPathContext->new($doc);
ok(!defined($xc->getVarLookupData));
$xc->registerVarLookupFunc(\&get_variable,\%variables);
ok(defined($xc->getVarLookupData));
my $h1=$xc->getVarLookupData;
my $h2=\%variables;
ok("$h1" eq "$h2" );
ok($h1 eq $xc->getVarLookupData);
ok(\&get_variable eq $xc->getVarLookupFunc);

# test values returned by XPath queries
ok($xc->find('$a') == 2);
ok($xc->find('$b') eq "b");
ok($xc->findnodes('//@a[.=$b]')->size() == 1);
ok($xc->findnodes('//@a[.=$b]')->size() == 1);
ok($xc->findnodes('$c')->size() == 2);
ok($xc->findnodes('$c')->size() == 2);
ok($xc->findnodes('$c[1]')->pop->isSameNode($variables{c}->get_node(1)));
ok($xc->findnodes('$c[@a="b"]')->size() == 1);
ok($xc->findnodes('$d')->size() == 1);
ok($xc->findnodes('$d/*')->size() == 2);
ok($xc->findnodes('$d')->pop->isSameNode($variables{d}));
ok($xc->findvalue('$e') eq 'Bla');
ok($xc->findnodes('$e')->pop->isSameNode($variables{e}->get_node(1)));
ok($xc->findnodes('$c[@*=$f]')->size() == 1);
ok($xc->findvalue('$f') eq 'b');
ok($xc->findnodes('$f')->pop->nodeName eq 'a');
ok($xc->findnodes('$f')->pop->isSameNode($variables{f}));
ok($xc->findnodes('$g')->pop->isSameNode($variables{g}));

# unregiser variable lookup
$xc->unregisterVarLookupFunc();
eval { $xc->find('$a') };
ok($@);
ok(!defined($xc->getVarLookupFunc()));

my $foo='foo';
$xc->registerVarLookupFunc(sub {},$foo);
ok($xc->getVarLookupData eq 'foo');
$foo=undef;
ok($xc->getVarLookupData eq 'foo');

