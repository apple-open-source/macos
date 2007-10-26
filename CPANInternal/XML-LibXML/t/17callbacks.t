use Test;
BEGIN { plan tests => 38}
END { ok(0) unless $loaded }
use XML::LibXML;
use IO::File;
$loaded = 1;
ok(1);

my $using_globals = '';

my $parser = XML::LibXML->new();
ok($parser);

$parser->match_callback( \&match );
$parser->read_callback( \&read );
$parser->open_callback( \&open );
$parser->close_callback( \&close );

$parser->expand_xinclude( 1 );

$dom = $parser->parse_file("example/test.xml");

ok($dom);

#warn $dom->toString();

my $root = $dom->getDocumentElement();

my @nodes = $root->findnodes( 'xml/xsl' );
ok( scalar @nodes );

chdir("example/complex") || die "chdir: $!";
open(F, "complex.xml") || die "Cannot open complex.xml: $!";
local $/;
my $str = <F>;
close F;

my $parser2 = XML::LibXML->new();
$parser2->expand_xinclude( 1 );
$dom = $parser2->parse_string($str);
ok($dom);

# warn $dom->toString() , "\n";

$using_globals = 1;
$XML::LibXML::match_cb = \&match;
$XML::LibXML::open_cb = \&open;
$XML::LibXML::read_cb = \&read;
$XML::LibXML::close_cb = \&close;

ok($parser->parse_string($str));

# warn $dom->toString() , "\n";

sub match {
# warn "match: $_[0]\n";
    ok($using_globals, defined($XML::LibXML::match_cb));
    return 1;
}

sub close {
# warn "close $_[0]\n";
    ok($using_globals, defined($XML::LibXML::close_cb));
    if ( $_[0] ) {
        $_[0]->close();
    }
    return 1;
}

sub open {
 #warn("open: $_[0]\n");
    $file = new IO::File;
    if ( $file->open( "<$_[0]" ) ){
#        warn "open!\n";
        ok($using_globals, defined($XML::LibXML::open_cb));
    }
    else {
#        warn "cannot open $_[0] $!\n";
        $file = 0;
    }   
# warn("opened $file\n");
   
    return $file;
}

sub read {
#    warn "read!";
    my $rv = undef;
    my $n = 0;
    if ( $_[0] ) {
#        warn "read $_[1] bytes!\n";
        $n = $_[0]->read( $rv , $_[1] );
#        warn "read!" if $n > 0;
        ok($using_globals, defined($XML::LibXML::read_cb)) if $n > 0
    }
    return $rv;
}
