use Test;
BEGIN { plan tests => 2 };

use XML::LibXML;

my $doc = XML::LibXML->createDocument;

my $t1 = $doc->createTextNode( "foo" );
my $t2 = $doc->createTextNode( "bar" );

$t1->addChild( $t2 );

eval {
    my $v = $t2->nodeValue;
};
ok($@);

ok(1); 