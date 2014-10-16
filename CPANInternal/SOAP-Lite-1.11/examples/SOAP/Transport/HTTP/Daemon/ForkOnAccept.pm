package SOAP::Transport::HTTP::Daemon::ForkOnAccept;

use strict;
use vars qw(@ISA);
use SOAP::Transport::HTTP;

# Idea and implementation of Michael Douglass

@ISA = qw(SOAP::Transport::HTTP::Daemon);

sub handle {
  my $self = shift->new;

  CLIENT:
  while (my $c = $self->accept) {
    my $pid = fork();

    # We are going to close the new connection on one of two conditions
    #  1. The fork failed ($pid is undefined)
    #  2. We are the parent ($pid != 0)
    unless( defined $pid && $pid == 0 ) {
      $c->close;
      next;
    }
    # From this point on, we are the child.

    $self->close;  # Close the listening socket (always done in children)

    # Handle requests as they come in
    while (my $r = $c->get_request) {
      $self->request($r);
      $self->SOAP::Transport::HTTP::Server::handle;
      $c->send_response($self->response);
    }
    $c->close;
    return;
  }
}

1;
