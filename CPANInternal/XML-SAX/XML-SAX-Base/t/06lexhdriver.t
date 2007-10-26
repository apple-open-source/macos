use Test;
BEGIN { plan tests => 8 }
use XML::SAX::Base;
use strict;
use vars qw/%events $meth_count/;
require "t/events.pl";

# Tests for ContentHandler classes using a filter

my $sax_it = SAXAutoload->new();
my $filter = Filter->new(LexicalHandler => $sax_it);
my $driver = Driver->new(LexicalHandler => $filter);
$driver->parse();

ok($meth_count == 7);

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
 
    $self->SUPER::comment($events{comment});
    $self->SUPER::start_dtd($events{start_dtd});
    $self->SUPER::end_dtd($events{end_dtd});
    $self->SUPER::start_cdata($events{start_cdata});
    $self->SUPER::end_cdata($events{end_cdata});
    $self->SUPER::start_entity($events{start_entity});
    $self->SUPER::end_entity($events{end_entity});

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
