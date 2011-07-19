use strict;
use warnings;  

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_MSSQL_ODBC_${_}" } qw/DSN USER PASS/};

if (not ($dsn && $user)) {
  plan skip_all =>
    'Set $ENV{DBICTEST_MSSQL_ODBC_DSN}, _USER and _PASS to run this test' .
    "\nWarning: This test drops and creates a table called 'track'";
} else {
  eval "use DateTime; use DateTime::Format::Strptime;";
  if ($@) {
    plan skip_all => 'needs DateTime and DateTime::Format::Strptime for testing';
  }
  else {
    plan tests => 4 * 2; # (tests * dt_types)
  }
}

my $schema = DBICTest::Schema->clone;

$schema->connection($dsn, $user, $pass);
$schema->storage->ensure_connected;

# coltype, column, datehash
my @dt_types = (
  ['DATETIME',
   'last_updated_at',
   {
    year => 2004,
    month => 8,
    day => 21,
    hour => 14,
    minute => 36,
    second => 48,
    nanosecond => 500000000,
  }],
  ['SMALLDATETIME', # minute precision
   'small_dt',
   {
    year => 2004,
    month => 8,
    day => 21,
    hour => 14,
    minute => 36,
  }],
);

for my $dt_type (@dt_types) {
  my ($type, $col, $sample_dt) = @$dt_type;

  eval { $schema->storage->dbh->do("DROP TABLE track") };
  $schema->storage->dbh->do(<<"SQL");
CREATE TABLE track (
 trackid INT IDENTITY PRIMARY KEY,
 cd INT,
 position INT,
 $col $type,
)
SQL
  ok(my $dt = DateTime->new($sample_dt));

  my $row;
  ok( $row = $schema->resultset('Track')->create({
        $col => $dt,
        cd => 1,
      }));
  ok( $row = $schema->resultset('Track')
    ->search({ trackid => $row->trackid }, { select => [$col] })
    ->first
  );
  is( $row->$col, $dt, 'DateTime roundtrip' );
}

# clean up our mess
END {
  if (my $dbh = eval { $schema->storage->_dbh }) {
    $dbh->do('DROP TABLE track');
  }
}
