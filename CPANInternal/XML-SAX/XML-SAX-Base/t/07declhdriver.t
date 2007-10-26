use Test;
BEGIN { plan tests => 5 }
use XML::SAX::Base;
use strict;
use vars qw/%events $meth_count/;
require "t/events.pl";

# Tests for ContentHandler classes using a filter

my $sax_it = SAXAutoload->new();
my $filter = Filter->new(DeclHandler => $sax_it);
my $driver = Driver->new(DeclHandler => $filter);
$driver->parse();

ok($meth_count == 4);

# end main

package Filter;
use base qw(XML::SAX::Base);
# this space intentionally blank

1;

package Driver;
use base qw(XML::SAX::Base);

sub parse {
    my $self = shift;
    my %events = %main::events;
 
    $self->SUPER::element_decl($events{element_decl});
    $self->SUPER::attribute_decl($events{attribute_decl});
    $self->SUPER::internal_entity_decl($events{internal_entity_decl});
    $self->SUPER::external_entity_decl($events{external_entity_decl});

#    return $self->SUPER::result(1);
}
1;

# basic single class SAX Handler
package SAXAutoload;
use vars qw($AUTOLOAD);
use strict;

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my %options = @_;
    $options{methods} = {};
    return bless \%options, $class;
}

sub AUTOLOAD {
    my $self = shift;
    my $data = shift;
    my $name = $AUTOLOAD;
    $name =~ s/.*://;   # strip fully-qualified portion
    return if $name eq 'DESTROY';
    #warn "name is $name \ndata is $data\n";
    my $okay_count = 0;
    foreach my $key (keys (%{$data})) {
       $okay_count++ if defined $main::events{$name}->{$key};
    }
    #warn "count $okay_count \n";
    main::ok($okay_count == scalar (keys (%{$data})));
    $main::meth_count++;
}
1;
