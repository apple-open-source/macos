# -*- perl -*-
#
#  Net::Server::Fork - Net::Server personality
#
#  $Id: Fork.pm,v 1.1 2004/04/19 17:50:29 dasenbro Exp $
#
#  Copyright (C) 2001, Paul T Seamons
#                      paul@seamons.com
#                      http://seamons.com/
#
#  Copyright (C) 2003-2004, Rob Brown bbb@cpan.org
#
#  This package may be distributed under the terms of either the
#  GNU General Public License
#    or the
#  Perl Artistic License
#
#  All rights reserved.
#
################################################################

package Net::Server::Fork;

use strict;
use vars qw($VERSION @ISA);
use Net::Server ();
use Net::Server::SIG qw(register_sig check_sigs);
use Socket qw(SO_TYPE SOL_SOCKET SOCK_DGRAM);
use POSIX qw(WNOHANG);

$VERSION = $Net::Server::VERSION; # done until separated

### fall back to parent methods
@ISA = qw(Net::Server);


### override-able options for this package
sub options {
  my $self = shift;
  my $prop = $self->{server};
  my $ref  = shift;

  $self->SUPER::options($ref);

  foreach ( qw(max_servers max_dequeue
               check_for_dead check_for_dequeue) ){
    $prop->{$_} = undef unless exists $prop->{$_};
    $ref->{$_} = \$prop->{$_};
  }
}

### make sure some defaults are set
sub post_configure {
  my $self = shift;
  my $prop = $self->{server};

  ### let the parent do the rest
  $self->SUPER::post_configure;

  ### what are the max number of processes
  $prop->{max_servers} = 256
    unless defined $prop->{max_servers};

  ### how often to see if children are alive
  ### only used when max_servers is reached
  $prop->{check_for_dead} = 60
    unless defined $prop->{check_for_dead};

  ### I need to know who is the parent
  $prop->{ppid} = $$;

  ### let the post bind set up a select handle for us
  $prop->{multi_port} = 1;

}


### loop, fork, and process connections
sub loop {
  my $self = shift;
  my $prop = $self->{server};

  ### get ready for children
  $prop->{children} = {};

  ### register some of the signals for safe handling
  register_sig(PIPE => 'IGNORE',
               INT  => sub { $self->server_close() },
               TERM => sub { $self->server_close() },
               QUIT => sub { $self->server_close() },
               HUP  => sub { $self->sig_hup() },
               CHLD => sub {
                 while ( defined(my $chld = waitpid(-1, WNOHANG)) ){
                   last unless $chld > 0;
                   $self->delete_child($chld);
                 }
               },
               );

  my ($last_checked_for_dead, $last_checked_for_dequeue) = (time(), time());

  ### this is the main loop
  while( 1 ){

    ### make sure we don't use too many processes
    my $n_children = grep { $_->{status} !~ /dequeue/ } (values %{ $prop->{children} });
    while ($n_children > $prop->{max_servers}){

      ### block for a moment (don't look too often)
      select(undef,undef,undef,5);
      &check_sigs();

      ### periodically see which children are alive
      my $time = time();
      if( $time - $last_checked_for_dead > $prop->{check_for_dead} ){
        $last_checked_for_dead = $time;
        $self->log(2,"Max number of children reached ($prop->{max_servers}) -- checking for alive.");
        foreach (keys %{ $prop->{children} }){
          ### see if the child can be killed
          kill(0,$_) or $self->delete_child($_);
        }
      }
      $n_children = grep { $_->{status} !~ /dequeue/ } (values %{ $prop->{children} });
    }

    ### periodically check to see if we should clear a queue
    if( defined $prop->{check_for_dequeue} ){
      my $time = time();
      if( $time - $last_checked_for_dequeue
          > $prop->{check_for_dequeue} ){
        $last_checked_for_dequeue = $time;
        if( defined($prop->{max_dequeue}) ){
          my $n_dequeue = grep { $_->{status} =~ /dequeue/ } (values %{ $prop->{children} });
          if( $n_dequeue < $prop->{max_dequeue} ){
            $self->run_dequeue();
          }
        }
      }
    }

    ### call the pre accept hook
    $self->pre_accept_hook;

    ### try to call accept
    ### accept will check signals as appropriate
    if( ! $self->accept() ){
      last if $prop->{_HUP};
      next;
    }

    ### call the post accept hook
    $self->post_accept_hook;

    ### fork a child so the parent can go back to listening
    my $pid = fork;

    ### trouble
    if( not defined $pid ){
      $self->log(1,"Bad fork [$!]");
      sleep(5);

    ### parent
    }elsif( $pid ){
      close($prop->{client}) if ! $prop->{udp_true};
      $prop->{children}->{$pid}->{status} = 'processing';

    ### child
    }else{
      $self->run_client_connection;
      exit;

    }

  }

  ### fall back to the main run routine
}

sub pre_accept_hook {};

### Net::Server::Fork's own accept method which
### takes advantage of safe signals
sub accept {
  my $self = shift;
  my $prop = $self->{server};

  ### block on trying to get a handle, timeout on 10 seconds
  my(@socks) = $prop->{select}->can_read(10);

  ### see if any sigs occured
  if( &check_sigs() ){
    return undef if $prop->{_HUP};
    return undef unless @socks; # don't continue unless we have a connection
  }

  ### choose one at random (probably only one)
  my $sock = $socks[rand @socks];
  return undef unless defined $sock;

  ### check if this is UDP
  if( SOCK_DGRAM == $sock->getsockopt(SOL_SOCKET,SO_TYPE) ){
    $prop->{udp_true} = 1;
    $prop->{client}   = $sock;
    $prop->{udp_true} = 1;
    $prop->{udp_peer} = $sock->recv($prop->{udp_data},
                                    $sock->NS_recv_len,
                                    $sock->NS_recv_flags);

  ### Receive a SOCK_STREAM (TCP or UNIX) packet
  }else{
    delete $prop->{udp_true};
    $prop->{client} = $sock->accept();
    return undef unless defined $prop->{client};
  }
}

### override a little to restore sigs
sub run_client_connection {
  my $self = shift;

  ### close the main sock, we still have
  ### the client handle, this will allow us
  ### to HUP the parent at any time
  $_ = undef foreach @{ $self->{server}->{sock} };

  ### restore sigs (for the child)
  $SIG{HUP} = $SIG{CHLD} = $SIG{PIPE}
     = $SIG{INT} = $SIG{TERM} = $SIG{QUIT} = 'DEFAULT';

  $self->SUPER::run_client_connection;

}

### Stub function in case check_for_dequeue is used.
sub run_dequeue {
  die "run_dequeue: virtual method not defined";
}

1;

__END__

=head1 NAME

Net::Server::Fork - Net::Server personality

=head1 SYNOPSIS

  use Net::Server::Fork;
  @ISA = qw(Net::Server::Fork);

  sub process_request {
     #...code...
  }

  __PACKAGE__->run();

=head1 DESCRIPTION

Please read the pod on Net::Server first.  This module
is a personality, or extension, or sub class, of the
Net::Server module.

This personality binds to one or more ports and then waits
for a client connection.  When a connection is received,
the server forks a child.  The child handles the request
and then closes.

=head1 ARGUMENTS

=over 4

=item check_for_dead

Number of seconds to wait before looking for dead children.
This only takes place if the maximum number of child processes
(max_servers) has been reached.  Default is 60 seconds.

=item max_servers

The maximum number of children to fork.  The server will
not accept connections until there are free children. Default
is 256 children.

=item max_dequeue

The maximum number of dequeue processes to start.  If a
value of zero or undef is given, no dequeue processes will
be started.  The number of running dequeue processes will
be checked by the check_for_dead variable.

=item check_for_dequeue

Seconds to wait before forking off a dequeue process.  It
is intended to use the dequeue process to take care of
items such as mail queues.  If a value of undef is given,
no dequeue processes will be started.

=back

=head1 CONFIGURATION FILE

See L<Net::Server>.

=head1 PROCESS FLOW

Process flow follows Net::Server until the post_accept phase.
At this point a child is forked.  The parent is immediately
able to wait for another request.  The child handles the
request and then exits.

=head1 HOOKS

The Fork server has the following hooks in addition to
the hooks provided by the Net::Server base class.
See L<Net::Server>

=over 4

=item C<$self-E<gt>pre_accept_hook()>

This hook occurs just before the accept is called.

=item C<$self-E<gt>post_accept_hook()>

This hook occurs just after accept but before the fork.

=item C<$self-E<gt>run_dequeue()>

This hook only gets called in conjuction with the
check_for_dequeue setting.

=back

=head1 TO DO

See L<Net::Server>

=head1 AUTHOR

Paul T. Seamons paul@seamons.com

and maintained by Rob Brown bbb@cpan.org

=head1 SEE ALSO

Please see also
L<Net::Server::INET>,
L<Net::Server::PreFork>,
L<Net::Server::MultiType>,
L<Net::Server::SIG>
L<Net::Server::Single>

=cut

