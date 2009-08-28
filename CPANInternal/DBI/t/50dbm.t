#!perl -w
$|=1;

use strict;
use File::Path;
use File::Spec;
use Test::More;
use Cwd;
use Config qw(%Config);

my $using_dbd_gofer = ($ENV{DBI_AUTOPROXY}||'') =~ /^dbi:Gofer.*transport=/i;

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

    # Potential DBM modules in preference order (SDBM_File first)
    # skip NDBM and ODBM as they don't support EXISTS
    my @dbms = qw(SDBM_File GDBM_File DB_File BerkeleyDB);

    if ("@ARGV" eq "all") {
	# test with as many of the major DBM types as are available
        @dbm_types = grep { eval { local $^W; require "$_.pm" } } @dbms;
    }
    elsif (@ARGV) {
	@dbm_types = @ARGV;
    }
    else {
	# we only test SDBM_File by default to avoid tripping up
	# on any broken DBM's that may be installed in odd places.
	# It's only DBD::DBM we're trying to test here.
        # (However, if SDBM_File is not available, then use another.)
        for my $dbm (@dbms) {
            if (eval { local $^W; require "$dbm.pm" }) {
                @dbm_types = ($dbm);
                last;
            }
        }
    }

    print "Using DBM modules: @dbm_types\n";
    print "Using MLDBM serializers: @mldbm_types\n" if @mldbm_types;

    my $num_tests = (1+@mldbm_types) * @dbm_types * 12;
	
    if (!$num_tests) {
        plan skip_all => "No DBM modules available";
    }
    else {
        plan tests => $num_tests;
    }
}

my $dir = File::Spec->catdir(getcwd(),'test_output');

rmtree $dir;
mkpath $dir;

my( $two_col_sql,$three_col_sql ) = split /\n\n/,join '',<DATA>;

for my $mldbm ( '', @mldbm_types ) {
    my $sql = ($mldbm) ? $three_col_sql : $two_col_sql;
    my @sql = split /\s*;\n/, $sql;
    for my $dbm_type ( @dbm_types ) {
	print "\n--- Using $dbm_type ($mldbm) ---\n";
        eval { do_test( $dbm_type, \@sql, $mldbm ) }
            or warn $@;
    }
}
rmtree $dir;

sub do_test {
    my $dtype = shift;
    my $stmts = shift;
    my $mldbm = shift;

    # The DBI can't test locking here, sadly, because of the risk it'll hang
    # on systems with broken NFS locking daemons.
    # (This test script doesn't test that locking actually works anyway.)

    my $dsn ="dbi:DBM(RaiseError=0,PrintError=1):dbm_type=$dtype;mldbm=$mldbm;lockfile=0";

    if ($using_dbd_gofer) {
        $dsn .= ";f_dir=$dir";
    }

    my $dbh = DBI->connect( $dsn );

    my $dbm_versions;
    if ($DBI::VERSION >= 1.37   # needed for install_method
    && !$ENV{DBI_AUTOPROXY}     # can't transparently proxy driver-private methods
    ) {
        $dbm_versions = $dbh->dbm_versions;
    }
    else {
        $dbm_versions = $dbh->func('dbm_versions');
    }
    print $dbm_versions;
    ok($dbm_versions);
    isa_ok($dbh, 'DBI::db');

    # test if it correctly accepts valid $dbh attributes
    SKIP: {
        skip "Can't set attributes after connect using DBD::Gofer", 2
            if $using_dbd_gofer;
        eval {$dbh->{f_dir}=$dir};
        ok(!$@);
        eval {$dbh->{dbm_mldbm}=$mldbm};
        ok(!$@);
    }

    # test if it correctly rejects invalid $dbh attributes
    #
    eval {
        local $SIG{__WARN__} = sub { } if $using_dbd_gofer;
        local $dbh->{RaiseError} = 1;
        local $dbh->{PrintError} = 0;
        $dbh->{dbm_bad_name}=1;
    };
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
    return 1;
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


