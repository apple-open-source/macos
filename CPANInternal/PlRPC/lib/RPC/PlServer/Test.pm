# -*- perl -*-

use strict;
require 5.004;


require RPC::PlServer;
require Net::Daemon::Test;


package RPC::PlServer::Test;

$RPC::PlServer::Test::VERSION = '0.01';
@RPC::PlServer::Test::ISA = qw(RPC::PlServer);

@RPC::PlServer::ISA = qw(Net::Daemon::Test);
