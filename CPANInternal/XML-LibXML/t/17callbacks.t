# $Id: 17callbacks.t,v 1.1.1.2 2007/10/10 23:04:15 ahuda Exp $
use Test;
BEGIN { plan tests => 43 }
END { ok(0) unless $loaded }
use XML::LibXML;
use IO::File;
$loaded = 1;
ok(1);


my $using_globals = '';

{
    # first test checks if local callbacks work
    my $parser = XML::LibXML->new();
    ok($parser);

    $parser->match_callback( \&match1 );
    $parser->read_callback( \&read1 );
    $parser->open_callback( \&open1 );
    $parser->close_callback( \&close1 );

    $parser->expand_xinclude( 1 );

    $dom = $parser->parse_file("example/test.xml");

    ok($dom);
    # warn $dom->toString();

    my $root = $dom->getDocumentElement();

    my @nodes = $root->findnodes( 'xml/xsl' );
    ok( scalar @nodes );
}

{
    # test per parser callbacks. These tests must not fail!
    
    my $parser = XML::LibXML->new();
    my $parser2 = XML::LibXML->new();

    ok($parser);
    ok($parser2);

    $parser->match_callback( \&match1 );
    $parser->read_callback( \&read1 );
    $parser->open_callback( \&open1 );
    $parser->close_callback( \&close1 );

    $parser->expand_xinclude( 1 );

    $parser2->match_callback( \&match2 );
    $parser2->read_callback( \&read2 );
    $parser2->open_callback( \&open2 );
    $parser2->close_callback( \&close2 );

    $parser2->expand_xinclude( 1 );

   
    my $dom1 = $parser->parse_file( "example/test.xml");
    my $dom2 = $parser2->parse_file("example/test.xml");

    ok($dom1);
    ok($dom2);

    my $val1  = ( $dom1->findnodes( "/x/xml/text()") )[0]->string_value();
    my $val2  = ( $dom2->findnodes( "/x/xml/text()") )[0]->string_value();

    $val1 =~ s/^\s*|\s*$//g;
    $val2 =~ s/^\s*|\s*$//g;

    ok( $val1, "test" );
    ok( $val2, "test 4" );
}

chdir("example/complex") || die "chdir: $!";
open(F, "complex.xml") || die "Cannot open complex.xml: $!";
local $/;
my $str = <F>;
close F;

{
    # tests if callbacks are called correctly within DTDs
    my $parser2 = XML::LibXML->new();
    $parser2->expand_xinclude( 1 );
    $dom = $parser2->parse_string($str);
    ok($dom);
}



$using_globals = 1;
$XML::LibXML::match_cb = \&match1;
$XML::LibXML::open_cb  = \&open1;
$XML::LibXML::read_cb  = \&read1;
$XML::LibXML::close_cb = \&close1;

{
    # tests if global callbacks are working
    my $parser = XML::LibXML->new();
    ok($parser);

    ok($parser->parse_string($str));

    # warn $dom->toString() , "\n";
}


sub match1 {
    # warn "match: $_[0]\n";
    ok($using_globals, defined($XML::LibXML::match_cb));
    return 1;
}

sub close1 {
    # warn "close $_[0]\n";
    ok($using_globals, defined($XML::LibXML::close_cb));
    if ( $_[0] ) {
        $_[0]->close();
    }
    return 1;
}

sub open1 {
    my $f = shift;
    # warn("open: $f\n");
    $file = new IO::File;
    if ( $file->open( "<$f" ) ){
        # warn "open file";
        ok($using_globals, defined($XML::LibXML::open_cb));
    }
    else {
        # warn "cannot open file";
        $file = 0;
    }   
    return $file;
}

sub read1 {
    # warn "read!";
    my $rv = undef;
    my $n = 0;
    if ( $_[0] ) {
        $n = $_[0]->read( $rv , $_[1] );
        ok($using_globals, defined($XML::LibXML::read_cb)) if $n > 0
    }
    return $rv;
}

sub match2 {
    # warn "match2: $_[0]\n";
    return 1;
}

sub close2 {
    # warn "close2 $_[0]\n";
    if ( $_[0] ) {
        $_[0]->close();
    }
    return 1;
}

sub open2 {
    # warn("open2: $_[0]\n");
    $file = new IO::File;
    my $fn = $_[0];
    $fn    =~ s/([^\d])(\.xml)$/${1}4$2/; # use a different file
    if ( $file->open( "<$fn" ) ){
        ok(1);
    }
    else {
        ok(0);
        $file = 0;
    }   
    # warn("opened $file\n");
   
    return $file;
}

sub read2 {
    # warn "read2!";
    my $rv = undef;
    my $n = 0;
    if ( $_[0] ) {
        $n = $_[0]->read( $rv , $_[1] );
        # warn "read!" if $n > 0;
    }
    return $rv;
}

