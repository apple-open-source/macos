use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_ORA_${_}" } qw/DSN USER PASS/};

if (not ($dsn && $user && $pass)) {
    plan skip_all => 'Set $ENV{DBICTEST_ORA_DSN}, _USER and _PASS to run this test. ' .
         'Warning: This test drops and creates a table called \'track\'';
}
else {
    eval "use DateTime; use DateTime::Format::Oracle;";
    if ($@) {
        plan skip_all => 'needs DateTime and DateTime::Format::Oracle for testing';
    }
    else {
        plan tests => 10;
    }
}

# DateTime::Format::Oracle needs this set
$ENV{NLS_DATE_FORMAT} = 'DD-MON-YY';
$ENV{NLS_TIMESTAMP_FORMAT} = 'YYYY-MM-DD HH24:MI:SSXFF';
$ENV{NLS_LANG} = 'AMERICAN_AMERICA.WE8ISO8859P1';

my $schema = DBICTest::Schema->connect($dsn, $user, $pass);

# Need to redefine the last_updated_on column
my $col_metadata = $schema->class('Track')->column_info('last_updated_on');
$schema->class('Track')->add_column( 'last_updated_on' => {
    data_type => 'date' });
$schema->class('Track')->add_column( 'last_updated_at' => {
    data_type => 'timestamp' });

my $dbh = $schema->storage->dbh;

#$dbh->do("alter session set nls_timestamp_format = 'YYYY-MM-DD HH24:MI:SSXFF'");

eval {
  $dbh->do("DROP TABLE track");
};
$dbh->do("CREATE TABLE track (trackid NUMBER(12), cd NUMBER(12), position NUMBER(12), title VARCHAR(255), last_updated_on DATE, last_updated_at TIMESTAMP, small_dt DATE)");

# insert a row to play with
my $new = $schema->resultset('Track')->create({ trackid => 1, cd => 1, position => 1, title => 'Track1', last_updated_on => '06-MAY-07', last_updated_at => '2009-05-03 21:17:18.5' });
is($new->trackid, 1, "insert sucessful");

my $track = $schema->resultset('Track')->find( 1 );

is( ref($track->last_updated_on), 'DateTime', "last_updated_on inflated ok");

is( $track->last_updated_on->month, 5, "DateTime methods work on inflated column");

#note '$track->last_updated_at => ', $track->last_updated_at;
is( ref($track->last_updated_at), 'DateTime', "last_updated_at inflated ok");

is( $track->last_updated_at->nanosecond, 500_000_000, "DateTime methods work with nanosecond precision");

my $dt = DateTime->now();
$track->last_updated_on($dt);
$track->last_updated_at($dt);
$track->update;

is( $track->last_updated_on->month, $dt->month, "deflate ok");
is( int $track->last_updated_at->nanosecond, int $dt->nanosecond, "deflate ok with nanosecond precision");

# test datetime_setup

$schema->storage->disconnect;

delete $ENV{NLS_DATE_FORMAT};
delete $ENV{NLS_TIMESTAMP_FORMAT};

$schema->connection($dsn, $user, $pass, {
    on_connect_call => 'datetime_setup'
});

$dt = DateTime->now();

my $timestamp = $dt->clone;
$timestamp->set_nanosecond( int 500_000_000 );

$track = $schema->resultset('Track')->find( 1 );
$track->update({ last_updated_on => $dt, last_updated_at => $timestamp });

$track = $schema->resultset('Track')->find(1);

is( $track->last_updated_on, $dt, 'DateTime round-trip as DATE' );
is( $track->last_updated_at, $timestamp, 'DateTime round-trip as TIMESTAMP' );

is( int $track->last_updated_at->nanosecond, int 500_000_000,
  'TIMESTAMP nanoseconds survived' );

# clean up our mess
END {
    if($schema && ($dbh = $schema->storage->dbh)) {
        $dbh->do("DROP TABLE track");
    }
}

