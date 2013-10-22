use Test::More;
use strict;
BEGIN { plan tests => 4 };

use JSON::RPC::Legacy::Server;

my $server = JSON::RPC::Legacy::Server->new;

isa_ok($server, 'JSON::RPC::Legacy::Server');

isa_ok($server->json, 'JSON');

my $test = JSON::RPC::Legacy::Server::Test->new;

isa_ok($test, 'JSON::RPC::Legacy::Server');

isa_ok($test->json, 'DummyJSONCoder');


####

package JSON::RPC::Legacy::Server::Test;

use base qw(JSON::RPC::Legacy::Server);


sub create_json_coder {
    bless {}, 'DummyJSONCoder';
}
