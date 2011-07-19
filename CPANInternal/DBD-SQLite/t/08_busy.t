#!/usr/bin/perl

# Test that two processes can write at once, assuming we commit timely.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test qw/connect_ok @CALL_FUNCS/;
use Test::More;
use Test::NoWarnings;

plan tests => 11 * @CALL_FUNCS + 1;

foreach my $call_func (@CALL_FUNCS) {

	my $dbh = connect_ok(
	    dbfile     => 'foo',
	    RaiseError => 1,
	    PrintError => 0,
	    AutoCommit => 0,
	);

	my $dbh2 = connect_ok(
	    dbfile     => 'foo',
	    RaiseError => 1,
	    PrintError => 0,
	    AutoCommit => 0,
	);

	# NOTE: Let's make it clear what we're doing here.
	# $dbh starts locking with the first INSERT statement.
	# $dbh2 tries to INSERT, but as the database is locked,
	# it starts waiting. However, $dbh won't release the lock.
	# Eventually $dbh2 gets timed out, and spits an error, saying
	# the database is locked. So, we don't need to let $dbh2 wait
	# too much here. It should be timed out anyway.
	ok($dbh2->$call_func(300, 'busy_timeout'));

	ok($dbh->do("CREATE TABLE Blah ( id INTEGER, val VARCHAR )"));
	ok($dbh->commit);
	ok($dbh->do("INSERT INTO Blah VALUES ( 1, 'Test1' )"));
	eval {
	    $dbh2->do("INSERT INTO Blah VALUES ( 2, 'Test2' )");
	};
	ok($@);
	if ($@) {
	    print "# expected insert failure : $@";
	    $dbh2->rollback;
	}

	$dbh->commit;
	ok($dbh2->do("INSERT INTO Blah VALUES ( 2, 'Test2' )"));
	$dbh2->commit;

	$dbh2->disconnect;
	undef($dbh2);

	# NOTE: The second test is to see what happens if a lock is
	# is released while waiting. When both parent and child are
	# ready, the database is locked by the child. The parent
	# starts waiting for a long enough time (apparently we need
	# to wait much longer than we expected, as testers may use
	# very slow (virtual) machines to test, but don't worry,
	# it's only for the slowest environment). After a short sleep,
	# the child commits and releases the lock. Eventually the parent
	# notices that, and does the pended INSERT (hopefully before
	# it is timed out). As both the parent and the child wait till
	# both are ready, we don't need to sleep for a long time.
	pipe(READER, WRITER);
	my $pid = fork;
	if (!defined($pid)) {
	    # fork failed
	    skip("No fork here", 1);
	    skip("No fork here", 1);
	} elsif (!$pid) {
	    # child
	    my $dbh2 = DBI->connect('dbi:SQLite:foo', '', '', 
	    {
	        RaiseError => 1,
	        PrintError => 0,
	        AutoCommit => 0,
	    });
	    $dbh2->do("INSERT INTO Blah VALUES ( 3, 'Test3' )");
	    select WRITER; $| = 1; select STDOUT;
	    print WRITER "Ready\n";
	    sleep(2);
	    $dbh2->commit;
	    $dbh2->disconnect;
	    exit;
	} else {
	    # parent
	    close WRITER;
	    my $line = <READER>;
	    chomp($line);
	    ok($line, "Ready");
	    ok($dbh->$call_func(100000, 'busy_timeout'));
	    eval { $dbh->do("INSERT INTO Blah VALUES (4, 'Test4' )") };
	    ok !$@;
	    if ($@) {
	        print STDERR "# Your testing environment might be too slow to pass this test: $@";
	        $dbh->rollback;
	    }
	    else {
	        $dbh->commit;
	    }
	    wait;
	    $dbh->disconnect;
	    unlink 'foo';
	}
}
