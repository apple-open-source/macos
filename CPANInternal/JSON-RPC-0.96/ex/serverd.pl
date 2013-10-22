#!/usr/bin/perl -w

# JSON-RPC Server (daemon version)

use JSON::RPC::Server::Daemon;

my $server = JSON::RPC::Server::Daemon->new(LocalPort => 8080);

$server->dispatch_to({'/jsonrpc/API' => 'MyApp', '/jsonrpc/API/Subclass' => 'MyApp::Subclass'})->handle();

__END__
