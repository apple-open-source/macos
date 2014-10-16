use strict;
use Test::More tests => 6;
use_ok qw(IO::SessionSet);

is IO::SessionSet->SessionDataClass(), 'IO::SessionData', 'SessionDataClass';

my $set;

ok $set = IO::SessionSet->new(), 'new';
is scalar $set->sessions(), 0, 'sessions';

is $set->to_handle('foo'), undef, "to_handle('foo');";
is $set->to_session('foo'), undef, "to_session('foo');";
