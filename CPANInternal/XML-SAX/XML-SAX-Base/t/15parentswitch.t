use Test;
BEGIN { plan tests => 3 }
use XML::SAX::Base;
use strict;
use vars qw/%events $meth_count $one_count $two_count/;
require "t/events.pl";

# Tests for in-stream switch of Handler classes.
my $driver = Driver->new();
my $handler = HandlerOne->new(Parent => $driver);
$driver->set_document_handler($handler);

$driver->parse();


ok($one_count == 3);
ok($two_count == 3);
ok($meth_count == 6);

# end main

package HandlerOne;
use base qw(XML::SAX::Base);

sub start_element {
    my ($self, $element) = @_;
    $main::meth_count++;
    $main::one_count++;
    if ($main::meth_count == 3) {
        $self->{Parent}->set_content_handler(HandlerTwo->new(Parent => $self->{Parent}));
    }
    
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

