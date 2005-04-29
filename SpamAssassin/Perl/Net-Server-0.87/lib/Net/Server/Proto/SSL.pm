# -*- perl -*-
#
#  Net::Server::Proto::SSL - Net::Server Protocol module
#
#  $Id: SSL.pm,v 1.1 2004/04/19 17:50:30 dasenbro Exp $
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

package Net::Server::Proto::SSL;

use strict;
use vars qw($VERSION $AUTOLOAD @ISA);
use Net::Server::Proto::TCP ();
eval { require IO::Socket::SSL; };
$@ && warn "Module IO::Socket::SSL is required for SSL.";

$VERSION = $Net::Server::VERSION; # done until separated
@ISA = qw(IO::Socket::SSL);


sub object {
  my $type  = shift;
  my $class = ref($type) || $type || __PACKAGE__;

  my ($default_host,$port,$server) = @_;
  my $prop = $server->{server};
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

  ### read any additional protocol specific arguments
  $server->configure({
    SSL_server      => \$prop->{SSL_server},
    SSL_use_cert    => \$prop->{SSL_use_cert},
    SSL_verify_mode => \$prop->{SSL_verify_mode},
    SSL_key_file    => \$prop->{SSL_key_file},
    SSL_cert_file   => \$prop->{SSL_cert_file},
    SSL_ca_path     => \$prop->{SSL_ca_path},
    SSL_ca_file     => \$prop->{SSL_ca_file},
    SSL_cipher_list => \$prop->{SSL_cipher_list},
  });

  ### create the handle under this package
  my $sock = $class->SUPER::new();

  ### store some properties
  $sock->NS_host($host);
  $sock->NS_port($port);
  $sock->NS_proto('SSL');

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

  ### add in any ssl specific properties
  foreach ( keys %$prop ){
    next unless /^SSL_/;
    $args{$_} = $prop->{$_};
  }

  ### connect to the sock
  $sock->SUPER::configure(\%args)
    or $server->fatal("Can't connect to SSL port $port on $host [$!]");

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
    bless $client, ref($sock);
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

  if( $prop =~ /^(NS_proto|NS_port|NS_host)$/ ){
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

=head1 NAME

  Net::Server::Proto::SSL - adp0 - Net::Server SSL protocol.

=head1 SYNOPSIS

See L<Net::Server::Proto>.

=head1 DESCRIPTION

Experimental.  If anybody has any successes or ideas for
improvment under SSL, please email <perl.ssl@seamons.com>.
This is extremely alpha.

Protocol module for Net::Server.  This module implements a
secure socket layer over tcp (also known as SSL).
See L<Net::Server::Proto>.

There is a limit inherent from using IO::Socket::SSL,
namely that only one SSL connection can be maintained by
Net::Server.  However, Net::Server should also be able to
maintain any number of TCP, UDP, or UNIX connections in
addition to the one SSL connection.

Additionally, getline support is very limited and writing
directly to STDOUT will not work.  This is entirely dependent
upon the implementation of IO::Socket::SSL.  getline may work
but the client is not copied to STDOUT under SSL.  It is suggested
that clients sysread and syswrite to the client handle
(located in $self->{server}->{client} or passed to the process_request
subroutine as the first argument).

=head1 PARAMETERS

In addition to the normal Net::Server parameters, any of the
SSL parameters from IO::Socket::SSL may also be specified.
See L<IO::Socket::SSL> for information on setting this up.

=head1 LICENCE

Distributed under the same terms as Net::Server

=head1 THANKS

Thanks to Vadim for pointing out the IO::Socket::SSL accept
was returning objects blessed into the wrong class.

=cut
