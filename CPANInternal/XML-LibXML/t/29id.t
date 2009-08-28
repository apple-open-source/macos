#!/usr/bin/perl

use Test;
use XML::LibXML;

BEGIN {
    if (XML::LibXML::LIBXML_VERSION() >= 20623) {
        plan tests => 42;
    }
    else {
        plan tests => 0;
        print "# Skipping ID tests on libxml2 <= 2.6.23\n";
	exit;
    }
}

my $parser = XML::LibXML->new;

my $xml1 = <<'EOF';
<!DOCTYPE root [
<!ELEMENT root (root?)>
<!ATTLIST root id ID #REQUIRED
               notid CDATA #IMPLIED
>
]>
<root id="foo" notid="x"/>
EOF

my $xml2 = <<'EOF';
<root2 xml:id="foo"/>
EOF

sub _debug {
  my ($msg,$n)=@_;
  print "$msg\t$$n\n'",(ref $n ? $n->toString : "NULL"),"'\n";
}

for my $do_validate (0..1) {
  my ($n,$doc,$root,$at);
  ok( $doc = $parser->parse_string($xml1) );
  $root = $doc->getDocumentElement;
  $n = $doc->getElementById('foo');
  ok( $root->isSameNode( $n ) );

  # old name
  $n = $doc->getElementsById('foo');
  ok( $root->isSameNode( $n ) );

  $at = $n->getAttributeNode('id');
  ok( $at );
  ok( $at->isId );

  $at = $root->getAttributeNode('notid');
  ok( $at->isId == 0 );

  # _debug("1: foo: ",$n);
  $doc->getDocumentElement->setAttribute('id','bar');
  ok( $doc->validate ) if $do_validate;
  $n = $doc->getElementById('bar');
  ok( $root->isSameNode( $n ) );

  # _debug("1: bar: ",$n);
  $n = $doc->getElementById('foo');
  ok( !defined($n) );
  # _debug("1: !foo: ",$n);

  my $test = $doc->createElement('root');
  $root->appendChild($test);
  $test->setAttribute('id','new');
  ok( $doc->validate ) if $do_validate;
  $n = $doc->getElementById('new');
  ok( $test->isSameNode( $n ) );

  $at = $n->getAttributeNode('id');
  ok( $at );
  ok( $at->isId );
  # _debug("1: new: ",$n);
}

{
  my ($n,$doc,$root,$at);
  ok( $doc = $parser->parse_string($xml2) );
  $root = $doc->getDocumentElement;

  $n = $doc->getElementById('foo');
  ok( $root->isSameNode( $n ) );
  # _debug("1: foo: ",$n);

  $doc->getDocumentElement->setAttribute('xml:id','bar');
  $n = $doc->getElementById('foo');
  ok( !defined($n) );
  # _debug("1: !foo: ",$n);

  $n = $doc->getElementById('bar');
  ok( $root->isSameNode( $n ) );

  $at = $n->getAttributeNode('xml:id');
  ok( $at );
  ok( $at->isId );

  $n->setAttribute('id','FOO');
  ok( $at->isSameNode($n->getAttributeNode('xml:id')) );

  $at = $n->getAttributeNode('id');
  ok( $at );
  ok( ! $at->isId );

  $at = $n->getAttributeNodeNS('http://www.w3.org/XML/1998/namespace','id');
  ok( $at );
  ok( $at->isId );
  # _debug("1: bar: ",$n);

  $doc->getDocumentElement->setAttributeNS('http://www.w3.org/XML/1998/namespace','id','baz');
  $n = $doc->getElementById('bar');
  ok( !defined($n) );
  # _debug("1: !bar: ",$n);

  $n = $doc->getElementById('baz');
  ok( $root->isSameNode( $n ) );
  # _debug("1: baz: ",$n);
  $at = $n->getAttributeNodeNS('http://www.w3.org/XML/1998/namespace','id');
  ok( $at );
  ok( $at->isId );

  $doc->getDocumentElement->setAttributeNS('http://www.w3.org/XML/1998/namespace','xml:id','bag');
  $n = $doc->getElementById('baz');
  ok( !defined($n) );
  # _debug("1: !baz: ",$n);

  $n = $doc->getElementById('bag');
  ok( $root->isSameNode( $n ) );
  # _debug("1: bag: ",$n);

  $n->removeAttribute('id');
  ok( $root->toString, '<root2 xml:id="bag"/>' );
}

1;
