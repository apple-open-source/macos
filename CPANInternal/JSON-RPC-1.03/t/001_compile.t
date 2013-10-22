use strict;
use Test::More;

use_ok $_ for qw(
    JSON::RPC::Constants
    JSON::RPC::Dispatch
    JSON::RPC::Parser
    JSON::RPC::Procedure
);

done_testing;

1;
