# $Id: 06elements.t,v 1.1.1.2 2007/10/10 23:04:15 ahuda Exp $

##
# this test checks the DOM element and attribute interface of XML::LibXML

use Test;

BEGIN { plan tests => 187 };
use XML::LibXML;

my $foo       = "foo";
my $bar       = "bar";
my $nsURI     = "http://foo";
my $prefix    = "x";
my $attname1  = "A";
my $attvalue1 = "a";
my $attname2  = "B";
my $attvalue2 = "b";
my $attname3  = "C";

my @badnames= ("1A", "<><", "&", "-:");

print "# 1. bound node\n";
{
    my $doc = XML::LibXML::Document->new();
    my $elem = $doc->createElement( $foo );
    ok($elem);
    ok($elem->tagName, $foo);
    
    {
        foreach my $name ( @badnames ) {
            eval { $elem->setNodeName( $name ); };
            ok( $@ );
        }
    }
    
    $elem->setAttribute( $attname1, $attvalue1 );
    ok( $elem->hasAttribute($attname1) );
    ok( $elem->getAttribute($attname1), $attvalue1);

    my $attr = $elem->getAttributeNode($attname1);
    ok($attr);
    ok($attr->name, $attname1);
    ok($attr->value, $attvalue1);

    $elem->setAttribute( $attname1, $attvalue2 );
    ok($elem->getAttribute($attname1), $attvalue2);
    ok($attr->value, $attvalue2);

    my $attr2 = $doc->createAttribute($attname2, $attvalue1);
    ok($attr2);

    $elem->setAttributeNode($attr2);
    ok($elem->hasAttribute($attname2) );
    ok($elem->getAttribute($attname2),$attvalue1);

    my $tattr = $elem->getAttributeNode($attname2);
    ok($tattr->isSameNode($attr2));

    $elem->setAttribute($attname2, "");    
    ok($elem->hasAttribute($attname2) );
    ok($elem->getAttribute($attname2), "");
    
    $elem->setAttribute($attname3, "");    
    ok($elem->hasAttribute($attname3) );
    ok($elem->getAttribute($attname3), "");

    {
        foreach my $name ( @badnames ) {
            eval {$elem->setAttribute( $name, "X" );};
            ok( $@ );
        }

    }


    print "# 1.1 Namespaced Attributes\n";

    $elem->setAttributeNS( $nsURI, $prefix . ":". $foo, $attvalue2 );
    ok( $elem->hasAttributeNS( $nsURI, $foo ) );
    ok( ! $elem->hasAttribute( $foo ) );
    ok( $elem->hasAttribute( $prefix.":".$foo ) );
    # warn $elem->toString() , "\n";
    $tattr = $elem->getAttributeNodeNS( $nsURI, $foo );
    ok($tattr);
    ok($tattr->name, $foo);
    ok($tattr->nodeName, $prefix .":".$foo);
    ok($tattr->value, $attvalue2 );

    $elem->removeAttributeNode( $tattr );
    ok( !$elem->hasAttributeNS($nsURI, $foo) );


    # empty NS
    $elem->setAttributeNS( '', $foo, $attvalue2 );
    ok( $elem->hasAttribute( $foo ) );
    $tattr = $elem->getAttributeNode( $foo );
    ok($tattr);
    ok($tattr->name, $foo);
    ok($tattr->nodeName, $foo);
    ok(!defined($tattr->namespaceURI));
    ok($tattr->value, $attvalue2 );

    ok($elem->hasAttribute($foo) == 1);
    ok($elem->hasAttributeNS(undef, $foo) == 1);
    ok($elem->hasAttributeNS('', $foo) == 1);
     
    $elem->removeAttributeNode( $tattr );
    ok( !$elem->hasAttributeNS('', $foo) );
    ok( !$elem->hasAttributeNS(undef, $foo) );

    # node based functions
    my $e2 = $doc->createElement($foo);
    $doc->setDocumentElement($e2);
    my $nsAttr = $doc->createAttributeNS( $nsURI.".x", $prefix . ":". $foo, $bar);
    ok( $nsAttr );
    $elem->setAttributeNodeNS($nsAttr);
    ok( $elem->hasAttributeNS($nsURI.".x", $foo) );    
    $elem->removeAttributeNS( $nsURI.".x", $foo);
    ok( !$elem->hasAttributeNS($nsURI.".x", $foo) );

    # warn $elem->toString;
    print "# set attribute ".$prefix . ":". $attname1."\n";

    $elem->setAttributeNS( $nsURI, $prefix . ":". $attname1, $attvalue2 );
    # warn $elem->toString;


    $elem->removeAttributeNS("",$attname1);
    # warn $elem->toString;

    ok( ! $elem->hasAttribute($attname1) );
    ok( $elem->hasAttributeNS($nsURI,$attname1) );
    # warn $elem->toString;

    {
        foreach my $name ( @badnames ) {
            eval {$elem->setAttributeNS( undef, $name, "X" );};
            ok( $@ );
        }
    }
} 

print "# 2. unbound node\n";
{
    my $elem = XML::LibXML::Element->new($foo);
    ok($elem);
    ok($elem->tagName, $foo);

    $elem->setAttribute( $attname1, $attvalue1 );
    ok( $elem->hasAttribute($attname1) );
    ok( $elem->getAttribute($attname1), $attvalue1);

    my $attr = $elem->getAttributeNode($attname1);
    ok($attr);
    ok($attr->name, $attname1);
    ok($attr->value, $attvalue1);

    $elem->setAttributeNS( $nsURI, $prefix . ":". $foo, $attvalue2 );
    ok( $elem->hasAttributeNS( $nsURI, $foo ) );
    # warn $elem->toString() , "\n";
    my $tattr = $elem->getAttributeNodeNS( $nsURI, $foo );
    ok($tattr);
    ok($tattr->name, $foo);
    ok($tattr->nodeName, $prefix .":".$foo);
    ok($tattr->value, $attvalue2 );

    $elem->removeAttributeNode( $tattr );
    ok( !$elem->hasAttributeNS($nsURI, $foo) );
    # warn $elem->toString() , "\n";
}

print "# 3. Namespace handling\n";
print "# 3.1 Namespace switching\n";
{
    my $elem = XML::LibXML::Element->new($foo);
    ok($elem);

    my $doc = XML::LibXML::Document->new();
    my $e2 = $doc->createElement($foo);
    $doc->setDocumentElement($e2);
    my $nsAttr = $doc->createAttributeNS( $nsURI, $prefix . ":". $foo, $bar);
    ok( $nsAttr );

    $elem->setAttributeNodeNS($nsAttr);
    ok( $elem->hasAttributeNS($nsURI, $foo) );    

    ok( not defined $nsAttr->ownerDocument);
    # warn $elem->toString() , "\n";
} 

print "# 3.2 default Namespace and Attributes\n";
{
    my $doc  = XML::LibXML::Document->new();
    my $elem = $doc->createElementNS( "foo", "root" );
    $doc->setDocumentElement( $elem );

    $elem->setNamespace( "foo", "bar" );

    $elem->setAttributeNS( "foo", "x:attr",  "test" );
    $elem->setAttributeNS( undef, "attr2",  "test" );

    ok( $elem->getAttributeNS( "foo", "attr" ), "test" );
    ok( $elem->getAttributeNS( "", "attr2" ), "test" );

    # warn $doc->toString;
    # actually this doesn't work correctly with libxml2 <= 2.4.23
    $elem->setAttributeNS( "foo", "attr2",  "bar" );
    ok( $elem->getAttributeNS( "foo", "attr2" ), "bar" );
    # warn $doc->toString;
}

print "# 4. Text Append and Normalization\n";

{
    my $doc = XML::LibXML::Document->new();
    my $t1 = $doc->createTextNode( "bar1" );
    my $t2 = $doc->createTextNode( "bar2" );
    my $t3 = $doc->createTextNode( "bar3" );
    my $e  = $doc->createElement("foo");
    $e->appendChild( $t1 );
    $e->appendChild( $t2 );
    $e->appendChild( $t3 );

    my @cn = $e->childNodes;

    # this is the correct behaviour for DOM. the nodes are still
    # refered
    ok( scalar( @cn ), 3 );
    
    $e->normalize;
    
    @cn = $e->childNodes;
    ok( scalar( @cn ), 1 );

    ok(not defined $t2->parentNode);
    ok(not defined $t3->parentNode);
}


print "# 5. XML::LibXML extensions\n";
{
    my $plainstring = "foo";
    my $stdentstring= "$foo & this";
    
    my $doc = XML::LibXML::Document->new();
    my $elem = $doc->createElement( $foo );
    $doc->setDocumentElement( $elem );
    
    $elem->appendText( $plainstring );
    ok( $elem->string_value , $plainstring );

    $elem->appendText( $stdentstring );
    ok( $elem->string_value , $plainstring.$stdentstring );

    $elem->appendTextChild( "foo");
    $elem->appendTextChild( "foo" => "foo&bar" );

    my @cn = $elem->childNodes;
    ok( @cn );
    ok( scalar(@cn), 3 );
    ok( !$cn[1]->hasChildNodes);
    ok( $cn[2]->hasChildNodes);
}

print "# 6. XML::LibXML::Attr nodes\n";
{
  my $dtd = <<'EOF';
<!DOCTYPE root [
<!ELEMENT root EMPTY>
<!ATTLIST root fixed CDATA  #FIXED "foo">
<!ATTLIST root a:ns_fixed CDATA  #FIXED "ns_foo">
<!ATTLIST root name NMTOKEN #IMPLIED>
<!ENTITY ent "ENT">
]>
EOF
  my $ns = q(urn:xx);
  my $xml_nons = qq(<root foo="&quot;bar&ent;&quot;" xmlns:a="$ns"/>);
  my $xml_ns = qq(<root xmlns="$ns" xmlns:a="$ns" foo="&quot;bar&ent;&quot;"/>);

  for my $xml ($xml_nons, $xml_ns) {
    my $parser = new XML::LibXML;
    $parser->complete_attributes(0);
    $parser->expand_entities(0);
    my $doc = $parser->parse_string($dtd.$xml);

    ok ($doc);
    my $root = $doc->getDocumentElement;
    {
      my $attr = $root->getAttributeNode('foo');
      ok ($attr);
      ok (ref($attr), 'XML::LibXML::Attr');
      ok ($root->isSameNode($attr->ownerElement));
      ok ($attr->value, '"barENT"');
      ok ($attr->serializeContent, '&quot;bar&ent;&quot;');
      ok ($attr->toString, ' foo="&quot;bar&ent;&quot;"');
    }
    {
      my $attr = $root->getAttributeNodeNS(undef,'foo');
      ok ($attr);
      ok (ref($attr), 'XML::LibXML::Attr');
      ok ($root->isSameNode($attr->ownerElement));
      ok ($attr->value, '"barENT"');
    }
    # fixed values are defined
    ok ($root->getAttribute('fixed'),'foo');
    skip (
	XML::LibXML::LIBXML_VERSION >= 20627 ? 0 : 'skipping for libxml2 <= 2.6.27',
	$root->getAttributeNS($ns,'ns_fixed'),'ns_foo');
    ok ($root->getAttribute('a:ns_fixed'),'ns_foo');

    ok ($root->hasAttribute('fixed'),0);
    ok ($root->hasAttributeNS($ns,'ns_fixed'),0);
    ok ($root->hasAttribute('a:ns_fixed'),0);


    # but no attribute nodes correspond to them
    ok (!defined $root->getAttributeNode('a:ns_fixed'));
    ok (!defined $root->getAttributeNode('fixed'));
    ok (!defined $root->getAttributeNode('name'));
    ok (!defined $root->getAttributeNode('baz'));
    ok (!defined $root->getAttributeNodeNS($ns,'foo'));
    ok (!defined $root->getAttributeNodeNS($ns,'fixed'));
    ok (!defined $root->getAttributeNodeNS($ns,'ns_fixed'));
    ok (!defined $root->getAttributeNodeNS(undef,'fixed'));
    ok (!defined $root->getAttributeNodeNS(undef,'name'));
    ok (!defined $root->getAttributeNodeNS(undef,'baz'));
  }

  for my $xml ($xml_nons, $xml_ns) {
    my $parser = new XML::LibXML;
    $parser->complete_attributes(1);
    $parser->expand_entities(1);
    my $doc = $parser->parse_string($dtd.$xml);
    ok ($doc);
    my $root = $doc->getDocumentElement;
    {
      my $attr = $root->getAttributeNode('foo');
      ok ($attr);
      ok (ref($attr), 'XML::LibXML::Attr');
      ok ($root->isSameNode($attr->ownerElement));
      ok ($attr->value, '"barENT"');
      ok ($attr->serializeContent, '&quot;barENT&quot;');
      ok ($attr->toString, ' foo="&quot;barENT&quot;"');
    }
    # fixed values are defined
    ok ($root->getAttribute('fixed'),'foo');
    ok ($root->getAttributeNS($ns,'ns_fixed'),'ns_foo');
    ok ($root->getAttribute('a:ns_fixed'),'ns_foo');

    # and attribute nodes are created
    {
      my $attr = $root->getAttributeNode('fixed');
      ok (ref($attr), 'XML::LibXML::Attr');
      ok ($attr->value,'foo');
      ok ($attr->toString, ' fixed="foo"');
    }
    {
      my $attr = $root->getAttributeNode('a:ns_fixed');
      ok (ref($attr), 'XML::LibXML::Attr');
      ok ($attr->value,'ns_foo');
    }
    {
      my $attr = $root->getAttributeNodeNS($ns,'ns_fixed');
      ok (ref($attr), 'XML::LibXML::Attr');
      ok ($attr->value,'ns_foo');
      ok ($attr->toString, ' a:ns_fixed="ns_foo"');
    }

    ok (!defined $root->getAttributeNode('ns_fixed'));
    ok (!defined $root->getAttributeNode('name'));
    ok (!defined $root->getAttributeNode('baz'));
    ok (!defined $root->getAttributeNodeNS($ns,'foo'));
    ok (!defined $root->getAttributeNodeNS($ns,'fixed'));
    ok (!defined $root->getAttributeNodeNS(undef,'name'));
    ok (!defined $root->getAttributeNodeNS(undef,'baz'));
  }
}
