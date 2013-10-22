use Test::More;
use strict;
BEGIN { plan tests => 1 };

use CGI;
use JSON::RPC::Legacy::Client;
use JSON::RPC::Legacy::Server;

ok(1); # If we made it this far, we're ok.

END {
    warn "\nJSON::RPC::nLegacy::Server::CGI requires CGI.pm (>= 2.9.2)." if(CGI->VERSION < 2.92);
}

