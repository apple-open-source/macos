use strict;
use Test::More;
eval { require Test::MockObject }
    or plan skip_all => 'Cannot test without Test::MockObject';

plan tests => 15;

my $mock = Test::MockObject->new();

$mock->fake_module('MIME::Lite',
    new => sub { my $class = shift;
        my %arg_of = @_;
        return bless { %arg_of }, $class;
    },
    send => sub {},
    replace => sub {},
    add => sub {},
    as_string => sub {},
);

use_ok qw(SOAP::Transport::MAILTO);

my $transport;

ok $transport = SOAP::Transport::MAILTO::Client->new(
    endpoint => 'mailto:test@example.org'
), 'new( endpoint => mailto:test@example.org';
is $transport, $transport->new() , '$transport->new() returns $transport';

test_send_receive($transport);

$transport = SOAP::Transport::MAILTO::Client->new(
    smtp => 'smtp.example.org',
    From => 'test@example.org',
    'Reply-To' => 'test@example.org',
    Subject => 'MAILTO.t',
    Encoding => 'Quoted-printable',
);
test_send_receive($transport, endpoint => 'mailto:arg_of@example.org');
test_send_receive($transport, endpoint => 'mailto:');

sub test_send_receive {
    my $transport = shift;
    my %arg_of = @_;
    $transport->send_receive(%arg_of);

    ok $transport->is_success(), 'is_success() is true';
    ok ! $transport->code(), 'code() is false';
    ok ! $transport->message(), 'message() is false';
    ok ! $transport->status(), 'status() is false';
}