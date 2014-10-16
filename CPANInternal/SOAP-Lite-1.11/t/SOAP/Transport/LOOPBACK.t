use strict;
use warnings;
use Test::More qw(no_plan);

use_ok qw(SOAP::Transport::LOOPBACK);

my $transport;

ok $transport = SOAP::Transport::LOOPBACK::Client->new();
is $transport, $transport->new() , '$transport->new() returns $transport';

use_ok qw(SOAP::Lite);

my $soap = SOAP::Lite->proxy('loopback:main');
my $som = $soap->call('test', 'Zumsel');
die $som->fault()->{ faultstring } if $som->fault();
is $som->result, 'Zumsel', 'result is Zumsel';

