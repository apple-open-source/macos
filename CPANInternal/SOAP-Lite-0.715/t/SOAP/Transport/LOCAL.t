use strict;
use Test::More qw(no_plan);

use_ok qw(SOAP::Transport::LOCAL);

my $transport;

ok $transport = SOAP::Transport::LOCAL::Client->new();

# call methods if known, pass through the rest as arguments
#
# ->new calls the methods $self->endpoint('local:main') and $self->options({}),
# and calls SUPER::new("foo");
#
ok $transport = SOAP::Transport::LOCAL::Client->new(endpoint => 'local:main', options => [], "foo");
is $transport, $transport->new() , '$transport->new() returns $transport';

use_ok qw(SOAP::Lite);

my $soap = SOAP::Lite->proxy('local:main');
$soap->transport()->dispatch_to('main');

my $som = $soap->call('test');
die $som->fault()->{ faultstring } if $som->fault();
is $som->result, 'Zumsel', 'result is Zumsel';

sub test {
    return "Zumsel";
}
