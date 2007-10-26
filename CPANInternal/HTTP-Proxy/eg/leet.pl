#!/usr/bin/perl -w
use HTTP::Proxy qw( :log );
use HTTP::Proxy::BodyFilter::tags;
use HTTP::Proxy::BodyFilter::htmltext;
use strict;

# a very simple proxy
my $proxy = HTTP::Proxy->new( @ARGV );

my %leet = (
    a   => [qw( 4 /-\ @ )],
    b   => ['|3'],
    c   => [qw! c ( &lt; [ !],
    e   => [qw( e 3 )],
    g   => [qw( g 6 )],
    h   => [qw! h |-| )-( !],
    k   => [qw( k |&lt; ]{ )],
    i   => ['i', '!'],
    l   => [ 'l', "1", "|" ],
    m   => [ 'm', "|V|", "|\\/|" ],
    n   => ["|\\|"],
    o   => ['o', "0"],
    s   => [ "5", "Z" ],
    t   => [ "7", "+" ],
    u   => [qw( u \_/ )],
    v   => [qw( v \/ )],
    w   => [qw( vv `// )],
    'y' => ['j', '`/'],
    z   => ["2"],
);

# but a complicated filter
$proxy->push_filter(
    mime     => 'text/html',
    response => HTTP::Proxy::BodyFilter::tags->new,
    response => HTTP::Proxy::BodyFilter::htmltext->new(
        sub {
            s/([a-zA-Z])/$leet{lc $1}[rand @{$leet{lc $1}}]||$1/ge;
        }
    )
);

$proxy->start;

