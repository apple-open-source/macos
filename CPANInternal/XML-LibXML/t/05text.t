# $Id: 05text.t,v 1.1.1.2 2007/10/10 23:04:15 ahuda Exp $

##
# this test checks the DOM Characterdata interface of XML::LibXML

use Test;

BEGIN { plan tests => 32 };
use XML::LibXML;

my $doc = XML::LibXML::Document->new();

{
    print "# 1. creation\n";
    my $foo = "foobar";
    my $textnode = $doc->createTextNode($foo);
    ok( $textnode );
    ok( $textnode->nodeName(), '#text' );
    ok( $textnode->nodeValue(), $foo );

    print "# 2. substring\n";
    my $tnstr = $textnode->substringData( 1,2 );
    ok( $tnstr , "oo" );
    ok( $textnode->nodeValue(), $foo );

    print "# 3. Expansion\n";
    $textnode->appendData( $foo );
    ok( $textnode->nodeValue(), $foo . $foo );

    $textnode->insertData( 6, "FOO" );
    ok( $textnode->nodeValue(), $foo."FOO".$foo );

    $textnode->setData( $foo );
    $textnode->insertData( 6, "FOO" );
    ok( $textnode->nodeValue(), $foo."FOO" );
    $textnode->setData( $foo );
    $textnode->insertData( 3, "" );
    ok( $textnode->nodeValue(), $foo );

    print "# 4. Removement\n";
    $textnode->deleteData( 1,2 );
    ok( $textnode->nodeValue(), "fbar" );
    $textnode->setData( $foo );
    $textnode->deleteData( 1,10 );
    ok( $textnode->nodeValue(), "f" );
    $textnode->setData( $foo );
    $textnode->deleteData( 10,1 );
    ok( $textnode->nodeValue(), $foo );
    $textnode->deleteData( 1,0 );
    ok( $textnode->nodeValue(), $foo );
    $textnode->deleteData( 0,0 );
    ok( $textnode->nodeValue(), $foo );
    $textnode->deleteData( 0,2 );
    ok( $textnode->nodeValue(), "obar" );

    print "# 5. Replacement\n";
    $textnode->setData( "test" );
    $textnode->replaceData( 1,2, "phish" );
    ok( $textnode->nodeValue(), "tphisht" );
    $textnode->setData( "test" );
    $textnode->replaceData( 1,4, "phish" );
    ok( $textnode->nodeValue(), "tphish" );
    $textnode->setData( "test" );
    $textnode->replaceData( 1,0, "phish" );
    ok( $textnode->nodeValue(), "tphishest" );


    print "# 6. XML::LibXML features\n";
    $textnode->setData( "test" );

    $textnode->replaceDataString( "es", "new" );   
    ok( $textnode->nodeValue(), "tnewt" );

    $textnode->replaceDataRegEx( 'n(.)w', '$1s' );
    ok( $textnode->nodeValue(), "test" );

    $textnode->setData( "blue phish, white phish, no phish" );
    $textnode->replaceDataRegEx( 'phish', 'test' );
    ok( $textnode->nodeValue(), "blue test, white phish, no phish" );

    # replace them all!
    $textnode->replaceDataRegEx( 'phish', 'test', 'g' );
    ok( $textnode->nodeValue(), "blue test, white test, no test" );

    # check if special chars are encoded properly 
    $textnode->setData( "te?st" );
    $textnode->replaceDataString( "e?s", 'ne\w' );   
    ok( $textnode->nodeValue(), 'tne\wt' );

    # check if entities don't get translated
    $textnode->setData(q(foo&amp;bar));
    ok ( $textnode->getData eq q(foo&amp;bar) );
}

{
    print "# standalone test\n";
    my $node = XML::LibXML::Text->new("foo");
    ok($node);
    ok($node->nodeValue, "foo" );
}

{
    print "# CDATA node name test\n";

    my $node = XML::LibXML::CDATASection->new("test");

    ok( $node->string_value(), "test" );
    ok( $node->nodeName(), "#cdata-section" );
}

{
    print "# Comment node name test\n";

    my $node = XML::LibXML::Comment->new("test");

    ok( $node->string_value(), "test" );
    ok( $node->nodeName(), "#comment" );
}

{
    print "# Document node name test\n";

    my $node = XML::LibXML::Document->new();

    ok( $node->nodeName(), "#document" );
}
{
    print "# Document fragment node name test\n";

    my $node = XML::LibXML::DocumentFragment->new();

    ok( $node->nodeName(), "#document-fragment" );
}