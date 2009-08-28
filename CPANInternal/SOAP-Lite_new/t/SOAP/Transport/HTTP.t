use strict;
use Test::More qw(no_plan);
use_ok qw(SOAP::Transport::HTTP);

# Try mocking LWP::UserAgent to simulate sending something over the wire.
# Skip if we don't have Test::MockObject.
SKIP: {
    eval "require Test::MockObject"
        or skip "Cannot simulate transport layer without Test::MockObject", 3;

    require HTTP::Response;

    my $mock = Test::MockObject->new();
    $mock->fake_module( 'LWP::UserAgent',
        'new' => sub { return bless {}, $_[0] },
        'agent' => sub {
            return $_[1]
                ? $_[0]->{ agent } = $_[1]
                : $_[0]->{ agent }
        },
        # TODO return something meaningful
        'request' => sub {
            return HTTP::Response->new(200, '200 OK');
        },
    );
    my $client;
    ok $client = SOAP::Transport::HTTP::Client->new();
    $client->send_receive();
    is $client->code(), '200';
    is $client->message(), '200 OK';
}

# client
my $client;
ok $client = SOAP::Transport::HTTP::Client->new(), 'SOAP::Transport::HTTP::Client->new()';

# just use twice to avoid warning
undef $SOAP::Constants::PATCH_HTTP_KEEPALIVE;
undef $SOAP::Constants::PATCH_HTTP_KEEPALIVE;
ok $client = SOAP::Transport::HTTP::Client->new(), 'SOAP::Transport::HTTP::Client->new() - PATCH_KEEPALIVE = undef';

is $client, $client->new(), '$client->new() returns $client';

is $client->http_request('foo'), $client;
is $client->http_request(), 'foo';

is $client->http_response('foo'), $client;
is $client->http_response(), 'foo';
undef $client;

# package SOAP::Transport::HTTP::Server;
my $server;
ok $server = SOAP::Transport::HTTP::Server->new(), 'SOAP::Transport::HTTP::Server->new()';
isa_ok $server, 'SOAP::Transport::HTTP::Server';
isa_ok $server, 'SOAP::Server';
is $server, $server->new(), '$server->new() returns $server';

like $server->product_tokens(), qr{SOAP::Lite}x;

test_make_fault($server);

undef $server;

# package SOAP::Transport::HTTP::CGI;

ok $server = SOAP::Transport::HTTP::CGI->new();
isa_ok $server, 'SOAP::Transport::HTTP::Server';
isa_ok $server, 'SOAP::Server';

test_make_fault($server);

# package SOAP::Transport::HTTP::Daemon
my $transport;

ok $transport = SOAP::Transport::HTTP::Daemon->new(), 'SOAP::Transport::HTTP::Daemon->new()';
is $transport, $transport->new(), '$transport->new() is $transport';
is $transport->SSL(1), $transport;
is $transport->SSL(), 1;
is $transport->http_daemon_class(), 'HTTP::Daemon::SSL';
is $transport->SSL(0), $transport;
is $transport->SSL(), 0;
is $transport->http_daemon_class(), 'HTTP::Daemon';

undef $transport;

# package SOAP::Transport::HTTP::Apache is untestable under mod_perl 1.x
# due to missing exports in Apache::Constant

SKIP: {
    eval "require FCGI;"
        or skip "Can't test without FCGI", 1;

    # package SOAP::Transport::HTTP::FCGI
    ok $transport = SOAP::Transport::HTTP::FCGI->new(), 'SOAP::Transport::HTTP::FCGI->new()';
    undef $transport;

}


sub test_make_fault {
    my $server = shift;
    # try creating a fault
    my $request = HTTP::Request->new();
    is $server->request($request), $server, '$server->request($request)';
    is $server->request(), $request, '$server->request()';
    $server->make_fault();
    is $server->response()->code(), 500, 'fault response code is 500';
    like $server->response->content(), qr{\bFault\b}x, 'Fault content';
}
