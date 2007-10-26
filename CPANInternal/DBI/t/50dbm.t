#!perl -w

use strict;
use File::Path;
use Test::More;
use Config qw(%Config);

use DBI;
use vars qw( @mldbm_types @dbm_types );
BEGIN {

    # Be conservative about what modules we use here.
    # We don't want to be tripped up by a badly installed module
    # so we remove from @INC any version-specific dirs that don't
    # also have an arch-specific dir. Plus, for 5.8 remove any <=5.7

    # 0=SQL::Statement if avail, 1=DBI::SQL::Nano
    # next line forces use of Nano rather than default behaviour
    $ENV{DBI_SQL_NANO}=1;

    if (eval { require 'MLDBM.pm'; }) {
        push @mldbm_types, 'Data::Dumper' if eval { require 'Data/Dumper.pm' };
        push @mldbm_types, 'Storable'     if eval { require 'Storable.pm' };
    }

    if ("@ARGV" eq "all") {
	# test with as many of the 5 major DBM types as are available
	for (qw( SDBM_File GDBM_File NDBM_File ODBM_File DB_File BerkeleyDB )){
	    push @dbm_types, $_ if eval { require "$_.pm" };
	}
    }
    elsif (@ARGV) {
	@dbm_types = @ARGV;
    }
    else {
	# we only test SDBM_File by default to avoid tripping up
	# on any broken DBM's that may be installed in odd places.
	# It's only DBD::DBM we're trying to test here.
        @dbm_types = ("SDBM_File");
    }

    print "Using DBM modules: @dbm_types\n";
    print "Using MLDBM serializers: @mldbm_types\n" if @mldbm_types;

    my $num_tests = (1+@mldbm_types) * @dbm_types * 11;
	
    if (!$num_tests) {
        plan skip_all => "No DBM modules available";
    }
	else {
		plan tests => $num_tests;
	}
}

my $dir = './test_output';

rmtree $dir;
mkpath $dir;

my( $two_col_sql,$three_col_sql ) = split /\n\n/,join '',<DATA>;

for my $mldbm ( '', @mldbm_types ) {
    my $sql = ($mldbm) ? $three_col_sql : $two_col_sql;
    my @sql = split /\s*;\n/, $sql;
    for my $dbm_type ( @dbm_types ) {
	print "\n--- Using $dbm_type ($mldbm) ---\n";
        do_test( $dbm_type, \@sql, $mldbm );
    }
}
rmtree $dir;

sub do_test {
    my $dtype = shift;
    my $stmts = shift;
    my $mldbm = shift;
    $|=1;

    # The DBI can't test locking here, sadly, because of the risk it'll hang
    # on systems with broken NFS locking daemons.
    # (This test script doesn't test that locking actually works anyway.)

    my $dsn ="dbi:DBM(RaiseError=1,PrintError=0):dbm_type=$dtype;mldbm=$mldbm;lockfile=0";
    my $dbh = DBI->connect( $dsn );

    if ($DBI::VERSION >= 1.37 ) { # needed for install_method
        print $dbh->dbm_versions;
    }
    else {
        print $dbh->func('dbm_versions');
    }
    isa_ok($dbh, 'DBI::db');

    # test if it correctly accepts valid $dbh attributes
    #
    eval {$dbh->{f_dir}=$dir};
    ok(!$@);
    eval {$dbh->{dbm_mldbm}=$mldbm};
    ok(!$@);

    # test if it correctly rejects invalid $dbh attributes
    #
    eval {$dbh->{dbm_bad_name}=1};
    ok($@);

    for my $sql ( @$stmts ) {
        $sql =~ s/\S*fruit/${dtype}_fruit/; # include dbm type in table name
        $sql =~ s/;$//;  # in case no final \n on last line of __DATA__
        #diag($sql);
        my $null = '';
        my $expected_results = {
            1 => 'oranges',
            2 => 'apples',
            3 => $null,
        };
        $expected_results = {
            1 => '11',
            2 => '12',
            3 => '13',
        } if $mldbm;
	print " $sql\n";
        my $sth = $dbh->prepare($sql) or die $dbh->errstr;
        $sth->execute;
        die $sth->errstr if $sth->err and $sql !~ /DROP/;
        next unless $sql =~ /SELECT/;
        my $results='';
        # Note that we can't rely on the order here, it's not portable,
        # different DBMs (or versions) will return different orders.
        while (my ($key, $value) = $sth->fetchrow_array) {
            ok exists $expected_results->{$key};
            is $value, $expected_results->{$key};
        }
        is $DBI::rows, keys %$expected_results;
    }
    $dbh->disconnect;
}
1;
__DATA__
DROP TABLE IF EXISTS fruit;
CREATE TABLE fruit (dKey INT, dVal VARCHAR(10));
INSERT INTO  fruit VALUES (1,'oranges'   );
INSERT INTO  fruit VALUES (2,'to_change' );
INSERT INTO  fruit VALUES (3, NULL       );
INSERT INTO  fruit VALUES (4,'to delete' );
UPDATE fruit SET dVal='apples' WHERE dKey=2;
DELETE FROM  fruit WHERE dVal='to delete';
SELECT * FROM fruit;
DROP TABLE fruit;

DROP TABLE IF EXISTS multi_fruit;
CREATE TABLE multi_fruit (dKey INT, dVal VARCHAR(10), qux INT);
INSERT INTO  multi_fruit VALUES (1,'oranges'  , 11 );
INSERT INTO  multi_fruit VALUES (2,'apples'   ,  0 );
INSERT INTO  multi_fruit VALUES (3, NULL      , 13 );
INSERT INTO  multi_fruit VALUES (4,'to_delete', 14 );
UPDATE multi_fruit SET qux='12' WHERE dKey=2;
DELETE FROM  multi_fruit WHERE dKey=4;
SELECT dKey,qux FROM multi_fruit;
DROP TABLE multi_fruit;


