# -*- perl -*-
#
#  Net::Server::MultiType - Net::Server personality
#  
#  $Id: MultiType.pm,v 1.1 2004/04/19 17:50:29 dasenbro Exp $
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

package Net::Server::MultiType;

use strict;
use vars qw($VERSION @ISA);
use Net::Server;

$VERSION = $Net::Server::VERSION; # done until separated

### fall back to parent methods
### we will start out with this, but it should be different if overriden
@ISA = qw(Net::Server);

### allow for override-able options
sub options {
  my $self = shift;
  my $prop = $self->{server};
  my $ref  = shift;

  $self->SUPER::options($ref);

  foreach ( qw(server_type) ){
    $prop->{$_} = [] unless exists $prop->{$_};
    $ref->{$_} = $prop->{$_};
  }
}


sub run {

  ### pass package or object
  my $self = ref($_[0]) ? shift() : (bless {}, shift());
  $self->{server} = {} unless defined($self->{server}) && ref($self->{server});
  my $prop = $self->{server};

  ### save for a HUP
  $prop->{commandline} = [ $0, @ARGV ]
    unless defined $prop->{commandline};

  $self->configure_hook;      # user customizable hook

  ### do the configuration now
  $self->configure(@_);

  ### don't do anything if I haven't specified a type
  if( defined $prop->{server_type} ){

    ### make sure server_type is an array ref
    $prop->{server_type} = [$prop->{server_type}]
      unless ref $prop->{server_type};

    ### iterate on the passed types
    foreach (@{ $prop->{server_type} }){

      next if $_ eq 'MultiType';
      next if ! /^(\w+)$/;
      $_ = $1; # satisfy taint

      my $package = "Net::Server::$_";
      my $package_file = $package .'.pm';
      $package_file =~ s{::}{/}g;
      
      ### see if the package is available
      eval { require $package_file; };

      ### skip if there was an error
      if( $@ ){
        warn "Couldn't become server type \"$package\" [$@]\n";
        next;
      }

      ### turn me into that package
      require $package_file; # outside the eval block
      unshift @ISA, $package;
      if( !defined($prop->{setsid}) && !length($prop->{log_file}) ){
        warn "Becoming sub class of \"$package\"\n";
      }

      ### success - skip any others
      last;
      
    }

  }

  ### now run as the new type of thingy
  ### passing self, instead of package, doesn't instantiate a new object
  $self->SUPER::run();

}

1;

__END__

=head1 NAME

Net::Server::MultiType - Net::Server personality

=head1 SYNOPSIS

  use Net::Server::MultiType;
  @ISA = qw(Net::Server::MultiType);

  sub process_request {
     #...code...
  }

  my @types = qw(PreFork Fork Single);

  Net::Server::MultiType->run(server_type=>\@types);

=head1 DESCRIPTION

Please read the pod on Net::Server first.  This module is a
personality, or extension, or sub class, of the Net::Server
module.

This personality is intended to allow for easy use of
multiple Net::Server personalities.  Given a list of server
types, Net::Server::MultiType will require one at a time
until it finds one that is installed on the system.  It then
adds that package to its @ISA, thus inheriting the methods
of that personality.

=head1 ARGUMENTS

In addition to the command line arguments of the Net::Server
base class, Net::Server::MultiType contains one other
configurable parameter.

  Key               Value            Default
  server_type       'server_type'    'Single'

=over 4

=item server_type

May be called many times to build up an array or possible
server_types.  At execution, Net::Server::MultiType will
find the first available one and then inherit the methods of
that personality

=back

=head1 CONFIGURATION FILE

C<Net::Server::MultiType> allows for the use of a
configuration file to read in server parameters.  The format
of this conf file is simple key value pairs.  Comments and
white space are ignored.

  #-------------- file test.conf --------------

  ### multi type info
  ### try PreFork first, then go to Single
  server_type PreFork
  server_type Single

  ### server information
  min_servers   20
  max_servers   80
  spare_servers 10

  max_requests  1000

  ### user and group to become
  user        somebody
  group       everybody

  ### logging ?
  log_file    /var/log/server.log
  log_level   3
  pid_file    /tmp/server.pid

  ### access control
  allow       .+\.(net|com)
  allow       domain\.com
  deny        a.+

  ### background the process?
  background  1

  ### ports to bind
  host        127.0.0.1
  port        localhost:20204
  port        20205

  ### reverse lookups ?
  # reverse_lookups on
 
  #-------------- file test.conf --------------

=head1 PROCESS FLOW

See L<Net::Server>

=head1 HOOKS

There are no additional hooks in Net::Server::MultiType.

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

