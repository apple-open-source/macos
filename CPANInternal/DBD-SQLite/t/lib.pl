#   Hej, Emacs, give us -*- perl mode here!
#
#   $Id: lib.pl,v 1.2 2007/01/30 07:01:06 wkakes Exp $
#
#   lib.pl is the file where database specific things should live,
#   whereever possible. For example, you define certain constants
#   here and the like.
#

require 5.003;
use strict;
use vars qw($mdriver $dbdriver $childPid $test_dsn $test_user $test_password
            $haveFileSpec);


#
#   Driver names; EDIT THIS!
#
$mdriver = 'SQLite';
$dbdriver = $mdriver; # $dbdriver is usually just the same as $mdriver.
                      # The exception is DBD::pNET where we have to
                      # to separate between local driver (pNET) and
                      # the remote driver ($dbdriver)


#
#   DSN being used; do not edit this, edit "$dbdriver.dbtest" instead
#
mkdir 'output';

$haveFileSpec = eval { require File::Spec };
my $table_dir = $haveFileSpec ?
    File::Spec->catdir(File::Spec->curdir(), 'output', 'foo') : 'output/foo';
$test_dsn      = $ENV{'DBI_DSN'}
    ||  "DBI:$dbdriver:dbname=$table_dir";
$test_user     = $ENV{'DBI_USER'}  ||  "";
$test_password = $ENV{'DBI_PASS'}  ||  "";


$::COL_NULLABLE = 1;
$::COL_KEY = 2;


my $file;
if (-f ($file = "t/$dbdriver.dbtest")  ||
    -f ($file = "$dbdriver.dbtest")    ||
    -f ($file = "../tests/$dbdriver.dbtest")  ||
    -f ($file = "tests/$dbdriver.dbtest")) {
    eval { require $file; };
    if ($@) {
	print STDERR "Cannot execute $file: $@.\n";
	print "1..0\n";
	exit 0;
    }
}
if (-f ($file = "t/$mdriver.mtest")  ||
    -f ($file = "$mdriver.mtest")    ||
    -f ($file = "../tests/$mdriver.mtest")  ||
    -f ($file = "tests/$mdriver.mtest")) {
    eval { require $file; };
    if ($@) {
	print STDERR "Cannot execute $file: $@.\n";
	print "1..0\n";
	exit 0;
    }
}


open (STDERR, ">&STDOUT") || die "Cannot redirect stderr" ;  
select (STDERR) ; $| = 1 ;
select (STDOUT) ; $| = 1 ;


#
#   The Testing() function builds the frame of the test; it can be called
#   in many ways, see below.
#
#   Usually there's no need for you to modify this function.
#
#       Testing() (without arguments) indicates the beginning of the
#           main loop; it will return, if the main loop should be
#           entered (which will happen twice, once with $state = 1 and
#           once with $state = 0)
#       Testing('off') disables any further tests until the loop ends
#       Testing('group') indicates the begin of a group of tests; you
#           may use this, for example, if there's a certain test within
#           the group that should make all other tests fail.
#       Testing('disable') disables further tests within the group; must
#           not be called without a preceding Testing('group'); by default
#           tests are enabled
#       Testing('enabled') reenables tests after calling Testing('disable')
#       Testing('finish') terminates a group; any Testing('group') must
#           be paired with Testing('finish')
#
#   You may nest test groups.
#
{
    # Note the use of the pairing {} in order to get local, but static,
    # variables.
    my (@stateStack, $count, $off);

    $count = 0;

    sub Testing(;$) {
	my ($command) = shift;
	if (!defined($command)) {
	    @stateStack = ();
	    $off = 0;
	    if ($count == 0) {
		++$count;
		$::state = 1;
	    } elsif ($count == 1) {
		my($d);
		if ($off) {
		    print "1..0\n";
		    exit 0;
		}
		++$count;
		$::state = 0;
		print "1..$::numTests\n";
	    } else {
		return 0;
	    }
	    if ($off) {
		$::state = 1;
	    }
	    $::numTests = 0;
	} elsif ($command eq 'off') {
	    $off = 1;
	    $::state = 0;
	} elsif ($command eq 'group') {
	    push(@stateStack, $::state);
	} elsif ($command eq 'disable') {
	    $::state = 0;
	} elsif ($command eq 'enable') {
	    if ($off) {
		$::state = 0;
	    } else {
		my $s;
		$::state = 1;
		foreach $s (@stateStack) {
		    if (!$s) {
			$::state = 0;
			last;
		    }
		}
	    }
	    return;
	} elsif ($command eq 'finish') {
	    $::state = pop(@stateStack);
	} else {
	    die("Testing: Unknown argument\n");
	}
	return 1;
    }


#
#   Read a single test result
#

    sub Test ($;$$) {
	my($result, $error, $diag) = @_;
       
        ++$::numTests;
	if ($count == 2) {
	    if (defined($diag)) {
	        printf("$diag%s", (($diag =~ /\n$/) ? "" : "\n"));
	    }
	    if ($::state || $result) {
		print "ok $::numTests ". (defined($error) ? "$error\n" : "\n");
		return 1;
	    } else {
		print("not ok $::numTests - " .
			(defined($error) ? "$error\n" : "\n"));
		print("FAILED Test $::numTests - " .
			(defined($error) ? "$error\n" : "\n"));
		return 0;
	    }
	}
	return 1;
    }
}


#
#   Print a DBI error message
#
sub DbiError ($$) {
    my($rc, $err) = @_;
    $rc ||= 0;
    $err ||= '';
    print "Test $::numTests: DBI error $rc, $err\n";
}


#
#   This functions generates a list of possible DSN's aka
#   databases and returns a possible table name for a new
#   table being created.
#
#   Problem is, we have two different situations here: Test scripts
#   call us by pasing a dbh, which is fine for most situations.
#   From within DBD::pNET, however, the dbh isn't that meaningful.
#   Thus we are working with the global variable $listTablesHook:
#   Once defined, we call &$listTablesHook instead of ListTables.
#
#   See DBD::pNET/t/pNET.mtest for details.
#
{
    use vars qw($listTablesHook);

    my(@tables, $testtable, $listed);

    $testtable = "testaa";
    $listed = 0;

    sub FindNewTable($) {
	my($dbh) = @_;

	if (!$listed) {
	    if (defined($listTablesHook)) {
		@tables = &$listTablesHook($dbh);
	    } elsif (defined(&ListTables)) {
		@tables = &ListTables($dbh);
	    } else {
		die "Fatal: ListTables not implemented.\n";
	    }
	    $listed = 1;
	}

	# A small loop to find a free test table we can use to mangle stuff in
	# and out of. This starts at testaa and loops until testaz, then testba
	# - testbz and so on until testzz.
	my $foundtesttable = 1;
	my $table;
	while ($foundtesttable) {
	    $foundtesttable = 0;
	    foreach $table (@tables) {
		if ($table eq $testtable) {
		    $testtable++;
		    $foundtesttable = 1;
		}
	    }
	}
	$table = $testtable;
	$testtable++;
	$table;
    }
}


sub ErrMsg { print (@_); }
sub ErrMsgF { printf (@_); }


1;
