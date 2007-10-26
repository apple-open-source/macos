package SOAP::Transport::HTTP::Daemon::ForkAfterProcessing;

use strict;
use vars qw(@ISA);
use SOAP::Transport::HTTP;

# Idea and implementation of Peter Fraenkel (Peter.Fraenkel@msdw.com)

@ISA = qw(SOAP::Transport::HTTP::Daemon);

sub handle {
  my $self = shift->new;
 CLIENT:
  while (my $c = $self->accept) {
    my $first = 1;
    while (my $r = $c->get_request) {
      $self->request($r);
      $self->SOAP::Transport::HTTP::Server::handle;
      if ($first && fork) { $first=0; $c->close; next CLIENT }
      $c->send_response($self->response)
    }
    $c->close;
    undef $c;
  }
}

1;
