use Test;
BEGIN { plan tests => 10 }
use XML::SAX::Base;
use strict;
use vars qw/%events $meth_count/;
require "t/events.pl";

# Tests for DocumentHandler classes using a filter

my $sax_it = SAXAutoload->new();
my $filter = Filter->new(DocumentHandler => $sax_it);
my $driver = Driver->new(DocumentHandler => $filter);
$driver->parse();

ok($meth_count == 9);

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
 
    $self->SUPER::start_document($events{start_document});
    $self->SUPER::processing_instruction($events{processing_instruction});
    $self->SUPER::start_element($events{start_element});
    $self->SUPER::characters($events{characters});
    $self->SUPER::ignorable_whitespace($events{ignorable_whitespace});
    $self->SUPER::set_document_locator($events{set_document_locator});
    $self->SUPER::end_element($events{end_element});
    $self->SUPER::entity_reference($events{entity_reference});
    $self->SUPER::end_document($events{end_document});

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
