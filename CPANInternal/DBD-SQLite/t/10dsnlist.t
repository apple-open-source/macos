#!/usr/local/bin/perl
#
#   $Id: 10dsnlist.t,v 1.2 2007/01/30 07:01:06 wkakes Exp $
#
#   This test creates a database and drops it. Should be executed
#   after listdsn.
#


#
#   Include lib.pl
#
require DBI;
$mdriver = "";
foreach $file ("lib.pl", "t/lib.pl", "DBD-~DBD_DRIVER~/t/lib.pl") {
    do $file; if ($@) { print STDERR "Error while executing lib.pl: $@\n";
			   exit 10;
		      }
    if ($mdriver ne '') {
	last;
    }
}
if ($mdriver eq 'pNET'  ||  $mdriver eq 'Adabas') {
    print "1..0\n";
    exit 0;
}
print "Driver is $mdriver\n";

sub ServerError() {
    print STDERR ("Cannot connect: ", $DBI::errstr, "\n",
	"\tEither your server is not up and running or you have no\n",
	"\tpermissions for acessing the DSN $test_dsn.\n",
	"\tThis test requires a running server and write permissions.\n",
	"\tPlease make sure your server is running and you have\n",
	"\tpermissions, then retry.\n");
    exit 10;
}

#
#   Main loop; leave this untouched, put tests into the loop
#
while (Testing()) {
    # Check if the server is awake.
    $dbh = undef;
    Test($state or ($dbh = DBI->connect($test_dsn, $test_user,
					$test_password)))
	or ServerError();

    Test($state or (@dsn = DBI->data_sources($mdriver)) >= 0);
    if (!$state) {
	my $d;
	print "List of $mdriver data sources:\n";
	foreach $d (@dsn) {
	    print "    $d\n";
	}
	print "List ends.\n";
    }
    Test($state or $dbh->disconnect());

    #
    #   Try different DSN's
    #
    my(@dsnList);
    if (($mdriver eq 'mysql'  or  $mdriver eq 'mSQL')
	and  $test_dsn eq "DBI:$mdriver:test") {
	@dsnList = ("DBI:$mdriver:test:localhost",
		    "DBI:$mdriver:test;localhost",
		    "DBI:$mdriver:database=test;host=localhost");
    }
    my($dsn);
    foreach $dsn (@dsnList) {
	Test($state or ($dbh = DBI->connect($dsn, $test_user,
					    $test_password)))
	    or print "Cannot connect to DSN $dsn: ${DBI::errstr}\n";
	Test($state or $dbh->disconnect());
    }
}

exit 0;

# Hate -w :-)
$test_dsn = $test_user = $test_password = $DBI::errstr;
