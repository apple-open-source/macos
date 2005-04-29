# -*- perl -*-
#
#  Net::Server::INET - Net::Server personality
#  
#  $Id: INET.pm,v 1.1 2004/04/19 17:50:29 dasenbro Exp $
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

package Net::Server::INET;

use strict;
use vars qw(@ISA $VERSION);
use Net::Server;


$VERSION = $Net::Server::VERSION; # done until separated

@ISA = qw(Net::Server);

sub post_configure {
  my $self = shift;
  $self->{server}->{_is_inet} = 1;
  $self->SUPER::post_configure();
  delete $self->{server}->{_is_inet};
}

### no need to prepare bind
sub pre_bind {}

### inet has no port to bind
sub bind {}

### connection is already accepted
sub accept { 
  my $self = shift;
  my $prop = $self->{server};

  ### Net::Server::INET will not do any determination of TCP,UDP,Unix
  ### it is up to the programmer to keep these as separate processes
  delete $prop->{udp_true};
  ### receive a udp packet
#  if( $prop->{udp_true} ){
#    $prop->{client}   = *STDIN;
#    $prop->{udp_peer} = STDIN->recv($prop->{udp_data},
#                                    $prop->{udp_packet_size},
#                                    $prop->{udp_packet_offset});
#  }

  1;
}

### accept only one connection per process
sub done { 1 }

### set up handles
sub post_accept {
  my $self = shift;

  ### STDIN and STDOUT are already bound

  ### create a handle for those who want to use
  ### an IO::Socket'ish handle - more portable
  ### to just use STDIN and STDOUT though
  $self->{server}->{client} = Net::Server::INET::Handle->new();

}

### can't hup single process
sub hup_server {}

################################################################
### the rest are methods to tie STDIN and STDOUT to a GLOB
### this most likely isn't necessary, but the methods are there
### support for this is experimental and may go away
################################################################
package Net::Server::INET::Handle;

use vars qw(@ISA);
use strict;
use IO::Handle ();
@ISA = qw(IO::Handle);

sub new {
  my $class = shift;
  local *HAND;
  STDIN->autoflush(1);
  STDOUT->autoflush(1);
  tie( *HAND, $class, *STDIN, *STDOUT)
    or die "can't tie *HAND: $!";
  bless \*HAND, $class;
  return \*HAND;
}  

sub NS_proto {
  return '';
}

### tied handle methods
sub TIEHANDLE {
  my ($class, $in, $out) = @_;
  bless [ \$in, \$out ], $class;
}

sub PRINT {
  my $handle = shift()->[1];
  local *FH = $$handle;
  CORE::print FH @_;
}

sub PRINTF {
  my $handle = shift()->[1];
  local *FH = $$handle;
  CORE::printf FH @_;
}

sub WRITE {
  my $handle = shift()->[1];
  local *FH = $$handle;
  local ($\) = "";
  $_[1] = length($_[0]) unless defined $_[1];
  CORE::print FH substr($_[0], $_[2] || 0, $_[1]);
}

sub READ {
  my $handle = shift()->[0];
  local *FH = $$handle;
  CORE::read(FH, $_[0], $_[1], $_[2] || 0);
}

sub READLINE {
  my $handle = shift()->[0];
  local *FH = $$handle;
  return scalar <FH>;
}

sub GETC {
  my $handle = shift()->[0];
  local *FH = $$handle;
  return CORE::getc(FH);
}

sub EOF {
  my $handle = shift()->[0];
  local *FH = $$handle;
  return CORE::eof(FH);
}

sub OPEN {}

sub CLOSE {
  my $self = shift;
  $self = undef;
}

sub BINMODE {}

sub TELL {}

sub SEEK {}

sub DESTROY {}

sub FILENO {}

sub FETCH {}
1;


__END__

=head1 NAME

Net::Server::INET - Net::Server personality

=head1 SYNOPSIS

  use Net::Server::INET;
  @ISA = qw(Net::Server::INET);

  sub process_request {
     #...code...
  }

  Net::Server::INET->run();

=head1 DESCRIPTION

Please read the pod on Net::Server first.  This module
is a personality, or extension, or sub class, of the
Net::Server module.

This personality is intended for use with inetd.  It offers
no methods beyond the Net::Server base class.  This module
operates by overriding the pre_bind, bind, accept, and
post_accept methods to let all socket processing to be done
by inetd.

=head1 CONFIGURATION FILE

See L<Net::Server>.

=head1 PROCESS FLOW

See L<Net::Server>

=head1 HOOKS

There are no additional hooks in Net::Server::INET.

=head1 TO DO

See L<Net::Server>

=head1 AUTHOR

Paul T. Seamons paul@seamons.com

=head1 SEE ALSO

Please see also
L<Net::Server::Fork>,
L<Net::Server::INET>,
L<Net::Server::PreFork>,
L<Net::Server::MultiType>,
L<Net::Server::Single>

=cut


