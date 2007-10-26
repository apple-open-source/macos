use Test;
BEGIN { plan tests => 31 }
use XML::SAX::PurePerl;
use XML::SAX::PurePerl::DebugHandler;

my $handler = TestElementHandler->new();
ok($handler);

my $parser = XML::SAX::PurePerl->new(Handler => $handler);
ok($parser);

$handler->{test_el}{Name} = "foo";
$handler->{test_el}{LocalName} = "foo";
$handler->{test_el}{Prefix} = "";
$handler->{test_el}{NamespaceURI} = "";
$parser->parse_uri("testfiles/06a.xml");

$handler->reset;

$handler->{attr_name} = "{}a";
$handler->{test_attr}{Name} = "a";
$handler->{test_attr}{Value} = "1";
$handler->{test_attr}{Prefix} = "";
$handler->{test_attr}{LocalName} = "a";
$handler->{test_attr}{NamespaceURI} = "";
$parser->parse_uri("testfiles/06b.xml");

$handler->reset;

$handler->{test_el}{Name} = "foo";
$handler->{test_el}{LocalName} = "foo";
$handler->{test_el}{Prefix} = "";
$handler->{test_el}{NamespaceURI} = "http://foo.com";
$parser->parse_uri("testfiles/06c.xml");

$handler->reset;
$handler->{test_el}{Name} = "x:foo";
$handler->{test_el}{LocalName} = "foo";
$handler->{test_el}{Prefix} = "x";
$handler->{test_el}{NamespaceURI} = "http://foo.com";
$handler->{attr_name} = "{http://foo.com}a";
$handler->{test_attr}{Name} = "x:a";
$handler->{test_attr}{Value} = "2";
$handler->{test_attr}{Prefix} = "x";
$handler->{test_attr}{LocalName} = "a";
$handler->{test_attr}{NamespaceURI} = "http://foo.com";
$parser->parse_uri("testfiles/06d.xml");

$handler->reset;
$handler->{test_el}{Name} = "foo";
$handler->{test_el}{LocalName} = "foo";
$handler->{test_el}{Prefix} = "";
$handler->{test_el}{NamespaceURI} = "http://foo.com";
$handler->{attr_name} = "{}a";
$handler->{test_attr}{Name} = "a";
$handler->{test_attr}{NamespaceURI} = "";
$parser->parse_uri("testfiles/06e.xml");

$handler->reset;
# prefix with no ns binding. Error.
eval {
$parser->parse_uri("testfiles/06f.xml");
};
ok($@);

## HELPER PACKAGE ##

package TestElementHandler;

use Test;

sub new {
    my $class = shift;
    my %opts = @_;
    bless \%opts, $class;
}

sub reset {
    my $self = shift;
    %$self = ();
}

sub start_element {
    my ($self, $el) = @_;
    if ($self->{test_el}) {
        foreach my $key (keys %{ $self->{test_el} }) {
            #warn $key, "\n";
            ok("$key: $el->{$key}", "$key: $self->{test_el}{$key}");
        }
    }
    if ($self->{test_attr}) {
        foreach my $key (keys %{ $self->{test_attr} }) {
            #warn $key, "\n";
            ok("$key: $el->{Attributes}{ $self->{attr_name} }{$key}", "$key: $self->{test_attr}{$key}");
        }
    }
}

