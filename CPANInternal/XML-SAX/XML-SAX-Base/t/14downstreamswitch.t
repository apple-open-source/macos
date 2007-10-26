use Test;
BEGIN { plan tests => 3 }
use XML::SAX::Base;
use strict;
use vars qw/%events $meth_count $one_count $two_count/;
require "t/events.pl";

$meth_count = 0;
# Tests for in-stream switch of Handler classes.

my $handler = HandlerOne->new();
my $filter  = Filter->new(DocumentHandler => $handler);
my $driver  = Driver->new(DocumentHandler => $filter);
$driver->parse();


ok($one_count == 3);
ok($two_count == 3);
ok($meth_count == 6);

# end main

package Filter;
use base qw(XML::SAX::Base);

sub start_element {
    my ($self, $element) = @_;
    if ($main::meth_count == 3) {
        $self->set_content_handler(HandlerTwo->new());
    }
    $self->SUPER::start_element($element);
}


1;

package HandlerOne;
use base qw(XML::SAX::Base);

sub start_element {
    my ($self, $element) = @_;
    $main::meth_count++;
    $main::one_count++;
    
}


1;

package HandlerTwo;
use base qw(XML::SAX::Base);

sub start_element {
    my ($self, $element) = @_;
    $main::meth_count++;
    $main::two_count++;
}

1;


package Driver;
use base qw(XML::SAX::Base);

sub parse {
    my $self = shift;
    my %events = %main::events;
 
    $self->SUPER::start_element($events{start_element});
    $self->SUPER::start_element($events{start_element});
    $self->SUPER::start_element($events{start_element});
    $self->SUPER::start_element($events{start_element});
    $self->SUPER::start_element($events{start_element});
    $self->SUPER::start_element($events{start_element});

}
1;

