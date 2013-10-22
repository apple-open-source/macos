use Test::More;
use strict;
BEGIN { plan tests => 1 };

use CGI;
use JSON::RPC::Client;
use JSON::RPC::Server;

ok(1); # If we made it this far, we're ok.

END {
    warn "\nJSON::RPC::Server::CGI requires CGI.pm (>= 2.9.2)." if(CGI->VERSION < 2.92);
}

