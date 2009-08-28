# $Id: 04node.t,v 1.1.1.2 2007/10/10 23:04:15 ahuda Exp $

##
# this test checks the DOM Node interface of XML::LibXML
# it relies on the success of t/01basic.t and t/02parse.t

# it will ONLY test the DOM capabilities as specified in DOM Level3
# XPath tests should be done in another test file

# since all tests are run on a preparsed 

use Test;

BEGIN { plan tests => 136 };
use XML::LibXML;
use XML::LibXML::Common qw(:libxml);

my $xmlstring = q{<foo>bar<foobar/><bar foo="foobar"/><!--foo--><![CDATA[&foo bar]]></foo>};

my $parser = XML::LibXML->new();
my $doc    = $parser->parse_string( $xmlstring );

print "# 1   Standalone Without NameSpaces\n\n"; 
print "# 1.1 Node Attributes\n";

{
    my $node = $doc->documentElement;
    my $rnode;

    ok($node);
    ok($node->nodeType, XML_ELEMENT_NODE);
    ok($node->nodeName, "foo");
    ok(not defined $node->nodeValue);
    ok($node->hasChildNodes);
    ok($node->textContent, "bar&foo bar");

    {
        my @children = $node->childNodes;
        ok( scalar @children, 5 );
        ok( $children[0]->nodeType, XML_TEXT_NODE );
        ok( $children[0]->nodeValue, "bar" );
        ok( $children[4]->nodeType, XML_CDATA_SECTION_NODE );
        ok( $children[4]->nodeValue, "&foo bar" );

        my $fc = $node->firstChild;
        ok( $fc );
        ok( $fc->isSameNode($children[0]));
        ok( $fc->baseURI =~ /unknown-/ );

        my $od = $fc->ownerDocument;
        ok( $od );
        ok( $od->isSameNode($doc));

        my $xc = $fc->nextSibling;
        ok( $xc );
        ok( $xc->isSameNode($children[1]) );

        $fc = $node->lastChild;
        ok( $fc );
        ok( $fc->isSameNode($children[4]));

        $xc = $fc->previousSibling;
        ok( $xc );
        ok( $xc->isSameNode($children[3]) );
        $rnode = $xc;

        $xc = $fc->parentNode;
        ok( $xc );
        ok( $xc->isSameNode($node) );

        $xc = $children[2];   
        {
            print "# 1.2 Attribute Node\n";
            ok( $xc->hasAttributes );
            my $attributes = $xc->attributes;
            ok( $attributes );
            ok( ref($attributes), "XML::LibXML::NamedNodeMap" );
            ok( $attributes->length, 1 );
            my $attr = $attributes->getNamedItem("foo");

            ok( $attr );
            ok( $attr->nodeType, XML_ATTRIBUTE_NODE );
            ok( $attr->nodeName, "foo" );
            ok( $attr->nodeValue, "foobar" );
            ok( $attr->hasChildNodes, 0);
        }

        {
            my @attributes = $xc->attributes;
            ok( scalar( @attributes ), 1 );
        }

        print "# 1.2 Node Cloning\n";
        {
            my $cnode  = $doc->createElement("foo");
	    $cnode->setAttribute('aaa','AAA');
	    $cnode->setAttributeNS('http://ns','x:bbb','BBB');
            my $c1node = $doc->createElement("bar");
            $cnode->appendChild( $c1node );
            
            my $xnode = $cnode->cloneNode(0);
            ok( $xnode );
            ok( $xnode->nodeName, "foo" );
            ok( not $xnode->hasChildNodes );
	    ok( $xnode->getAttribute('aaa'),'AAA' );
	    ok( $xnode->getAttributeNS('http://ns','bbb'),'BBB' );

            $xnode = $cnode->cloneNode(1);
            ok( $xnode );
            ok( $xnode->nodeName, "foo" );
            ok( $xnode->hasChildNodes );
	    ok( $xnode->getAttribute('aaa'),'AAA' );
	    ok( $xnode->getAttributeNS('http://ns','bbb'),'BBB' );

            my @cn = $xnode->childNodes;
            ok( @cn );
            ok( scalar(@cn), 1);
            ok( $cn[0]->nodeName, "bar" );
            ok( !$cn[0]->isSameNode( $c1node ) );

            print "# clone namespaced elements\n";
            my $nsnode = $doc->createElementNS( "fooNS", "foo:bar" );

            my $cnsnode = $nsnode->cloneNode(0);
            ok( $cnsnode->nodeName, "foo:bar" );
            ok( $cnsnode->localNS() );
            ok( $cnsnode->namespaceURI(), 'fooNS' );

            print "# clone namespaced elements (recursive)\n";
            my $c2nsnode = $nsnode->cloneNode(1);
            ok( $c2nsnode->toString(), $nsnode->toString() );
        }

        print "# 1.3 Node Value\n";
        my $string2 = "<foo>bar<tag>foo</tag></foo>";
        {
            my $doc2 = $parser->parse_string( $string2 );
            my $root = $doc2->documentElement;
            ok( not defined $root->nodeValue );
            ok( $root->textContent, "barfoo");
        }
    }

    {
        my $children = $node->childNodes;
        ok( defined $children );
        ok( ref($children), "XML::LibXML::NodeList" );
    }

    print "# 2. (Child) Node Manipulation\n";

    print "# 2.1 Valid Operations\n";

    {
        print "# 2.1.1 Single Node\n";

        my $inode = $doc->createElement("kungfoo"); # already tested
        my $jnode = $doc->createElement("kungfoo"); 
        my $xn = $node->insertBefore($inode, $rnode);
        ok( $xn );
        ok( $xn->isSameNode($inode) );


        $node->insertBefore( $jnode, undef );
        my @ta  = $node->childNodes();
        $xn = pop @ta;
        ok( $xn->isSameNode( $jnode ) );
        $jnode->unbindNode;

        my @cn = $node->childNodes;
        ok(scalar(@cn), 6);
        ok( $cn[3]->isSameNode($inode) );

        $xn = $node->removeChild($inode);
        ok($xn);
        ok($xn->isSameNode($inode));
    
        @cn = $node->childNodes;
        ok(scalar(@cn), 5);
        ok( $cn[3]->isSameNode($rnode) );
    
        $xn = $node->appendChild($inode);    
        ok($xn);
        ok($xn->isSameNode($inode));
        ok($xn->isSameNode($node->lastChild));

        $xn = $node->removeChild($inode);
        ok($xn);
        ok($xn->isSameNode($inode));
        ok($cn[-1]->isSameNode($node->lastChild));

        $xn = $node->replaceChild( $inode, $rnode );
        ok($xn);
        ok($xn->isSameNode($rnode));

        my @cn2 = $node->childNodes;
        ok(scalar(@cn), 5);
        ok( $cn2[3]->isSameNode($inode) );
    }

    {
        print "\n# insertAfter Tests\n";
        my $anode = $doc->createElement("a");
        my $bnode = $doc->createElement("b");
        my $cnode = $doc->createElement("c");
        my $dnode = $doc->createElement("d");

        $anode->insertAfter( $bnode, undef );
        ok( $anode->toString(), '<a><b/></a>' );

        $anode->insertAfter( $dnode, undef );
        ok( $anode->toString(), '<a><b/><d/></a>' );

        $anode->insertAfter( $cnode, $bnode );
        ok( $anode->toString(), '<a><b/><c/><d/></a>' );
        
    }

    {
        print "\ntest\n" ;
        my ($inode, $jnode );

        $inode = $doc->createElement("kungfoo"); # already tested
        $jnode = $doc->createElement("foobar"); 

        my $xn = $inode->insertBefore( $jnode, undef);
        ok( $xn );
        ok( $xn->isSameNode( $jnode ) );
        print( "# ". $xn->toString() );
        
    }

    {
        print "# 2.1.2 Document Fragment\n";

        my @cn   = $doc->documentElement->childNodes;
        my $rnode= $doc->documentElement;

        my $frag = $doc->createDocumentFragment;
        my $node1= $doc->createElement("kung");
        my $node2= $doc->createElement("foo");

        $frag->appendChild($node1);
        $frag->appendChild($node2);

        my $xn = $node->appendChild( $frag );
        ok($xn);
        my @cn2 = $node->childNodes;
        ok(scalar(@cn2), 7);
        ok($cn2[-1]->isSameNode($node2));
        ok($cn2[-2]->isSameNode($node1));

        $frag->appendChild( $node1 );
        $frag->appendChild( $node2 );

        @cn2 = $node->childNodes;
        ok(scalar(@cn2), 5);

        $xn = $node->replaceChild( $frag, $cn[3] );
        ok($xn);
        ok($xn->isSameNode($cn[3]));
        @cn2 = $node->childNodes;
        ok(scalar(@cn2), 6);

        $frag->appendChild( $node1 );
        $frag->appendChild( $node2 );

        $xn = $node->insertBefore( $frag, $cn[0] );           
        ok($xn);
        ok($node1->isSameNode($node->firstChild));
        @cn2 = $node->childNodes;
        ok(scalar(@cn2), 6);
    }

    print "# 2.2 Invalid Operations\n";


    print "# 2.3 DOM extensions \n";
    {
        my $str = "<foo><bar/>com</foo>";
        my $doc = XML::LibXML->new->parse_string( $str );
        my $elem= $doc->documentElement;
        ok( $elem );
        ok( $elem->hasChildNodes );
        $elem->removeChildNodes;
        ok( $elem->hasChildNodes,0 );
        $elem->toString;
    }    
}

print "# 3   Standalone With NameSpaces\n\n"; 

{
    my $doc = XML::LibXML::Document->new();
    my $URI ="http://kungfoo";
    my $pre = "foo";
    my $name= "bar";

    my $elem = $doc->createElementNS($URI, $pre.":".$name);

    ok($elem);
    ok($elem->nodeName, $pre.":".$name);
    ok($elem->namespaceURI, $URI);
    ok($elem->prefix, $pre);
    ok($elem->localname, $name );

    ok( $elem->lookupNamespacePrefix( $URI ), $pre);
    ok( $elem->lookupNamespaceURI( $pre ), $URI);

    my @ns = $elem->getNamespaces;
    ok( scalar(@ns) ,1 );
}

print "# 4.   Document swtiching\n";

{
    print "# 4.1 simple document\n";
    my $docA = XML::LibXML::Document->new;
    {
        my $docB = XML::LibXML::Document->new;
        my $e1   = $docB->createElement( "A" );
        my $e2   = $docB->createElement( "B" );
        my $e3   = $docB->createElementNS( "http://kungfoo", "C:D" );
        $e1->appendChild( $e2 );
        $e1->appendChild( $e3 );

        $docA->setDocumentElement( $e1 );
    }
    my $elem = $docA->documentElement;
    my @c = $elem->childNodes;
    my $xroot = $c[0]->ownerDocument;
    ok( $xroot->isSameNode($docA) );

 
}

print "# 5.   libxml2 specials\n";

{
    my $docA = XML::LibXML::Document->new;
    my $e1   = $docA->createElement( "A" );
    my $e2   = $docA->createElement( "B" );
    my $e3   = $docA->createElement( "C" ); 

    $e1->appendChild( $e2 );
    my $x = $e2->replaceNode( $e3 );
    my @cn = $e1->childNodes;
    ok(@cn);
    ok( scalar(@cn), 1 );   
    ok($cn[0]->isSameNode($e3));
    ok($x->isSameNode($e2));

    $e3->addSibling( $e2 );
    @cn = $e1->childNodes;  
    ok( scalar(@cn), 2 );   
    ok($cn[0]->isSameNode($e3));
    ok($cn[1]->isSameNode($e2));     
}

print "# 6.   implicit attribute manipulation\n";

{
    my $parser = XML::LibXML->new();
    my $doc = $parser->parse_string( '<foo bar="foo"/>' );
    my $root = $doc->documentElement;
    my $attributes = $root->attributes;
    ok($attributes);

    my $newAttr = $doc->createAttribute( "kung", "foo" );
    $attributes->setNamedItem( $newAttr );
        
    my @att = $root->attributes;
    ok(@att);
    ok(scalar(@att), 2);
    $newAttr = $doc->createAttributeNS( "http://kungfoo", "x:kung", "foo" );

    $attributes->setNamedItem($newAttr);
    @att = $root->attributes;
    ok(@att);
    ok(scalar(@att), 4); # because of the namespace ...

    $newAttr = $doc->createAttributeNS( "http://kungfoo", "x:kung", "bar" );
    $attributes->setNamedItem($newAttr);
    @att = $root->attributes;
    ok(@att);
    ok(scalar(@att), 4);
    ok($att[2]->isSameNode($newAttr));

    $attributes->removeNamedItem("x:kung");

    @att = $root->attributes;
    ok(@att);
    ok(scalar(@att), 3);
    ok($attributes->length, 3);
}

print "# 7. importing and adopting\n";

{
    my $parser = XML::LibXML->new;
    my $doc1 = $parser->parse_string( "<foo>bar<foobar/></foo>" );
    my $doc2 = XML::LibXML::Document->new;

    ok( $doc1 && $doc2 );
    my $rnode1 = $doc1->documentElement;
    ok( $rnode1 );
    my $rnode2 = $doc2->importNode( $rnode1 );
    ok( not $rnode2->isSameNode( $rnode1 ) ) ;
    $doc2->setDocumentElement( $rnode2 );

    my $node = $rnode2->cloneNode(0);
    ok( $node );
    my $cndoc = $node->ownerDocument;
    ok( $cndoc );
    ok( $cndoc->isSameNode( $doc2 ) );

    my $xnode = XML::LibXML::Element->new("test");

    my $node2 = $doc2->importNode($xnode);
    ok( $node2 );
    my $cndoc2 = $node2->ownerDocument;
    ok( $cndoc2 );
    ok( $cndoc2->isSameNode( $doc2 ) );

    my $doc3 = XML::LibXML::Document->new;
    my $node3 = $doc3->adoptNode( $xnode );
    ok( $node3 );
    ok( $xnode->isSameNode( $node3 ) );
    ok( $doc3->isSameNode( $node3->ownerDocument ) );

    my $xnode2 = XML::LibXML::Element->new("test");
    $xnode2->setOwnerDocument( $doc3 ); # alternate version of adopt node
    ok( $xnode2->ownerDocument );
    ok( $doc3->isSameNode( $xnode2->ownerDocument ) );    
}

{
  # appending empty fragment
  my $doc = XML::LibXML::Document->new();
  my $frag = $doc->createDocumentFragment();
  my $root = $doc->createElement( 'foo' );
  my $r = $root->appendChild( $frag );
  ok( $r );
}

{ 
  for my $obj (qw(Document DocumentFragment Comment CDATASection PI Text)) { 
    
  } 
}

