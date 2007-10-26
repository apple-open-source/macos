use Test;
BEGIN { plan tests => 46 };
END {ok(0) unless $loaded;}
use XML::LibXML;
$loaded = 1;
ok($loaded);

# to test if findnodes works.
# i added findnodes to the node class, so a query can be started
# everywhere.

my $file    = "example/dromeds.xml";

# init the file parser
my $parser = XML::LibXML->new();
$dom    = $parser->parse_file( $file );

if ( defined $dom ) {
    # get the root document
    $elem   = $dom->getDocumentElement();
  
    # first very simple path starting at root
    my @list   = $elem->findnodes( "species" );
    ok( scalar(@list), 3 );

    # a simple query starting somewhere ...
    my $node = $list[0];
    my @slist = $node->findnodes( "humps" );
    ok( scalar(@slist), 1 );

    # find a single node
    @list   = $elem->findnodes( "species[\@name='Llama']" );
    ok( scalar( @list ), 1 );
  
    # find with not conditions
    @list   = $elem->findnodes( "species[\@name!='Llama']/disposition" );
    ok( scalar(@list), 2 );


    @list   = $elem->findnodes( 'species/@name' );
    # warn $elem->toString();

    ok( scalar @list && $list[0]->toString() eq ' name="Camel"' );

    my $x = XML::LibXML::Text->new( 1234 );
    if( defined $x ) {
        ok( $x->getData(), "1234" );
    }
    
    my $telem = $dom->createElement('test');
    $telem->appendWellBalancedChunk('<b>c</b>');
  
    finddoc($dom);
    ok(1);
}
ok( $dom );

# test to make sure that multiple array findnodes() returns
# don't segfault perl; it'll happen after the second one if it does
for (0..3) {
    my $doc = XML::LibXML->new->parse_string(
'<?xml version="1.0" encoding="UTF-8"?>
<?xsl-stylesheet type="text/xsl" href="a.xsl"?>
<a />');
    my @nds = $doc->findnodes("processing-instruction('xsl-stylesheet')");
}

$doc = $parser->parse_string(<<'EOT');
<a:foo xmlns:a="http://foo.com" xmlns:b="http://bar.com">
 <b:bar>
  <a:foo xmlns:a="http://other.com"/>
 </b:bar>
</a:foo>
EOT

my $root = $doc->getDocumentElement;
my @a = $root->findnodes('//a:foo');
ok(@a, 1);

@b = $root->findnodes('//b:bar');
ok(@b, 1);

@none = $root->findnodes('//b:foo');
@none = (@none, $root->findnodes('//foo'));
ok(@none, 0);

my @doc = $root->findnodes('document("example/test.xml")');
ok(@doc);
# warn($doc[0]->toString);

# this query should result an empty array!
my @nodes = $root->findnodes( "/humpty/dumpty" );
ok( scalar(@nodes), 0 );


my $docstring = q{
<foo xmlns="http://kungfoo" xmlns:bar="http://foo"/>
};
 $doc = $parser->parse_string( $docstring );
 $root = $doc->documentElement;

my @ns = $root->findnodes('namespace::*');
ok(scalar(@ns), 2 );

print "#bad xpaths\n";

my @badxpath = (
    'abc:::def',
    'foo///bar',
    '...',
    '/-',
               );

foreach my $xp ( @badxpath ) {
    eval { $res = $root->findnodes( $xp ); };
    ok($@);
    eval { $res = $root->find( $xp ); };
    ok($@);
    eval { $res = $root->findvalue( $xp ); };
    ok($@);
    eval { $res = $root->findnodes( encodeToUTF8( "iso-8859-1", $xp ) ); };
    ok($@);
    eval { $res = $root->find( encodeToUTF8( "iso-8859-1", $xp ) );};
    ok($@);
}


{
    # as reported by jian lou:
    # 1. getElementByTagName("myTag") is not working is
    # "myTag" is a node directly under root. Same problem
    # for findNodes("//myTag")
    # 2. When I add new nodes into DOM tree by
    # appendChild(). Then try to find them by
    # getElementByTagName("newNodeTag"), the newly created
    # nodes are not returned. ...
    #
    # this seems not to be a problem by XML::LibXML itself, but newer versions
    # of libxml2 (newer is 2.4.27 or later)
    #
    my $doc = XML::LibXML->createDocument();
    my $root= $doc->createElement( "A" );
    $doc->setDocumentElement($root);

    my $b= $doc->createElement( "B" );
    $root->appendChild( $b );

    my @list = $doc->findnodes( '//A' );
    ok( scalar @list );
    ok( $list[0]->isSameNode( $root ) );

    @list = $doc->findnodes( '//B' );
    ok( scalar @list );
    ok( $list[0]->isSameNode( $b ) );


    # @list = $doc->getElementsByTagName( "A" );
    # ok( scalar @list );
    # ok( $list[0]->isSameNode( $root ) );        

    @list = $root->getElementsByTagName( 'B' );
    ok( scalar @list );
    ok( $list[0]->isSameNode( $b ) );
}

{
    print "# test potential unbinding-segfault-problem \n";
    my $doc = XML::LibXML->createDocument();
    my $root= $doc->createElement( "A" );
    $doc->setDocumentElement($root);

    my $b= $doc->createElement( "B" );
    $root->appendChild( $b );
    my $c= $doc->createElement( "C" );
    $b->appendChild( $c );
    $b= $doc->createElement( "B" );
    $root->appendChild( $b );
    $c= $doc->createElement( "C" );
    $b->appendChild( $c );
    
    my @list = $root->findnodes( "B" );
    ok( scalar(@list) , 2 );
    foreach my $node ( @list ) {
        my @subnodes = $node->findnodes( "C" );
        $node->unbindNode() if ( scalar( @subnodes ) );
        ok(1);
    }
}

{
    print "# findnode remove problem\n";

    my $xmlstr = "<a><b><c>1</c><c>2</c></b></a>";
    
    my $doc       = $parser->parse_string( $xmlstr );
    my $root      = $doc->documentElement;
    my ( $lastc ) = $root->findnodes( 'b/c[last()]' );
    ok( $lastc );

    $root->removeChild( $lastc );
    ok( $root->toString(), $xmlstr );
}

# --------------------------------------------------------------------------- #
sub finddoc {
    my $doc = shift;
    return unless defined $doc;
    my $rn = $doc->documentElement;
    $rn->findnodes("/");
}
