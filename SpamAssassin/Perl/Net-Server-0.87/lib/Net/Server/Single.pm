# -*- perl -*-
#
#  Net::Server::Single - Net::Server personality
#  
#  $Id: Single.pm,v 1.1 2004/04/19 17:50:29 dasenbro Exp $
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

package Net::Server::Single;

use strict;
use vars qw($VERSION @ISA);
use Net::Server;


$VERSION = $Net::Server::VERSION; # done until separated

### fall back to parent methods
@ISA = qw(Net::Server);

### this module is simple a place holder so that
### Net::Server::MultiType can ask for Single as one of
### the fall back methods (which it does any way).
### Essentially all we are doing here is providing parallelism.

1;

__END__

=head1 NAME

Net::Server::Single - Net::Server personality

=head1 SYNOPSIS

  use Net::Server::MultiType;
  @ISA = qw(Net::Server::MultiType);

  sub process_request {
     #...code...
  }

  my @types = qw(PreFork Single Fork);

  Net::Server::MultiType->run(server_type=>\@types);

=head1 DESCRIPTION

This module offers no functionality beyond the Net::Server
base class.  This modules only purpose is to provide
parallelism for the MultiType personality.

See L<Net::Server>

=cut
