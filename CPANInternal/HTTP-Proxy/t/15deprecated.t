use strict;
use Test::More;
use File::Spec;
use HTTP::Proxy;

my $errfile = File::Spec->catfile( 't', 'stderr.out' );
my @deprecated = (
    [ maxchild => qr/^maxchild is deprecated, please use max_clients/ ],
    [ maxconn  => qr/^maxconn is deprecated, please use max_connections/ ],
    [
        maxserve =>
          qr/^maxserve is deprecated, please use max_keep_alive_requests/
    ],
);

plan tests => 4 * @deprecated;

# check the warnings
open my $olderr, ">&STDERR" or die "Can't dup STDERR: $!";
open STDERR, '>', $errfile or die "Can't redirect STDERR: $!";
select STDERR;
$| = 1;    # make unbuffered

# call the deprecated methods
my $p1 = HTTP::Proxy->new(
    maxchild => 1,
    maxconn  => 3,
    maxserve => 5,
);

my $p2 = HTTP::Proxy->new();
$p2->maxchild(9);
$p2->maxconn(8);
$p2->maxserve(7);

# get the old STDERR back
open STDERR, ">&", $olderr or die "Can't dup \$olderr: $!";

# read the stderr log
open my $fh, $errfile or die "Can't open $errfile: $!";
my @err = sort <$fh>;
close $fh or diag "Can't close $errfile: $!";

# run the tests
for (@deprecated) {
    like( shift @err, $_->[1], "$_->[0] is deprecated" );
    like( shift @err, $_->[1], "$_->[0] is deprecated" );
}
diag $_ for @err;

unlink $errfile or diag "Can't unlink $errfile: $!";

# check that the real method was called
is( $p1->max_clients, 1, "max_clients called for maxchild" );
is( $p1->max_connections, 3, "max_connections called for maxconn" );
is( $p1->max_keep_alive_requests, 5, "max_keep_alive_requests called for maxserve" );

is( $p2->max_clients, 9, "max_clients called for maxchild" );
is( $p2->max_connections, 8, "max_connections called for maxconn" );
is( $p2->max_keep_alive_requests, 7, "max_keep_alive_requests called for maxserve" );
