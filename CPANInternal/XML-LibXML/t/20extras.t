# $Id: 20extras.t,v 1.1.1.1 2004/05/20 17:55:25 jpetri Exp $

use Test;

BEGIN { plan tests => 12 };
use XML::LibXML;

my $string = "<foo><bar/></foo>";

my $parser = XML::LibXML->new();

{
    my $doc = $parser->parse_string( $string );
    ok($doc);
    local $XML::LibXML::skipXMLDeclaration = 1;
    ok( $doc->toString(), $string );
    local $XML::LibXML::setTagCompression = 1;
    ok( $doc->toString(), "<foo><bar></bar></foo>" );
}

{
    local $XML::LibXML::skipDTD = 1;
    $parser->expand_entities(0);
    my $doc = $parser->parse_file( "example/dtd.xml" );
    ok($doc);
    my $test = "<?xml version=\"1.0\"?>\n<doc>This is a valid document &foo; !</doc>\n";
    ok( $doc->toString, $test );
}

{
    my $doc = $parser->parse_string( $string );
    ok($doc);
    my $dclone = $doc->cloneNode(1); # deep
    ok( ! $dclone->isSameNode($doc) );
    ok( $dclone->getDocumentElement() );
    ok( $doc->toString() eq $dclone->toString() );

    my $clone = $doc->cloneNode(); # shallow
    ok( ! $clone->isSameNode($doc) );
    ok( ! $clone->getDocumentElement() );
    $doc->getDocumentElement()->unbindNode();
    ok( $doc->toString() eq $clone->toString() );
}
