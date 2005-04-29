# -*- perl -*-
#
#  Net::Server::Proto::TCP - Net::Server Protocol module
#  
#  $Id: TCP.pm,v 1.1 2004/04/19 17:50:30 dasenbro Exp $
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

package Net::Server::Proto::TCP;

use strict;
use vars qw($VERSION $AUTOLOAD @ISA);
use IO::Socket ();

$VERSION = $Net::Server::VERSION; # done until separated
@ISA = qw(IO::Socket::INET);

sub object {
  my $type  = shift;
  my $class = ref($type) || $type || __PACKAGE__;

  my ($default_host,$port,$server) = @_;
  my $host;

  ### allow for things like "domain.com:80"
  if( $port =~ m/^([\w\.\-\*\/]+):(\w+)$/ ){
    ($host,$port) = ($1,$2);

  ### allow for things like "80"
  }elsif( $port =~ /^(\w+)$/ ){
    ($host,$port) = ($default_host,$1);

  ### don't know that style of port
  }else{
    $server->fatal("Undeterminate port \"$port\" under ".__PACKAGE__);
  }

  ### create the handle under this package
  my $sock = $class->SUPER::new();

  ### store some properties
  $sock->NS_host($host);
  $sock->NS_port($port);
  $sock->NS_proto('TCP');

  return $sock;
}

sub log_connect {
  my $sock = shift;
  my $server = shift;
  my $host   = $sock->NS_host; 
  my $port   = $sock->NS_port;
  my $proto  = $sock->NS_proto;
 $server->log(2,"Binding to $proto port $port on host $host\n");
}

### connect the first time
sub connect {
  my $sock   = shift;
  my $server = shift;
  my $prop   = $server->{server};

  my $host  = $sock->NS_host;
  my $port  = $sock->NS_port;

  my %args = ();
  $args{LocalPort} = $port;                  # what port to bind on
  $args{Proto}     = 'tcp';                  # what procol to use
  $args{LocalAddr} = $host if $host !~ /\*/; # what local address (* is all)
  $args{Listen}    = $prop->{listen};        # how many connections for kernel to queue
  $args{Reuse}     = 1;  # allow us to rebind the port on a restart
  
  ### connect to the sock
  $sock->SUPER::configure(\%args)
    or $server->fatal("Can't connect to TCP port $port on $host [$!]");

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
    $client->NS_proto( $sock->NS_proto );
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
              $sock->NS_host,
              $sock->NS_port,
              $sock->NS_proto,
              );
}

### short routine to show what we think we are
sub show {
  my $sock = shift;
  my $t = "Ref = \"" .ref($sock) . "\"\n";
  foreach my $prop ( qw(NS_proto NS_port NS_host) ){
    $t .= "  $prop = \"" .$sock->$prop()."\"\n";
  }
  return $t;
}

### self installer
sub AUTOLOAD {
  my $sock = shift;

  my ($prop) = $AUTOLOAD =~ /::([^:]+)$/ ? $1 : '';
  if( ! $prop ){
    die "No property called.";
  }

  if( $prop =~ /^(NS_proto|NS_port|NS_host|NS_recv_len|NS_recv_flags)$/ ){
    no strict 'refs';
    * { __PACKAGE__ ."::". $prop } = sub {
      my $sock = shift;
      if( @_ ){
        ${*$sock}{$prop} = shift;
        return delete ${*$sock}{$prop} unless defined ${*$sock}{$prop};
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

  Net::Server::Proto::TCP - adp0 - Net::Server TCP protocol.

=head1 SYNOPSIS

See L<Net::Server::Proto>.

=head1 DESCRIPTION

Protocol module for Net::Server.  This module implements the
SOCK_STREAM socket type under INET (also known as TCP).
See L<Net::Server::Proto>.

=head1 PARAMETERS

There are no additional parameters that can be specified.
See L<Net::Server> for more information on reading arguments.

=head1 LICENCE

Distributed under the same terms as Net::Server

=cut

