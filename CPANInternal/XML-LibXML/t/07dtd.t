# $Id: 07dtd.t,v 1.1.1.1 2004/05/20 17:55:25 jpetri Exp $

use Test;

BEGIN { plan tests => 22 };
use XML::LibXML;
use XML::LibXML::Common qw(:libxml);

{
    my $doc = XML::LibXML::Document->new;
    my $dtd = $doc->createExternalSubset( "html",
                                          "-//W3C//DTD XHTML 1.0 Transitional//EN",
                                          "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"
                                        );

    ok( $dtd->isSameNode(  $doc->externalSubset ) );

}

{
    my $doc = XML::LibXML::Document->new;
    my $dtd = $doc->createInternalSubset( "html",
                                          "-//W3C//DTD XHTML 1.0 Transitional//EN",
                                          "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"
                                        );
    ok( $dtd->isSameNode( $doc->internalSubset ) );

    $doc->setExternalSubset( $dtd );
    ok(not defined $doc->internalSubset );
    ok( $dtd->isSameNode( $doc->externalSubset ) );

    $doc->setInternalSubset( $dtd );
    ok(not defined  $doc->externalSubset );
    ok( $dtd->isSameNode( $doc->internalSubset ) );

    my $dtd2 = $doc->createDTD( "huhu",
                                "-//W3C//DTD XHTML 1.0 Transitional//EN",
                                "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"
                              );

    $doc->setInternalSubset( $dtd2 );
    ok(not defined $dtd->parentNode );
    ok( $dtd2->isSameNode( $doc->internalSubset ) );    

 
    my $dtd3 = $doc->removeInternalSubset;
    ok( $dtd3->isSameNode($dtd2) );
    ok(not defined $doc->internalSubset );

    $doc->setExternalSubset( $dtd2 );

    $dtd3 = $doc->removeExternalSubset;
    ok( $dtd3->isSameNode($dtd2) );
    ok(not defined $doc->externalSubset );
}

{
    my $parser = XML::LibXML->new();

    my $doc = $parser->parse_file( "example/dtd.xml" );
    
    ok($doc);
 
    my $entity = $doc->createEntityReference( "foo" );
    ok($entity);
    ok($entity->nodeType, XML_ENTITY_REF_NODE );
 
    ok( $entity->hasChildNodes );
    ok( $entity->firstChild->nodeType, XML_ENTITY_DECL );
    ok( $entity->firstChild->nodeValue, " test " );

    my $edcl = $entity->firstChild;
    ok( $edcl->previousSibling->nodeType, XML_ELEMENT_DECL );

    {
        my $doc2  = XML::LibXML::Document->new;
        my $e = $doc2->createElement("foo");
        $doc2->setDocumentElement( $e );

        my $dtd2 = $doc->internalSubset->cloneNode(1);
        ok($dtd2);

#        $doc2->setInternalSubset( $dtd2 );
#        warn $doc2->toString;

#        $e->appendChild( $entity );
#        warn $doc2->toString;
    }
}


{
    my $parser = XML::LibXML->new();
    $parser->validation(1);
    $parser->keep_blanks(1);
    my $doc=$parser->parse_string(<<'EOF');
<?xml version='1.0'?>
<!DOCTYPE test [
 <!ELEMENT test (#PCDATA)>
]>
<test>
</test>
EOF
ok($doc->validate());
ok($doc->is_valid());

}
