use strict;
use Test::More tests => 9;

use_ok qw(IO::SessionData);
my $session;
ok $session = IO::SessionData->new(
    undef, 'foo', 'writeonly'
);

is $session->handle(), 'foo';
is $session->sessions(), undef;
is $session->pending(), 0;

is $session->write_limit(42), 42, 'write_limit(42)';
is $session->write_limit(), 42;

is $session->set_choke(42), 42, 'set_choke(42)';
is $session->set_choke(), 42;