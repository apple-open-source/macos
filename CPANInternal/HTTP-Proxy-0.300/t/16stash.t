use strict;
use Test::More tests => 7;
use HTTP::Proxy;

my $proxy;

$proxy = HTTP::Proxy->new;
is_deeply( $proxy->stash, {}, "Empty stash by default" );

$proxy = HTTP::Proxy->new( stash => { clunk => 'slosh', plop => 'biff' } );
is( $proxy->stash('clunk'), 'slosh', "get clunk from stash" );
is( $proxy->stash('plop'),  'biff',  "get plop from stash" );
is_deeply(
    $proxy->stash,
    { clunk => 'slosh', plop => 'biff' },
    "the whole hash"
);

is( $proxy->stash( clunk => 'sock' ), 'sock', "set returns the new value" );
is( $proxy->stash('clunk'), 'sock', "the new value is set" );

my $h = $proxy->stash;
%$h = ( thwack => 'spla_a_t', rip => 'uggh', zowie => 'thwape' );
is_deeply(
    $proxy->stash,
    { thwack => 'spla_a_t', rip => 'uggh', zowie => 'thwape' },
    "stash() is a reference to the stash itself"
);
