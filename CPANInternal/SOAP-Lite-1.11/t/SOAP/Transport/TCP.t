use strict;
use Test::More tests => 7;

use_ok qw(SOAP::Transport::TCP);

my $transport;

ok $transport = SOAP::Transport::TCP::Client->new(
    endpoint => 'tcp://127.0.0.1:0'
), 'new()';

is $transport, $transport->new(), '$transport->new() returns $transport';
{
    no warnings qw(uninitialized);
    $transport->send_receive();
}
ok ! $transport->is_success(), 'is_success() is false';
like $transport->status(), qr{ Cannot }x;
like $transport->message(), qr{ Cannot }x;
like $transport->code(), qr{ Cannot }x;
