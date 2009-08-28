#!perl -w
$|=1;

# --- Test DBI support for threads created after the DBI was loaded

BEGIN { eval "use threads;" }	# Must be first
my $use_threads_err = $@;

use strict;
use Config qw(%Config);
use Test::More;

BEGIN {
    if (!$Config{useithreads} || $] < 5.008001) {
	plan skip_all => "this $^O perl $] not supported for DBI iThreads";
    }
    die $use_threads_err if $use_threads_err; # need threads
}

my $threads = 10;
plan tests => 4 + 4 * $threads;

{
    package threads_sub;
    use base qw(threads);
}

use_ok('DBI');

$DBI::PurePerl = $DBI::PurePerl; # just to silence used only once warning
$DBI::neat_maxlen = 12345;
cmp_ok($DBI::neat_maxlen, '==', 12345, '... assignment of neat_maxlen was successful');

my @connect_args = ("dbi:ExampleP:", '', '');

my $dbh_parent = DBI->connect_cached(@connect_args);
isa_ok( $dbh_parent, 'DBI::db' );

# this our function for the threads to run

sub testing {
    cmp_ok($DBI::neat_maxlen, '==', 12345, '... DBI::neat_maxlen still holding its value');

    my $dbh = DBI->connect_cached(@connect_args);
    isa_ok( $dbh, 'DBI::db' );
    isnt($dbh, $dbh_parent, '... new $dbh is not the same instance as $dbh_parent');
 
    SKIP: {
	# skip seems broken with threads (5.8.3)
	# skip "Kids attribute not supported under DBI::PurePerl", 1 if $DBI::PurePerl;

        cmp_ok($dbh->{Driver}->{Kids}, '==', 1, '... the Driver has one Kid')
		unless $DBI::PurePerl && ok(1);
    }
}

# load up the threads

my @thr;
push @thr, threads_sub->create( \&testing )
    or die "thread->create failed ($!)"
    foreach (1..$threads);

# join all the threads

foreach my $thread (@thr) {
    $thread->join;
}

pass('... all tests have passed');

1;
