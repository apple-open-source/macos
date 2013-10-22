use Test::More;
use strict;
BEGIN { plan tests => 4 };

use JSON::RPC::Server;

my $server = JSON::RPC::Server->new;

isa_ok($server, 'JSON::RPC::Server');

isa_ok($server->json, 'JSON');

my $test = JSON::RPC::Server::Test->new;

isa_ok($test, 'JSON::RPC::Server');

isa_ok($test->json, 'DummyJSONCoder');


####

package JSON::RPC::Server::Test;

use base qw(JSON::RPC::Server);


sub create_json_coder {
    bless {}, 'DummyJSONCoder';
}
