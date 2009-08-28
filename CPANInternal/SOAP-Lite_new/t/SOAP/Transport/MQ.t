# I don't have any idea how MQ works, and I don't have it handy.
# I'll just mock enough of MQ for testing SOAP::Lite's module, which may
# or may not be correct.
use strict;
use Test::More;
# use SOAP::Lite +trace;
eval "require Test::MockObject"
    or plan skip_all => 'Cannot test without Test__MockObject';
plan tests => 12;

my $mock = Test::MockObject->new();
$mock->fake_module('MQClient::MQSeries',
    new => sub { return bless {}, $_[0]; },
    
);
$mock->fake_module('MQSeries::QueueManager',
    new => sub { return bless {}, $_[0]; },
    
);

# This is quite difficult.
# The server's handle method pulls out a message, then puts one in in a loop.
# handle() terminates if Reason returns MQRC_NO_MSG_AVAILABLE (23).
# So, decrementing the count and returning 23 allows handle to grab one 
# message, and terminate before putting another one in the reply queue.
# The server's parent's handle() is never called this way.
# This behaviour could also be influenced by testing the client, so be 
# aware of it when adding tests.

my $MESSAGE_COUNT = 1;
$mock->fake_module('MQSeries::Queue',
    new => sub { return bless {}, $_[0]; },
    Get => sub {
        my ($self , %arg_of) = @_;
        return $MQSeries::Queue::GET
            ? $arg_of{ Message }->Data( $MQSeries::Queue::GET )
            : ();
    },
    Reason => sub {
        if ($MQSeries::Queue::GET) {
            undef $MQSeries::Queue::GET;
            return --$MESSAGE_COUNT ? () : 23;
        }
        return;
    },
    Put => sub {
        return 1;
    },
);
$mock->fake_module('MQSeries::Message',
    new => sub { return bless {}, $_[0]; },
    MsgDesc => sub {},
    Data => sub {
        return $#_
            ? $_[0]->{ Data } = $_[1]
            : $_[0]->{ Data };
    },

);

$mock->fake_module('MQSeries',
    new => sub { return bless {}, $_[0]; },
    import => sub { my $caller = caller();
        # warn $caller;
        no strict qw(refs);
        *{ "$caller\::MQFMT_STRING" } = sub { 42 };
        *{ "$caller\::MQRC_NO_MSG_AVAILABLE" } = sub { 23 };
    },
    
);

use_ok qw(SOAP::Transport::MQ);

my $transport = SOAP::Transport::MQ::Client->new();
isa_ok $transport, 'SOAP::Client';
is $transport, $transport->new(), '$transport->new() is $transport';

my $endpoint = 'mq://user@localhost:42?Channel=A;QueueManager=B;RequestQueue=C;ReplyQueue=D';
is $transport->endpoint($endpoint)
    , $transport, '$transport->endpoint($endpoint) is $transport';
is $transport->endpoint($endpoint)
    , $transport, '$transport->endpoint($endpoint) is $transport';
is $transport->endpoint(), $endpoint, '$transport->endpoint() is $endpoint';

isa_ok $transport->requestqueue(), 'MQSeries::Queue';
isa_ok $transport->replyqueue(), 'MQSeries::Queue';

 
# server

my $server;
eval { $server = SOAP::Transport::MQ::Server->new() };
like $@, qr{ \A missing \s parameter \s \(uri\) \s}xms, 'new throws without uri';
ok $server = SOAP::Transport::MQ::Server->new($endpoint), 'new($endpoint)';

eval { $server->handle() };
is $@, "Error occured while waiting for requests\n", 'error without message';

$MQSeries::Queue::GET = q{<?xml version="1.0"?>
<SOAP-ENV:Envelope
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/">
    <SOAP-ENV:Body>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>};

is $server->handle(), 0, 'Count after one message is 0';