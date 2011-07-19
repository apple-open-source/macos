#!/usr/bin/perl -w

# JSON-RPC Server (daemon version)

use strict;
use JSON::RPC::Server::CGI;

my $server = JSON::RPC::Server::CGI->new;

$server->dispatch_to({'/API' => 'MyApp', '/API/Subclass' => 'MyApp::Subclass'})->handle();


__END__
