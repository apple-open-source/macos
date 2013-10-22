#   -*- perl -*-
#
#
#   PlRPC - Perl RPC, package for writing simple, RPC like clients and
#       servers
#
#
#   Copyright (c) 1997,1998  Jochen Wiedmann
#
#   You may distribute under the terms of either the GNU General Public
#   License or the Artistic License, as specified in the Perl README file.
#
#   Author: Jochen Wiedmann
#           Email: jochen.wiedmann at freenet.de
#

require 5.004;
use strict;

require RPC::PlServer::Comm;


package RPC::PlClient::Comm;


$RPC::PlClient::Comm::VERSION = '0.1002';
@RPC::PlClient::Comm::ISA = qw(RPC::PlServer::Comm);


sub getMaxMessage() { return 0; }


1;
