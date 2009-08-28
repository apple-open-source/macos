use Test;

BEGIN { plan tests => 52 }

use XML::LibXML;
use XML::LibXML::SAX;
use XML::LibXML::SAX::Parser;
use XML::LibXML::SAX::Builder;
use XML::SAX;
use IO::File;
ok(1);

ok(XML::SAX->add_parser(q(XML::LibXML::SAX::Parser)));

local $XML::SAX::ParserPackage = 'XML::LibXML::SAX::Parser';

my $sax = SAXTester->new;
ok($sax);

my $str = join('', IO::File->new("example/dromeds.xml")->getlines);
my $doc = XML::LibXML->new->parse_string($str);
ok($doc);

my $generator = XML::LibXML::SAX::Parser->new(Handler => $sax);
ok($generator);

$generator->generate($doc);

my $builder = XML::LibXML::SAX::Builder->new();
ok($builder);
my $gen2 = XML::LibXML::SAX::Parser->new(Handler => $builder);
my $dom2 = $gen2->generate($doc);
ok($dom2);

ok($dom2->toString, $str);
# warn($dom2->toString);

########### XML::SAX Tests ###########
my $parser = XML::SAX::ParserFactory->parser(Handler => $sax);
ok($parser);
$parser->parse_uri("example/dromeds.xml");

$parser->parse_string(<<EOT);
<?xml version='1.0' encoding="US-ASCII"?>
<dromedaries one="1" />
EOT

$sax = SAXNSTester->new;
ok($sax);

$parser->set_handler($sax);

$parser->parse_uri("example/ns.xml");

########### Namespace test ( empty namespaces ) ########

{
    my $h = "SAXNS2Tester";
    my $xml = "<a xmlns='xml://A'><b/></a>";
    my @tests = (
sub {
    XML::LibXML::SAX        ->new( Handler => $h )->parse_string( $xml );
},

sub {
    XML::LibXML::SAX::Parser->new( Handler => $h )->parse_string( $xml );
},
);  
    
    $_->() for @tests;

    
}


########### Error Handling ###########
{
  my $xml = '<a>Text</b>';

  my $handler = SAXErrorTester->new;

  foreach my $pkg (qw(XML::LibXML::SAX::Parser XML::LibXML::SAX)) {
    undef $@;
    eval {
      $pkg->new(Handler => $handler)->parse_string($xml);
    };
    ok($@); # We got an error
  }
  
  $handler = SAXErrorCallbackTester->new;
  eval { XML::LibXML::SAX->new(Handler => $handler )->parse_string($xml) };
  ok($@); # We got an error
  ok( $handler->{fatal_called} );

}

########### Helper class #############

package SAXTester;
use Test;

sub new {
    my $class = shift;
    return bless {}, $class;
}

sub start_document {
  ok(1);
}

sub end_document {
  ok(1);
}

sub start_element {
  my ($self, $el) = @_;
  ok($el->{LocalName}, qr{^(dromedaries|species|humps|disposition|legs)$});
  foreach my $attr (keys %{$el->{Attributes}}) {
    # warn("Attr: $attr = $el->{Attributes}->{$attr}\n");
  }
# warn("start_element: $el->{Name}\n");
}

sub end_element {
  my ($self, $el) = @_;
  # warn("end_element: $el->{Name}\n");
}

sub characters {
  my ($self, $chars) = @_;
  # warn("characters: $chars->{Data}\n");
}

1;

package SAXNSTester;
use Test;

sub new {
    bless {}, shift;
}

sub start_element {
    my ($self, $node) = @_;
    ok($node->{NamespaceURI} =~ /^urn:/);
    # warn("start_element:\n", Dumper($node));
}

sub end_element {
    my ($self, $node) = @_;
    # warn("end_element: $node->{Name}\n");
}

sub start_prefix_mapping {
    my ($self, $node) = @_;
    ok($node->{NamespaceURI} =~ /^(urn:camels|urn:mammals|urn:a)$/);
    # warn("start_prefix_mapping:\n", Dumper($node));
}

sub end_prefix_mapping {
    my ($self, $node) = @_;
    # warn("end_prefix_mapping:\n", Dumper($node));
    ok($node->{NamespaceURI} =~ /^(urn:camels|urn:mammals|urn:a)$/);
}

1;

package SAXNS2Tester;
use Test;

#sub new {
#    my $class = shift;
#    return bless {}, $class;
#}

sub start_element {
    my $self = shift;
    my ( $elt ) = @_;
    ok $elt->{NamespaceURI} eq "xml://A"
        if $elt->{Name} eq "b"
}

1;

package SAXErrorTester;
use Test;

sub new {
    bless {}, shift;
}

sub end_document {
    print "End doc: @_\n";
    return 1; # Shouldn't be reached
}

package SAXErrorCallbackTester;
use Test;

sub fatal_error {
    $_[0]->{fatal_called} = 1;
}

sub new {
    bless {}, shift;
}

sub end_document {
    print "End doc: @_\n";
    return 1; # Shouldn't be reached
}


1;
