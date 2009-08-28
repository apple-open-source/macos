# $Id: 07dtd.t,v 1.1.1.2 2007/10/10 23:04:15 ahuda Exp $

use Test;

BEGIN { plan tests => 38 };
use XML::LibXML;
use XML::LibXML::Common qw(:libxml);

my $htmlPublic = "-//W3C//DTD XHTML 1.0 Transitional//EN";
my $htmlSystem = "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd";

{
    my $doc = XML::LibXML::Document->new;
    my $dtd = $doc->createExternalSubset( "html",
                                          $htmlPublic,
                                          $htmlSystem
                                        );

    ok( $dtd->isSameNode(  $doc->externalSubset ) );
    ok( $dtd->publicId, $htmlPublic );
    ok( $dtd->systemId, $htmlSystem );
    ok( $dtd->getName, 'html' );
    
}

{
    my $doc = XML::LibXML::Document->new;
    my $dtd = $doc->createInternalSubset( "html",
                                          $htmlPublic,
                                          $htmlSystem
                                        );
    ok( $dtd->isSameNode( $doc->internalSubset ) );

    $doc->setExternalSubset( $dtd );
    ok(not defined $doc->internalSubset );
    ok( $dtd->isSameNode( $doc->externalSubset ) );

    ok( $dtd->getPublicId, $htmlPublic );
    ok( $dtd->getSystemId, $htmlSystem );

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
    
    my $dtd = $doc->internalSubset;
    ok( $dtd->getName, 'doc' );
    ok( $dtd->publicId, undef );
    ok( $dtd->systemId, undef );

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
{

my $parser = XML::LibXML->new();
$parser->validation(0);
$parser->load_ext_dtd(0); # This should make libxml not try to get the DTD

my $xml = '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://localhost/does_not_exist.dtd">
<html xmlns="http://www.w3.org/1999/xhtml"><head><title>foo</title></head><body><p>bar</p></body></html>';
my $doc = eval {
    $parser->parse_string($xml);
};

ok(!$@);
if ($@) {
    warn "Parsing error: $@\n";
}
ok($doc);

}
{
  my $bad = 'example/bad.dtd';
  ok( -f $bad );
  eval { XML::LibXML::Dtd->new("-//Foo//Test DTD 1.0//EN", 'example/bad.dtd') };
  ok ($@);

  undef $@;
  my $dtd;
  {
    local $/;
    open my $f, '<', $bad; 
    $dtd = <$f>;
  }
  ok( length($dtd) > 5 );
  eval { XML::LibXML::Dtd->parse_string($dtd) };
  ok ($@);

  my $xml = "<!DOCTYPE test SYSTEM \"example/bad.dtd\">\n<test/>";

  {	    
    my $parser = XML::LibXML->new;
    $parser->load_ext_dtd(0);
    $parser->validation(0);
    my $doc = $parser->parse_string($xml);
    ok( $doc );
  }
  {
    my $parser = XML::LibXML->new;
    $parser->load_ext_dtd(1);
    $parser->validation(0);
    undef $@;
    eval { $parser->parse_string($xml) };
    ok( $@ );
  }
}
