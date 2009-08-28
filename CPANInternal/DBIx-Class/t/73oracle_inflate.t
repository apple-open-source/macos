use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_ORA_${_}" } qw/DSN USER PASS/};

eval "use DateTime; use DateTime::Format::Oracle;";
if ($@) {
    plan skip_all => 'needs DateTime and DateTime::Format::Oracle for testing';
}
elsif (not ($dsn && $user && $pass)) {
    plan skip_all => 'Set $ENV{DBICTEST_ORA_DSN}, _USER and _PASS to run this test. ' .
         'Warning: This test drops and creates a table called \'track\'';
}
else {
    plan tests => 4;
}

# DateTime::Format::Oracle needs this set
$ENV{NLS_DATE_FORMAT} = 'DD-MON-YY';

my $schema = DBICTest::Schema->connect($dsn, $user, $pass);

# Need to redefine the last_updated_on column
my $col_metadata = $schema->class('Track')->column_info('last_updated_on');
$schema->class('Track')->add_column( 'last_updated_on' => {
    data_type => 'date' });

my $dbh = $schema->storage->dbh;

eval {
  $dbh->do("DROP TABLE track");
};
$dbh->do("CREATE TABLE track (trackid NUMBER(12), cd NUMBER(12), position NUMBER(12), title VARCHAR(255), last_updated_on DATE)");

# insert a row to play with
my $new = $schema->resultset('Track')->create({ trackid => 1, cd => 1, position => 1, title => 'Track1', last_updated_on => '06-MAY-07' });
is($new->trackid, 1, "insert sucessful");

my $track = $schema->resultset('Track')->find( 1 );

is( ref($track->last_updated_on), 'DateTime', "last_updated_on inflated ok");

is( $track->last_updated_on->month, 5, "DateTime methods work on inflated column");

my $dt = DateTime->now();
$track->last_updated_on($dt);
$track->update;

is( $track->last_updated_on->month, $dt->month, "deflate ok");

# clean up our mess
END {
    if($dbh) {
        $dbh->do("DROP TABLE track");
    }
}

