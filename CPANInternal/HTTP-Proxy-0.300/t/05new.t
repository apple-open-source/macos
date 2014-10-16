use Test::More tests => 10;

use HTTP::Proxy qw( :log );

my $proxy;

$proxy = HTTP::Proxy->new;

# check for defaults
is( $proxy->logmask, NONE, 'Default log mask' );
is( $proxy->port, 8080,        'Default port' );
is( $proxy->host, 'localhost', 'Default host' );
is( $proxy->agent, undef, 'Default agent' );

# new with arguments
$proxy = HTTP::Proxy->new(
    port    => 3128,
    host    => 'foo',
    logmask => STATUS,
);

is( $proxy->port, 3128, 'port set by new' );
is( $proxy->logmask, STATUS, 'verbosity set by new' );
is( $proxy->host, 'foo', 'host set by new' );

# check the accessors
is( $proxy->logmask(NONE), STATUS, 'logmask accessor' );
is( $proxy->logmask, NONE, 'logmask changed by accessor' );

# check a read-only accessor
my $conn = $proxy->conn;
$proxy->conn( $conn + 100 );
is( $proxy->conn, $conn, 'read-only attribute' );

