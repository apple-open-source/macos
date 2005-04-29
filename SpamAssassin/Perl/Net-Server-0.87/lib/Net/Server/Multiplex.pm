# -*- perl -*-
#
#  Net::Server::Multiplex - Net::Server personality
#
#  $Id: Multiplex.pm,v 1.1 2004/04/19 17:50:29 dasenbro Exp $
#
#  Copyright (C) 2001-2003, Rob Brown <bbb@cpan.org>
#
#  This package may be distributed under the terms of either the
#  GNU General Public License
#    or the
#  Perl Artistic License
#
#  All rights reserved.
#
################################################################

package Net::Server::Multiplex;

use strict;
use vars qw($VERSION @ISA);
use Net::Server;
use Net::Server::SIG qw(register_sig check_sigs);
use Carp qw(confess);
eval { require IO::Multiplex; import IO::Multiplex 1.05; };
$@ && warn "Module IO::Multiplex is required for Multiplex.";

$VERSION = $Net::Server::VERSION;
@ISA = qw(Net::Server);


sub loop {
  my $self = shift;
  my $prop = $self->{server};

  my $mux = new IO::Multiplex;
  $self->{mux} = $mux;

  foreach my $sock ( @{ $prop->{sock} } ) {
    $mux->listen($sock);
  }
  $mux->set_callback_object(init Net::Server::Multiplex::MUX $self);

  ###
  ### Use Net::Server::SIG for safe signal handling.
  ###

  ### register some of the signals for safe handling
  register_sig(PIPE => sub { $self->log(4, "SIG$_[0] received") },
               INT  => sub { $self->server_close() },
               TERM => sub { $self->server_close() },
               QUIT => sub { $self->server_close() },
               HUP  => sub { $self->sig_hup() },
               CHLD => sub { $self->sig_chld() },
               );

  if ( defined $prop->{check_for_dequeue} ) {
    # It does not matter which socket the timeout is associated with.
    $mux->set_timeout( $prop->{sock}->[0], $prop->{check_for_dequeue} );
  }

  $mux->loop(sub {
    my ($rdready, $wrready) = @_;
    &check_sigs();
    $mux->endloop if $prop->{_HUP};
  });

  ### fall back to the main run routine
}


# This method instead of run_client_connection
# because STDOUT should be tied correctly,
# not just globbed onto the socket.  This
# tie is taken care of in the mux_connection
# routine instead of within post_accept.
# Also, the process_request stuff should never be
# used since the request should be really processed
# via mux_* methods.

sub setup_client_connection {
  my $self = shift;
  my $mux  = shift;
  my $prop = $self->{server};

  ### Copied from Net::Server::post_accept...
  $prop->{requests} ++;
  *STDIN  = \*{ $prop->{client} };
#  *STDOUT = \*{ $prop->{client} };
#  STDIN->autoflush(1);

  ### Copied from Net::Server::run_client_connection...
  $self->get_client_info;     # determines information about peer and local
  $self->post_accept_hook;    # user customizable hook
  unless($self->allow_deny &&       # do allow/deny check on client info
         $self->allow_deny_hook ){  # user customizable hook
    $self->request_denied_hook;     # user customizable hook
    # Flush output buffer and close connection since it should be denied.
    close (STDOUT);
    return 0;
  }
  return 1;
}

# Compatibility interface for Net::Server
sub run_dequeue {
  confess "&$Net::Server::Multiplex::MUX::ISA[0]\::run_dequeue never defined";
}

sub mux_connection {}
sub mux_input {
  confess "&$Net::Server::Multiplex::MUX::ISA[0]\::mux_input never defined";
}
sub mux_eof {}
sub mux_close {}
sub mux_timeout {
  confess "&$Net::Server::Multiplex::MUX::ISA[0]\::mux_timeout never defined";
}

package Net::Server::Multiplex::MUX;

# Just a dumb module to be used for the
# Multiplex callback_object hooks

use strict;
use vars qw($VERSION @ISA);

$VERSION = $Net::Server::Multiplex::VERSION;
# This temporary @ISA should always be overridden
# at runtime when init() is called.  This module should
# really ISA whatever module ISA Net::Server::Multiplex.
@ISA = qw(Net::Server::Multiplex);

# This subroutine is meant to create the main callback
# object to be used for all listen file descriptors.
# It just needs to make sure the {net_server} property
# is set.
sub init {
  my $package  = shift;
  my $net_server= shift;
  # On-the-fly runtime molymorphism hack
  # to ISA the same type of thing passed.
  @ISA = (ref $net_server);
  my $self     = bless {
    net_server => $net_server,
  } => $package;
  return $self;
}

# The new() routine is passed the Net::Server object.  It
# is meant to create the client specific callback object.
# Note that the $net_server->{server} property hash may be
# modified by future connections through Net::Server.
# Any values within it that this object may need to use
# later must be copied within itself.
sub new {
  my $package  = shift;
  my $net_server= shift;
  my $self     = bless {
    # Some nice values to remember for this client
    net_server => $net_server,
    peeraddr   => $net_server->{server}->{peeraddr},
    connected  => time,
  } => $package;
  return $self;
}

# This subroutine is only used by the listen callback object.
sub mux_connection {
  my $self = shift;
  my $mux  = shift;
  my $fh   = shift;
  my $net_server = $self->{net_server};
  $net_server->{server}->{client} = $fh;
  $self->_link_stdout($mux, $fh);
  if ($net_server->setup_client_connection($mux)) {
    # Create client specific callback object
    my $client_object = new Net::Server::Multiplex::MUX ($net_server, $fh);
    # Set this as the callback object for this client
    $mux->set_callback_object($client_object, $fh);
    # Finally call the clients real mux_connection routine,
    # if any.  This allows all the mux_* routines to be
    # called from the same type of object.
    $client_object->SUPER::mux_connection($mux, $fh);
    #$client_object->mux_connection($mux, $fh);
  }
  $self->_unlink_stdout();
  return;
}

sub mux_input {
  my $self = shift;
  my $mux  = shift;
  my $fh   = shift;
  my $in_ref = shift;  # Scalar reference to the input
  $self->_link_stdout($mux, $fh);
  $self->SUPER::mux_input($mux, $fh, $in_ref);
  $self->_unlink_stdout();
  return;
}

sub mux_eof {
  my $self = shift;
  my $mux  = shift;
  my $fh   = shift;
  my $in_ref = shift;  # Scalar reference to the input
  $self->_link_stdout($mux, $fh);
  $self->SUPER::mux_eof($mux, $fh, $in_ref);
  $self->_unlink_stdout();
  $mux->shutdown($fh, 1);
  return;
}

sub mux_close {
  my $self = shift;
  my $mux  = shift;
  my $fh   = shift;
  $self->{net_server}->post_process_request_hook;
  $self->SUPER::mux_close($mux, $fh);
  return;
}

sub mux_timeout {
  my $self = shift;
  my $mux  = shift;
  my $fh   = shift;

  if ( my $check = $self->{net_server}->{server}->{check_for_dequeue} ) {
    $self->{net_server}->run_dequeue();
    $mux->set_timeout( $fh, $check );
  } else {
    $self->_link_stdout($mux, $fh);
    $self->SUPER::mux_timeout($mux, $fh);
    $self->_unlink_stdout();
  }
  return;
}

sub _link_stdout {
  my $self = shift;
  my $mux = shift;
  my $fh = shift;
  # Hook up STDOUT to the correct socket
  if (tied *$fh) {
    # Make sure STDOUT is tied however $fh is
    tie (*STDOUT, (ref tied *$fh), $mux, $fh);
  } else {
    *STDOUT = *$fh;
  }
}

sub _unlink_stdout {
  my $self = shift;
  my $x = tied *STDOUT;
  if ($x) {
    undef $x;
    untie *STDOUT;
  }
}

1;

__END__

=head1 NAME

Net::Server::Multiplex - Multiplex several connections within one process

=head1 SYNOPSIS

  package MyPlexer;

  use base 'Net::Server::Multiplex';

  sub mux_input {
     #...code...
  }

  __PACKAGE__->run();

=head1 DESCRIPTION

This personality is designed to handle multiple connections
all within one process.  It should only be used with protocols
that are guaranteed to be able to respond quickly on a packet
by packet basis.  If determining a response could take a while
or an unknown period of time, all other connections established
will block until the response completes.  If this condition
might ever occur, this personality should probably not be used.

This takes some nice features of Net::Server (like the server
listen socket setup, configuration file processing, safe signal
handling, convenient inet style STDIN/STDOUT handling, logging
features, deamonization and pid tracking, and restartability
-SIGHUP) and some nice features of IO::Multiplex (automatic
buffered IO and per-file-handle objects) and combines them for
an easy-to-use interace.

See examples/samplechat.pl distributed with Net::Server for a
simple chat server that uses several of these features.

=head1 PROCESS FLOW

The process flow is written in an open, easy to
override, easy to hook, fashion.  The basic flow is
shown below.

  $self->configure_hook;

  $self->configure(@_);

  $self->post_configure;

  $self->post_configure_hook;

  $self->pre_bind;

  $self->bind;

  if( Restarting server ){
     $self->restart_open_hook();
  }

  $self->post_bind_hook;

  $self->post_bind;

  $self->pre_loop_hook;

  $self->loop; # This basically just runs IO::Multiplex::loop
  # For routines inside a $self->loop
  # See CLIENT PROCESSING below

  $self->pre_server_close_hook;

  $self->post_child_cleanup_hook;

  $self->server_close;

  if( Restarting server ){
     $self->restart_close_hook();
     $self->hup_server;
     # Redo process again starting with configure_hook
  }

The server then exits.

=head1 CLIENT PROCESSING

The following represents the client processing program flow:

  $self->{server}->{client} = Net::Server::Proto::TCP->accept();  # NOTE: Multiplexed with mux_input() below

  if (check_for_dequeue seconds have passed) {
    $self->run_dequeue();
  }

  $self->get_client_info;

  $self->post_accept_hook; # Net::Server style

  if( $self->allow_deny

      && $self->allow_deny_hook ){

    # (Net::Server style $self->process_request() is never called.)

    # A unique client specific object is created
    # for all mux_* methods from this point on.
    $self = __PACKAGE__->new($self, client);

    $self->mux_connection; # IO::Multiplex style

    for (every packet received) {
      $self->mux_input;  # NOTE: Multiplexed with accept() above
    }

  }else{

    $self->request_denied_hook;

    # Notice that if either allow_deny or allow_deny_hook fails, then
    # new(), mux_connection(), and mux_input() will never be called.
    # mux_eof() and mux_close() will still be called, but using a
    # common listen socket callback object instead of a unique client
    # specific object.

  }

  $self->mux_eof;

  $self->post_process_request_hook;

  $self->mux_close;


This process then loops multiplexing between the accept()
for the next connection and mux_input() when input arrives
to avoid blocking either one.

=head1 HOOKS

The *_hook methods mentioned above are meant to be overridden
with your own subroutines if you desire to provide additional
functionality.

The loop() method of Net::Server has been overridden to run the
loop routine of IO::Multiplex instead.  The Net::Server methods
may access the IO::Multiplex object at C<$self-E<gt>{mux}> if
desired.  The IO::Multiplex methods may access the Net::Server
object at C<$self-E<gt>{net_server}> if desired.

The process_request() method is never used with this personality.

The other Net::Server hooks and methods should work the same.

=over 4

=item C<$self-E<gt>run_dequeue()>

This hook only gets called in conjuction with the check_for_dequeue
setting.  It will run every check_for_dequeue seconds.  Since no
forking is done, this hook should run fast in order to prevent
blocking the rest of the processing.

=back

=head1 TIMEOUTS

=head2 set_timeout

To utilize the optional timeout feature of IO::Multiplex,
you need to specify a timeout by using the set_timeout
method.

$self->{net_server}->{mux}->set_timeout($fh, $seconds_from_now);

$fh may be either a client socket or a listen socket file descriptor
within the mux.  $seconds_from_now may be fractional to achieve
more precise timeouts.  This is used in conjuction with mux_timeout,
which you should define yourself.

=head2 mux_timeout

The main loop() routine will call $obj->mux_timeout($mux, $fh)
when the timeout specified in set_timeout is reached where
$fh is the same as the one specified in set_timeout() and
$obj is its corresponding object (either the unique client
specific object or the main listen callback object) and
$mux is the main IO::Multiplex object itself.

=head1 CALLBACK INTERFACE

Callback objects should support the following interface.  You do not have
to provide all of these methods, just provide the ones you are interested in.
These are just like the IO::Multiplex hooks except that STDOUT is tied to
the corresponding client socket handle for your convenience and to more
closely emulate the Net::Server model.  However, unlike some other
Net::Server personalities, you should never read directly from STDIN
yourself.   You should define one or more of the following methods:

=head2 mux_connection ($mux,$fh)

(OPTIONAL)
Run once when the client first connects if the allow_deny passes.
Note that the C<$self-E<gt>{net_server}-E<gt>{server}> property hash
may be modified by future connections through Net::Server.  Any values
within it that this object may need to use later should be copied within
its own object at this point.

Example:
  $self->{peerport} = $self->{net_server}->{server}->{peerport};

=head2 mux_input ($mux,$fh,\$data)

(REQUIRED)
Run each time a packet is read.  It should consume $data starting
at the left and leave unconsumed data in the scalar for future
calls to mux_input.

=head2 mux_eof ($mux,$fh,\$data)

(OPTIONAL)
Run once when the client is done writing.  It should consume
the rest of $data since mux_input() will never be run again.

=head2 mux_close ($mux,$fh)

(OPTIONAL)
Run after the entire client socket has been closed.  No more
attempts should be made to read or write to the client or to
STDOUT.

=head2 mux_timeout ($mux,$fh)

(OPTIONAL)
Run once when the set_timeout setting expires as
explained above.

=head1 BUGS

This is only known to work with TCP servers.

If you need to use the IO::Multiplex style set_timeout / mux_timeout
interface, you cannot use the Net::Server style check_for_dequeue
/ run_dequeue interface.  It will not work if the check_for_dequeue
option is specified.  The run_dequeue method is just a compatibility
interface to comply with the Net::Server::Fork style run_dequeue but
is implemented in terms of the IO::Multiplex style set_timeout and
mux_timeout methods.

Please notify me, the author, of any other problems or issues
you find.

=head1 AUTHOR

Copyright (C) 2001-2003, Rob Brown <bbb@cpan.org>

This package may be distributed under the terms of either the
GNU General Public License
   or the
Perl Artistic License

All rights reserved.

=head1 SEE ALSO

L<Net::Server> by Paul Seamons <paul@seamons.com>,

L<IO::Multiplex> by Bruce Keeler <bruce@gridpoint.com>.

=cut
