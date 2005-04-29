#!/usr/bin/perl -w -T

# This example demonstrates some of the features of Net::Server::Multiplex
#
#
# To run this in background daemon mode, listening on port 2000, do:
#
#   samplechat.pl --setsid=1 --log_file=/tmp/samplechat.log --pid_file=/tmp/samplechat.pid --port=2000
#
# To turn off the daemon, do:
#
#   kill `cat /tmp/samplechat.pid`;
#

package SampleChatServer;

use strict;
use Net::Server::Multiplex;
use vars qw(@ISA);
@ISA = qw(Net::Server::Multiplex);


# Demonstrate a Net::Server style hook
sub allow_deny_hook {
  my $self = shift;
  my $prop = $self->{server};
  my $sock = $prop->{client};

  return 1 if $prop->{peeraddr} =~ /^127\./;
  return 0;
}


# Another Net::Server style hook
sub request_denied_hook {
  print "Go away!\n";
  print STDERR "DEBUG: Client denied!\n";
}


# IO::Multiplex style callback hook
sub mux_connection {
  my $self = shift;
  my $mux  = shift;
  my $fh   = shift;
  my $peer = $self->{peeraddr};
  # Net::Server stores a connection counter in the {requests} field.
  $self->{id} = $self->{net_server}->{server}->{requests};
  # Keep some values that I might need while the {server}
  # property hash still contains the current client info
  # and stash them in my own object hash.
  $self->{peerport} = $self->{net_server}->{server}->{peerport};
  # Net::Server directs STDERR to the log_file
  print STDERR "DEBUG: Client [$peer] (id $self->{id}) just connected...\n";
  # Notify everyone that the client arrived
  $self->broadcast($mux,"JOIN: (#$self->{id}) from $peer\r\n");
  # STDOUT is tie'd to the correct IO::Multiplex handle
  print "Welcome, you are number $self->{id} to connect.\r\n";
  # Try out the timeout feature of IO::Multiplex
  $mux->set_timeout($fh, 20);
  # This is my state and will be unique to this connection
  $self->{state} = "junior";
}


# If this callback is ever hooked, then the mux_connection callback
# is guaranteed to have already been run once (if defined).
sub mux_input {
  my $self = shift;
  my $mux  = shift;
  my $fh   = shift;
  my $in_ref = shift;  # Scalar reference to the input
  my $peer = $self->{peeraddr};
  my $id   = $self->{id};

  print STDERR "DEBUG: input from [$peer] ready for consuming.\n";
  # Process each line in the input, leaving partial lines
  # in the input buffer
  while ($$in_ref =~ s/^(.*?)\r?\n//) {
    next unless $1;
    my $message = "[$id - $peer] $1\r\n";
    $self->broadcast($mux, $message);
    print " - sent ".(length $message)." byte message\r\n";
  }
  if ($self->{state} eq "senior") {
    $mux->set_timeout($fh, 40);
  }
}


# It is possible that this callback will be called even
# if mux_connection or mux_input were never called.  This
# occurs when allow_deny or allow_deny_hook fails to
# authorize the client.  The callback object will be the
# default listen object instead of a client unique object.
# However, both object should contain the $self->{net_server}
# key pointing to the original Net::Server object.
sub mux_close {
  my $self = shift;
  my $mux  = shift;
  my $fh   = shift;
  my $peer = $self->{peeraddr};
  # If mux_connection has actually been run
  if (exists $self->{id}) {
    $self->broadcast($mux,"LEFT: (#$self->{id}) from $peer\r\n");
    print STDERR "DEBUG: Client [$peer] (id $self->{id}) closed connection!\n";
  }
}


# This callback will happen when the mux->set_timeout expires.
sub mux_timeout {
  my $self = shift;
  my $mux  = shift;
  my $fh   = shift;
  print STDERR "DEBUG: HEARTBEAT!\n";
  if ($self->{state} eq "junior") {
    print "Whoa, you must have a lot of patience.  You have been upgraded.\r\n";
    $self->{state} = "senior";
  } elsif ($self->{state} eq "senior") {
    print "If you don't want to talk then you should leave. *BYE*\r\n";
    close(STDOUT);
  }
  $mux->set_timeout($fh, 40);
}


# Routine to send a message to all clients in a mux.
sub broadcast {
  my $self = shift;
  my $mux  = shift;
  my $msg  = shift;
  foreach my $fh ($mux->handles) {
    # NOTE: All the client unique objects can be found at
    # $mux->{_fhs}->{$fh}->{object}
    # In this example, the {id} would be
    #   $mux->{_fhs}->{$fh}->{object}->{id}
    print $fh $msg;
  }
}


__PACKAGE__->run();
