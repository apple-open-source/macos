# $Id: 03doc.t,v 1.1.1.2 2007/10/10 23:04:15 ahuda Exp $

##
# this test checks the DOM Document interface of XML::LibXML
# it relies on the success of t/01basic.t and t/02parse.t

# it will ONLY test the DOM capabilities as specified in DOM Level3
# XPath tests should be done in another test file

# since all tests are run on a preparsed 

use Test;
use strict;

BEGIN { plan tests => 135 };
use XML::LibXML;
use XML::LibXML::Common qw(:libxml);

{
    print "# 1. Document Attributes\n";

    my $doc = XML::LibXML::Document->createDocument();
    ok($doc);
    ok( not defined $doc->encoding); 
    ok( $doc->version,  "1.0" );
    ok( $doc->standalone, -1 );  # is the value we get for undefined,
                                 # actually the same as 0 but just not set.
    ok( not defined $doc->URI);  # should be set by default.
    ok( $doc->compression, -1 ); # -1 indicates NO compression at all!
                                 # while 0 indicates just no zip compression 
                                 # (big difference huh?)

    $doc->setEncoding( "iso-8859-1" );
    ok( $doc->encoding, "iso-8859-1" );

    $doc->setVersion(12.5);
    ok( $doc->version, "12.5" );

    $doc->setStandalone(1);
    ok( $doc->standalone, 1 );

    $doc->setBaseURI( "localhost/here.xml" );
    ok( $doc->URI, "localhost/here.xml" );

    my $doc2 = XML::LibXML::Document->createDocument("1.1", "iso-8859-2");
    ok( $doc2->encoding, "iso-8859-2" );
    ok( $doc2->version,  "1.1" );
    ok( $doc2->standalone,  -1 );
}

{
    print "# 2. Creating Elements\n";
    my $doc = XML::LibXML::Document->new();
    {
        my $node = $doc->createDocumentFragment();
        ok($node);
        ok($node->nodeType, XML_DOCUMENT_FRAG_NODE);
    }

    {
        my $node = $doc->createElement( "foo" );
        ok($node);
        ok($node->nodeType, XML_ELEMENT_NODE );
        ok($node->nodeName, "foo" );
    }
    
    {
        print "# document with encoding\n";
        my $encdoc = XML::LibXML::Document->new( "1.0" );
        $encdoc->setEncoding( "iso-8859-1" );
        {
            my $node = $encdoc->createElement( "foo" );
            ok($node);
            ok($node->nodeType, XML_ELEMENT_NODE );
            ok($node->nodeName, "foo" );
        }

        print "# SAX style document with encoding\n";
        my $node_def = {
            Name => "object",
            LocalName => "object",
            Prefix => "",
            NamespaceURI => "",
                       };
        {
            my $node = $encdoc->createElement( $node_def->{Name} );
            ok($node);
            ok($node->nodeType, XML_ELEMENT_NODE );
            ok($node->nodeName, "object" );
        }
    }

    {
        # namespaced element test
        my $node = $doc->createElementNS( "http://kungfoo", "foo:bar" );
        ok($node);
        ok($node->nodeType, XML_ELEMENT_NODE);
        ok($node->nodeName, "foo:bar");
        ok($node->prefix, "foo");
        ok($node->localname, "bar");
        ok($node->namespaceURI, "http://kungfoo");
    }

    {
        print "# bad element creation\n";
        my @badnames = ( ";", "&", "<><", "/", "1A");

        foreach my $name ( @badnames ) {
            my $node = eval {$doc->createElement( $name );};
            ok( not defined $node );
        }

    }

    {
        my $node = $doc->createTextNode( "foo" );
        ok($node);
        ok($node->nodeType, XML_TEXT_NODE );
        ok($node->nodeValue, "foo" );
    }

    {
        my $node = $doc->createComment( "foo" );
        ok($node);
        ok($node->nodeType, XML_COMMENT_NODE );
        ok($node->nodeValue, "foo" );
        ok($node->toString, "<!--foo-->");
    }

    {
        my $node = $doc->createCDATASection( "foo" );
        ok($node);
        ok($node->nodeType, XML_CDATA_SECTION_NODE );
        ok($node->nodeValue, "foo" );
        ok($node->toString, "<![CDATA[foo]]>");
    }

    print "# 2.1 Create Attributes\n";
    {
        my $attr = $doc->createAttribute("foo", "bar");
        ok($attr);
        ok($attr->nodeType, XML_ATTRIBUTE_NODE );
        ok($attr->name, "foo");
        ok($attr->value, "bar" );
        ok($attr->hasChildNodes, 0);
        my $content = $attr->firstChild;
        ok( $content );
        ok( $content->nodeType, XML_TEXT_NODE );
    }
    {
        print "# bad attribute creation\n";
        my @badnames = ( ";", "&", "<><", "/", "1A");

        foreach my $name ( @badnames ) {
            my $node = eval {$doc->createAttribute( $name, "bar" );};
            ok( not defined $node );
        }

    }


    {
        eval {
            my $attr = $doc->createAttributeNS("http://kungfoo", "kung:foo","bar");
        };
        ok($@);

        my $root = $doc->createElement( "foo" );
        $doc->setDocumentElement( $root );

        my $attr;
        eval {
           $attr = $doc->createAttributeNS("http://kungfoo", "kung:foo","bar");
        };
        ok($attr);
        ok($attr->nodeName, "kung:foo");
        ok($attr->name,"foo" );
        ok($attr->value, "bar" );

        $attr->setValue( q(bar&amp;) );
        ok($attr->getValue, q(bar&amp;) );
    }
    {
        print "# bad attribute creation\n";
        my @badnames = ( ";", "&", "<><", "/", "1A");

        foreach my $name ( @badnames ) {
            my $node = eval {$doc->createAttributeNS( undef, $name, "bar" );};
            ok( not defined $node );
        }

    }

    print "# 2.2 Create PIs\n";
    {
        my $pi = $doc->createProcessingInstruction( "foo", "bar" );
        ok($pi);
        ok($pi->nodeType, XML_PI_NODE);
        ok($pi->nodeName, "foo");
        ok($pi->textContent, "bar");
        ok($pi->getData, "bar");
    }

    {
        my $pi = $doc->createProcessingInstruction( "foo" );
        ok($pi);
        ok($pi->nodeType, XML_PI_NODE);
        ok($pi->nodeName, "foo");
	my $data = $pi->textContent;
	# undef or "" depending on libxml2 version
        ok( !defined $data or length($data)==0 );
	$data = $pi->getData;
        ok( !defined $data or length($data)==0 );
	$pi->setData(q(bar&amp;));
	ok( $pi->getData, q(bar&amp;));
        ok($pi->textContent, q(bar&amp;));
    }
}

{
    print "# 3.  Document Manipulation\n";
    print "# 3.1 Document Elements\n"; 

    my $doc = XML::LibXML::Document->new();
    my $node = $doc->createElement( "foo" );
    $doc->setDocumentElement( $node );
    my $tn = $doc->documentElement;
    ok($tn);
    ok($node->isSameNode($tn));

    my $node2 = $doc->createElement( "bar" );
    
    $doc->appendChild($node2);
    my @cn = $doc->childNodes;
    ok( scalar(@cn) , 1);
    ok($cn[0]->isSameNode($node));

    $doc->insertBefore($node2, $node);
    @cn = $doc->childNodes;
    ok( scalar(@cn) , 1);
    ok($cn[0]->isSameNode($node));

    $doc->removeChild($node);
    @cn = $doc->childNodes;
    ok( scalar(@cn) , 0);

    for ( 1..2 ) {
        my $nodeA = $doc->createElement( "x" );
        $doc->setDocumentElement( $nodeA );
    }
    ok(1); # must not segfault here :)

    $doc->setDocumentElement( $node2 );
    @cn = $doc->childNodes;
    ok( scalar(@cn) , 1);
    ok($cn[0]->isSameNode($node2));

    my $node3 = $doc->createElementNS( "http://foo", "bar" );
    ok($node3);

    print "# 3.2 Processing Instructions\n"; 
    {
        my $pi = $doc->createProcessingInstruction( "foo", "bar" );
        $doc->appendChild( $pi );
        @cn = $doc->childNodes;
        ok( $pi->isSameNode($cn[-1]) );
        $pi->setData( 'bar="foo"' );
        ok( $pi->textContent, 'bar="foo"');
        $pi->setData( foo=>"foo" );
        ok( $pi->textContent, 'foo="foo"');
        
    }

    print "# 3.3 Comment Nodes\n"; 

    print "# 3.4 DTDs\n";
}

{
    print "# 4. Document Storing\n";
    my $parser = XML::LibXML->new;
    my $doc = $parser->parse_string("<foo>bar</foo>");  

    ok( $doc );

    print "# 4.1 to file handle\n";
    {
        require IO::File;
        my $fh = new IO::File;
        if ( $fh->open( "> example/testrun.xml" ) ) {
            $doc->toFH( $fh );
            $fh->close;
            ok(1);
            # now parse the file to check, if succeeded
            my $tdoc = $parser->parse_file( "example/testrun.xml" );
            ok( $tdoc );
            ok( $tdoc->documentElement );
            ok( $tdoc->documentElement->nodeName, "foo" );
            ok( $tdoc->documentElement->textContent, "bar" );
            unlink "example/testrun.xml" ;
        }
    }

    print "# 4.2 to named file\n";
    {
        $doc->toFile( "example/testrun.xml" );
        ok(1);
        # now parse the file to check, if succeeded
        my $tdoc = $parser->parse_file( "example/testrun.xml" );
        ok( $tdoc );
        ok( $tdoc->documentElement );
        ok( $tdoc->documentElement->nodeName, "foo" );
        ok( $tdoc->documentElement->textContent, "bar" );
        unlink "example/testrun.xml" ;        
    }

    print "# 5 ELEMENT LIKE FUNCTIONS\n";
    {
        my $parser2 = XML::LibXML->new();
        my $string1 = "<A><A><B/></A><A><B/></A></A>";
        my $string2 = '<C:A xmlns:C="xml://D"><C:A><C:B/></C:A><C:A><C:B/></C:A></C:A>';
        my $string3 = '<A xmlns="xml://D"><A><B/></A><A><B/></A></A>';
        my $string4 = '<C:A><C:A><C:B/></C:A><C:A><C:B/></C:A></C:A>';
        my $string5 = '<A xmlns:C="xml://D"><C:A>foo<A/>bar</C:A><A><C:B/>X</A>baz</A>';
        {
            my $doc2 = $parser2->parse_string($string1);
            my @as   = $doc2->getElementsByTagName( "A" );
            ok( scalar( @as ), 3);

            @as   = $doc2->getElementsByTagName( "*" );
            ok( scalar( @as ), 5);

            @as   = $doc2->getElementsByTagNameNS( "*", "B" );
            ok( scalar( @as ), 2);

            @as   = $doc2->getElementsByLocalName( "A" );
            ok( scalar( @as ), 3);

            @as   = $doc2->getElementsByLocalName( "*" );
            ok( scalar( @as ), 5);
        }
        {
            my $doc2 = $parser2->parse_string($string2);
            my @as   = $doc2->getElementsByTagName( "C:A" );
            ok( scalar( @as ), 3);
            @as   = $doc2->getElementsByTagNameNS( "xml://D", "A" );
            ok( scalar( @as ), 3);
            @as   = $doc2->getElementsByTagNameNS( "*", "A" );
            ok( scalar( @as ), 3);
            @as   = $doc2->getElementsByLocalName( "A" );
            ok( scalar( @as ), 3);
        }
        {
            my $doc2 = $parser2->parse_string($string3);
#            my @as   = $doc2->getElementsByTagName( "A" );
#            ok( scalar( @as ), 3);
            my @as   = $doc2->getElementsByTagNameNS( "xml://D", "A" );
            ok( scalar( @as ), 3);
            @as   = $doc2->getElementsByLocalName( "A" );
            ok( scalar( @as ), 3);
        }
        {
	    $parser2->recover(1);
	    local $SIG{'__WARN__'} = sub { }; 
            my $doc2 = $parser2->parse_string($string4);
#            my @as   = $doc2->getElementsByTagName( "C:A" );
#            ok( scalar( @as ), 3);
            my @as   = $doc2->getElementsByLocalName( "A" );
            ok( scalar( @as ), 3);
        }
        {
            my $doc2 = $parser2->parse_string($string5);
            my @as   = $doc2->getElementsByTagName( "C:A" );
            ok( scalar( @as ), 1);
            @as   = $doc2->getElementsByTagName( "A" );
            ok( scalar( @as ), 3);
            @as   = $doc2->getElementsByTagNameNS( "*", "A" );
            ok( scalar( @as ), 4);
            @as   = $doc2->getElementsByTagNameNS( "*", "*" );
            ok( scalar( @as ), 5);
            @as   = $doc2->getElementsByTagNameNS( "xml://D", "*" );
            ok( scalar( @as ), 2);

	    my $A = $doc2->getDocumentElement;
            @as   = $A->getChildrenByTagName( "A" );
	    ok( scalar( @as ), 1);
            @as   = $A->getChildrenByTagName( "C:A" );
	    ok( scalar( @as ), 1);
            @as   = $A->getChildrenByTagName( "C:B" );
	    ok( scalar( @as ), 0);
            @as   = $A->getChildrenByTagName( "*" );
	    ok( scalar( @as ), 2);
            @as   = $A->getChildrenByTagNameNS( "*", "A" );
	    ok( scalar( @as ), 2);
            @as   = $A->getChildrenByTagNameNS( "xml://D", "*" );
	    ok( scalar( @as ), 1);
            @as   = $A->getChildrenByTagNameNS( "*", "*" );
            ok( scalar( @as ), 2);
            @as   = $A->getChildrenByLocalName( "A" );
            ok( scalar( @as ), 2);
        }
    }
}
{
    print "# 5. Bug fixes (to be use with valgrind)\n";
    {  
       my $doc=XML::LibXML->createDocument(); # create a doc
       my $x=$doc->createPI(foo=>"bar");      # create a PI
       undef $doc;                            # should not free
       undef $x;                              # free the PI
       ok(1);
    }
    {  
       my $doc=XML::LibXML->createDocument(); # create a doc
       my $x=$doc->createAttribute(foo=>"bar"); # create an attribute
       undef $doc;                            # should not free
       undef $x;                              # free the attribute
       ok(1);
    }
    {  
       my $doc=XML::LibXML->createDocument(); # create a doc
       my $x=$doc->createAttributeNS(undef,foo=>"bar"); # create an attribute
       undef $doc;                            # should not free
       undef $x;                              # free the attribute
       ok(1);
    }
    {  
       my $doc=XML::LibXML->new->parse_string('<foo xmlns:x="http://foo.bar"/>');
       my $x=$doc->createAttributeNS('http://foo.bar','x:foo'=>"bar"); # create an attribute
       undef $doc;                            # should not free
       undef $x;                              # free the attribute
       ok(1);
    }

}