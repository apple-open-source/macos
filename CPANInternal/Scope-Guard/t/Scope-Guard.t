# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Scope-Guard.t'

use blib;
use strict;
use warnings;

use Test::More tests => 8;

BEGIN { use_ok('Scope::Guard') };

my $i = 1;

{
    my $sg = Scope::Guard->new(sub { ok($i++ == 1, 'handler invoked at scope end') });
}

sub {
    my $sg = Scope::Guard->new(sub { ok($i++ == 2, 'handler invoked on return') });
    return;
}->();

eval {
    my $sg = Scope::Guard->new(sub { ok($i++ == 3, 'handler invoked on exception') });
    my $j = 0;
    my $k = $j / $j;
};

like($@, qr{^Illegal division by zero}, 'exception was raised');

{
    my $sg = Scope::Guard->new(sub { ++$i });
    $sg->dismiss();
}

ok($i++ == 4, 'dismiss() disables handler');

{
    my $sg = Scope::Guard->new(sub { ++$i });
    $sg->dismiss(1);
}

ok($i++ == 5, 'dismiss(1) disables handler');

{
    my $sg = Scope::Guard->new(sub { ok($i++ == 6, 'dismiss(0) enables handler') });
    $sg->dismiss();
    $sg->dismiss(0);
}
