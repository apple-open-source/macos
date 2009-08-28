use strict;
use Test::More qw(no_plan);

use_ok qw(SOAP::Transport::POP3);

my $uri = 'pop://user@mail.example.org';

my $server;
#ok $server = SOAP::Transport::POP3::Server->new(
#    # $uri
#);
#is $server->new(), $server, '$server->new() is $server';