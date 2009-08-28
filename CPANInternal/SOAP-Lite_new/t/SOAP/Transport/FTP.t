use strict;
use Test::More tests => 7;

use_ok qw(SOAP::Transport::FTP);

my $transport;

ok $transport = SOAP::Transport::FTP::Client->new(), 'new()';

is $transport, $transport->new(), '$transport->new() returns $transport';

$transport->send_receive();

ok ! $transport->is_success(), 'is_success() is false';
like $transport->status(), qr{ Can't \s connect }x;
like $transport->message(), qr{ Can't \s connect }x;
like $transport->code(), qr{ Can't \s connect }x;
