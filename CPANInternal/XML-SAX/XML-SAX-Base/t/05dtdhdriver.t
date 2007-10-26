use Test;
BEGIN { plan tests => 7 }
use XML::SAX::Base;
use strict;
use vars qw/%events $meth_count/;
require "t/events.pl";

# Tests for ContentHandler classes using a filter

my $sax_it = SAXAutoload->new();
my $filter = Filter->new(DTDHandler => $sax_it);
my $driver = Driver->new(DTDHandler => $filter);
$driver->_parse();

ok($meth_count == 6);

# end main

package Filter;
use base qw(XML::SAX::Base);
# this space intentionally blank

1;

package Driver;
use base qw(XML::SAX::Base);

sub _parse {
    my $self = shift;
    my %events = %main::events;
 
    $self->SUPER::notation_decl($events{notation_decl});
    $self->SUPER::unparsed_entity_decl($events{unparsed_entity_decl});
    $self->SUPER::xml_decl($events{xml_decl});
    $self->SUPER::attlist_decl($events{attlist_decl});
    $self->SUPER::doctype_decl($events{doctype_decl});
    $self->SUPER::entity_decl($events{entity_decl});

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
