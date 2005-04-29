# -*- perl -*-
#
#  Net::Server::Proto::UNIX - Net::Server Protocol module
#  
#  $Id: UNIX.pm,v 1.1 2004/04/19 17:50:30 dasenbro Exp $
#  
#  Copyright (C) 2001, Paul T Seamons
#                      paul@seamons.com
#                      http://seamons.com/
#  
#  This package may be distributed under the terms of either the
#  GNU General Public License 
#    or the
#  Perl Artistic License
#
#  All rights reserved.
#  
################################################################

package Net::Server::Proto::UNIX;

use strict;
use vars qw($VERSION $AUTOLOAD @ISA);
use IO::Socket ();
use Socket qw(SOCK_STREAM SOCK_DGRAM);

$VERSION = $Net::Server::VERSION; # done until separated
@ISA = qw(IO::Socket::UNIX);

sub object {
  my $type  = shift;
  my $class = ref($type) || $type || __PACKAGE__;

  my ($default_host,$port,$server) = @_;
  my $prop = $server->{server};

  ### read any additional protocol specific arguments
  $server->configure({
    unix_type => \$prop->{unix_type},
    unix_path => \$prop->{unix_path},
  });

  my $u_type = $prop->{unix_type} || SOCK_STREAM;
  my $u_path = $prop->{unix_path} || undef;

  ### allow for things like "/tmp/myfile.sock|SOCK_STREAM"
  if( $port =~ m/^([\w\.\-\*\/]+)\|(\w+)$/ ){
    ($u_path,$u_type) = ($1,$2);

  ### allow for things like "/tmp/myfile.sock"
  }elsif( $port =~ /^([\w\.\-\*\/]+)$/ ){
    $u_path = $1;

  ### don't know that style of port
  }else{
    $server->fatal("Undeterminate port \"$port\" under ".__PACKAGE__);
  }

  ### allow for the string rather than the function
  if( $u_type eq 'SOCK_STREAM' ){
    $u_type = SOCK_STREAM;
  }elsif( $u_type eq 'SOCK_DGRAM' ){
    $u_type = SOCK_DGRAM;
  }

  ### create a blank socket
  my $sock = $class->SUPER::new();


  ### set a few more parameters for SOCK_DGRAM
  if( $u_type == SOCK_DGRAM ){
    
    $prop->{udp_recv_len} = 4096
      unless defined($prop->{udp_recv_len})
      && $prop->{udp_recv_len} =~ /^\d+$/;
    
    $prop->{udp_recv_flags} = 0
      unless defined($prop->{udp_recv_flags})
      && $prop->{udp_recv_flags} =~ /^\d+$/;
    
    $sock->NS_recv_len(   $prop->{udp_recv_len} );
    $sock->NS_recv_flags( $prop->{udp_recv_flags} );

  }elsif( $u_type == SOCK_STREAM ){

  }else{
    $server->fatal("Invalid type for UNIX socket ($u_type)... must be SOCK_STREAM or SOCK_DGRAM");
  }

  ### set some common parameters
  $sock->NS_unix_type( $u_type );
  $sock->NS_unix_path( $u_path );
  $sock->NS_proto('UNIX');
  
  return $sock;
}

sub log_connect {
  my $sock = shift;
  my $server    = shift;
  my $unix_path = $sock->NS_unix_path;
  my $type = ($sock->NS_unix_type == SOCK_STREAM) ? 'SOCK_STREAM' : 'SOCK_DGRAM';
  
  $server->log(2,"Binding to UNIX socket file $unix_path using $type\n");
}

### connect the first time
### doesn't support the listen or the reuse option
sub connect {
  my $sock   = shift;
  my $server = shift;
  my $prop   = $server->{server};

  my $unix_path = $sock->NS_unix_path;
  my $unix_type = $sock->NS_unix_type;

  my %args = ();
  $args{Local}  = $unix_path;       # what socket file to bind to
  $args{Type}   = $unix_type;       # SOCK_STREAM (default) or SOCK_DGRAM
  if( $unix_type != SOCK_DGRAM ){
    $args{Listen} = $prop->{listen};  # how many connections for kernel to queue
  }

  ### remove the old socket if it is still there
  if( -e $unix_path && ! unlink($unix_path) ){
    $server->fatal("Can't connect to UNIX socket at file $unix_path [$!]");
  }

  ### connect to the sock
  $sock->SUPER::configure(\%args)
    or $server->fatal("Can't connect to UNIX socket at file $unix_path [$!]");

  $server->fatal("Back sock [$!]!".caller())
    unless $sock;

}

### connect on a sig -HUP
sub reconnect {
  my $sock = shift;
  my $fd   = shift;
  my $server = shift;

  $sock->fdopen( $fd, 'w' )
    or $server->fatal("Error opening to file descriptor ($fd) [$!]");

}

### allow for endowing the child
sub accept {
  my $sock = shift;
  my $client = $sock->SUPER::accept();

  ### pass items on
  if( defined($client) ){
    $client->NS_proto(     $sock->NS_proto );
    $client->NS_unix_path( $sock->NS_unix_path );
    $client->NS_unix_type( $sock->NS_unix_type );
  }

  return $client;
}

### a string containing any information necessary for restarting the server
### via a -HUP signal
### a newline is not allowed
### the hup_string must be a unique identifier based on configuration info
sub hup_string {
  my $sock = shift;
  return join("|",
              $sock->NS_unix_path,
              $sock->NS_unix_type,
              $sock->NS_proto,
              );
}

### short routine to show what we think we are
sub show {
  my $sock = shift;
  my $t = "Ref = \"" .ref($sock) . "\"\n";
  foreach my $prop ( qw(NS_proto NS_unix_path NS_unix_type) ){
    $t .= "  $prop = \"" .$sock->$prop()."\"\n";
  }
  $t =~ s/"1"/SOCK_STREAM/;
  $t =~ s/"2"/SOCK_DGRAM/;
  return $t;
}

### self installer
sub AUTOLOAD {
  my $sock = shift;

  my ($prop) = $AUTOLOAD =~ /::([^:]+)$/ ? $1 : '';
  if( ! $prop ){
    die "No property called.";
  }

  if( $prop =~ /^(NS_proto|NS_unix_path|NS_unix_type|NS_recv_len|NS_recv_flags)$/ ){
    no strict 'refs';
    * { __PACKAGE__ ."::". $prop } = sub {
      my $sock = shift;
      if( @_ ){
        ${*$sock}{$prop} = shift;
        delete ${*$sock}{$prop} unless defined ${*$sock}{$prop};
      }else{
        return ${*$sock}{$prop};
      }
    };
    use strict 'refs';

    $sock->$prop(@_);

  }else{
    die "What method is that? [$prop]";
  }
}



1;

__END__

=head1 NAME

  Net::Server::Proto::UNIX - adp0 - Net::Server UNIX protocol.

=head1 SYNOPSIS

See L<Net::Server::Proto>.

=head1 DESCRIPTION

Protocol module for Net::Server.  This module implements the
SOCK_DGRAM and SOCK_STREAM socket types under UNIX.
See L<Net::Server::Proto>.

Any sockets created during startup will be chown'ed to the
user and group specified in the starup arguments.

=head1 PARAMETERS

The following paramaters may be specified in addition to
normal command line parameters for a Net::Server.  See
L<Net::Server> for more information on reading arguments.

=over 4

=item unix_type

Can be either SOCK_STREAM or SOCK_DGRAM (default is SOCK_STREAM).
This can also be passed on the port line (see L<Net::Server::Proto>).

=item unix_path

Default path to the socket file for this UNIX socket.  Default
is undef.  This can also be passed on the port line (see
L<Net::Server::Proto>).

=back

=head1 QUICK PARAMETER LIST

  Key               Value                    Default

  ## UNIX socket parameters
  unix_type         (SOCK_STREAM|SOCK_DGRAM) SOCK_STREAM
  unix_path         "filename"               undef

=head1 LICENCE

Distributed under the same terms as Net::Server

=cut

