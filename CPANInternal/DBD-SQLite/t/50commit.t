#!/usr/local/bin/perl
#
#   $Id: 50commit.t,v 1.2 2007/01/30 07:01:06 wkakes Exp $
#
#   This is testing the transaction support.
#
$^W = 1;


#
#   Include lib.pl
#
require DBI;
$mdriver = "";
foreach $file ("lib.pl", "t/lib.pl") {
    do $file; if ($@) { print STDERR "Error while executing lib.pl: $@\n";
			   exit 10;
		      }
    if ($mdriver ne '') {
	last;
    }
}
if ($mdriver eq 'whatever') {
    print "1..0\n";
    exit 0;
}


use vars qw($gotWarning);
sub CatchWarning ($) {
    $gotWarning = 1;
}


sub NumRows($$$) {
    my($dbh, $table, $num) = @_;
    my($sth, $got);

    if (!($sth = $dbh->prepare("SELECT * FROM $table"))) {
	return "Failed to prepare: err " . $dbh->err . ", errstr "
	    . $dbh->errstr;
    }
    if (!$sth->execute) {
	return "Failed to execute: err " . $dbh->err . ", errstr "
	    . $dbh->errstr;
    }
    $got = 0;
    while ($sth->fetchrow_arrayref) {
	++$got;
    }
    if ($got ne $num) {
	return "Wrong result: Expected $num rows, got $got.\n";
    }
    return '';
}

#
#   Main loop; leave this untouched, put tests after creating
#   the new table.
#
while (Testing()) {
    #
    #   Connect to the database
    Test($state or ($dbh = DBI->connect($test_dsn, $test_user,
					$test_password)),
	 'connect',
	 "Attempting to connect.\n")
	or ErrMsgF("Cannot connect: Error %s.\n\n"
		   . "Make sure, your database server is up and running.\n"
		   . "Check that '$test_dsn' references a valid database"
		   . " name.\nDBI error message: %s\n",
		   $DBI::err, $DBI::errstr);

    #
    #   Find a possible new table name
    #
    Test($state or $table = FindNewTable($dbh))
	or ErrMsgF("Cannot determine a legal table name: Error %s.\n",
		   $dbh->errstr);

    #
    #   Create a new table
    #
    Test($state or ($def = TableDefinition($table,
					   ["id",   "INTEGER",  4, 0],
					   ["name", "CHAR",    64, 0]),
		    $dbh->do($def)))
	or ErrMsgF("Cannot create table: Error %s.\n",
		   $dbh->errstr);

    Test($state or $dbh->{AutoCommit})
	or ErrMsg("AutoCommit is off\n", 'AutoCommint on');

    #
    #   Tests for databases that do support transactions
    #
    if (HaveTransactions()) {
	# Turn AutoCommit off
	$dbh->{AutoCommit} = 0;
	Test($state or (!$dbh->err && !$dbh->errstr && !$dbh->{AutoCommit}))
	    or ErrMsgF("Failed to turn AutoCommit off: err %s, errstr %s\n",
		       $dbh->err, $dbh->errstr);

	# Check rollback
	Test($state or $dbh->do("INSERT INTO $table VALUES (1, 'Jochen')"))
	    or ErrMsgF("Failed to insert value: err %s, errstr %s.\n",
		       $dbh->err, $dbh->errstr);
	my $msg;
	Test($state or !($msg = NumRows($dbh, $table, 1)))
	    or ErrMsg($msg);
	Test($state or $dbh->rollback)
	    or ErrMsgF("Failed to rollback: err %s, errstr %s.\n",
		       $dbh->err, $dbh->errstr);
	Test($state or !($msg = NumRows($dbh, $table, 0)))
	    or ErrMsg($msg);

	# Check commit
	Test($state or $dbh->do("DELETE FROM $table WHERE id = 1"))
	    or ErrMsgF("Failed to insert value: err %s, errstr %s.\n",
		       $dbh->err, $dbh->errstr);
	Test($state or !($msg = NumRows($dbh, $table, 0)))
	    or ErrMsg($msg);
	Test($state or $dbh->commit)
	    or ErrMsgF("Failed to rollback: err %s, errstr %s.\n",
		       $dbh->err, $dbh->errstr);
	Test($state or !($msg = NumRows($dbh, $table, 0)))
	    or ErrMsg($msg);

	# Check auto rollback after disconnect
	Test($state or $dbh->do("INSERT INTO $table VALUES (1, 'Jochen')"))
	    or ErrMsgF("Failed to insert: err %s, errstr %s.\n",
		       $dbh->err, $dbh->errstr);
	Test($state or !($msg = NumRows($dbh, $table, 1)))
	    or ErrMsg($msg);
	Test($state or $dbh->disconnect)
	    or ErrMsgF("Failed to disconnect: err %s, errstr %s.\n",
		       $dbh->err, $dbh->errstr);
	Test($state or ($dbh = DBI->connect($test_dsn, $test_user,
					    $test_password)))
	    or ErrMsgF("Failed to reconnect: err %s, errstr %s.\n",
		       $DBI::err, $DBI::errstr);
	Test($state or !($msg = NumRows($dbh, $table, 0)))
	    or ErrMsg($msg);

	# Check whether AutoCommit is on again
	Test($state or $dbh->{AutoCommit})
	    or ErrMsg("AutoCommit is off\n");

    #
    #   Tests for databases that don't support transactions
    #
    } else {
	if (!$state) {
	    $@ = '';
	    eval { $dbh->{AutoCommit} = 0; }
	}
	Test($state or $@)
	    or ErrMsg("Expected fatal error for AutoCommit => 0\n",
		      'AutoCommit off -> error');
    }

    #   Check whether AutoCommit mode works.
    Test($state or $dbh->do("INSERT INTO $table VALUES (1, 'Jochen')"))
	or ErrMsgF("Failed to delete: err %s, errstr %s.\n",
		   $dbh->err, $dbh->errstr);
    Test($state or !($msg = NumRows($dbh, $table, 1)), 'NumRows')
	or ErrMsg($msg);
    Test($state or $dbh->disconnect, 'disconnect')
	or ErrMsgF("Failed to disconnect: err %s, errstr %s.\n",
		   $dbh->err, $dbh->errstr);
    Test($state or ($dbh = DBI->connect($test_dsn, $test_user,
					$test_password)))
	or ErrMsgF("Failed to reconnect: err %s, errstr %s.\n",
		   $DBI::err, $DBI::errstr);
    Test($state or !($msg = NumRows($dbh, $table, 1)))
	or ErrMsg($msg);

    #   Check whether commit issues a warning in AutoCommit mode
    Test($state or $dbh->do("INSERT INTO $table VALUES (2, 'Tim')"))
	or ErrMsgF("Failed to insert: err %s, errstr %s.\n",
		   $dbh->err, $dbh->errstr);
    my $result;
    if (!$state) {
	$@ = '';
	$SIG{__WARN__} = \&CatchWarning;
	$gotWarning = 0;
	eval { $result = $dbh->commit; };
	$SIG{__WARN__} = 'DEFAULT';
    }
    Test($state or $gotWarning)
	or ErrMsg("Missing warning when committing in AutoCommit mode");

    #   Check whether rollback issues a warning in AutoCommit mode
    #   We accept error messages as being legal, because the DBI
    #   requirement of just issueing a warning seems scary.
    Test($state or $dbh->do("INSERT INTO $table VALUES (3, 'Alligator')"))
	or ErrMsgF("Failed to insert: err %s, errstr %s.\n",
		   $dbh->err, $dbh->errstr);
    if (!$state) {
	$@ = '';
	$SIG{__WARN__} = \&CatchWarning;
	$gotWarning = 0;
	eval { $result = $dbh->rollback; };
	$SIG{__WARN__} = 'DEFAULT';
    }
    Test($state or $gotWarning or $dbh->err)
	or ErrMsg("Missing warning when rolling back in AutoCommit mode");


    #
    #   Finally drop the test table.
    #
    Test($state or $dbh->do("DROP TABLE $table"))
	or ErrMsgF("Cannot DROP test table $table: %s.\n",
		   $dbh->errstr);
    Test($state or $dbh->disconnect())
	or ErrMsgF("Cannot DROP test table $table: %s.\n",
		   $dbh->errstr);
}
