use strict;
use warnings;  

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_SYBASE_${_}" } qw/DSN USER PASS/};

if (not ($dsn && $user)) {
  plan skip_all =>
    'Set $ENV{DBICTEST_SYBASE_DSN}, _USER and _PASS to run this test' .
    "\nWarning: This test drops and creates a table called 'track'";
} else {
  eval "use DateTime; use DateTime::Format::Sybase;";
  if ($@) {
    plan skip_all => 'needs DateTime and DateTime::Format::Sybase for testing';
  }
}

my @storage_types = (
  'DBI::Sybase::ASE',
  'DBI::Sybase::ASE::NoBindVars',
);
my $schema;

for my $storage_type (@storage_types) {
  $schema = DBICTest::Schema->clone;

  unless ($storage_type eq 'DBI::Sybase::ASE') { # autodetect
    $schema->storage_type("::$storage_type");
  }
  $schema->connection($dsn, $user, $pass, {
    AutoCommit => 1,
    on_connect_call => [ 'datetime_setup' ],
  });

  $schema->storage->ensure_connected;

  isa_ok( $schema->storage, "DBIx::Class::Storage::$storage_type" );

# coltype, col, date
  my @dt_types = (
    ['DATETIME', 'last_updated_at', '2004-08-21T14:36:48.080Z'],
# minute precision
    ['SMALLDATETIME', 'small_dt', '2004-08-21T14:36:00.000Z'],
  );
  
  for my $dt_type (@dt_types) {
    my ($type, $col, $sample_dt) = @$dt_type;

    eval { $schema->storage->dbh->do("DROP TABLE track") };
    $schema->storage->dbh->do(<<"SQL");
CREATE TABLE track (
   trackid INT IDENTITY PRIMARY KEY,
   cd INT NULL,
   position INT NULL,
   $col $type NULL
)
SQL
    ok(my $dt = DateTime::Format::Sybase->parse_datetime($sample_dt));

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

  # test a computed datetime column
  eval { $schema->storage->dbh->do("DROP TABLE track") };
  $schema->storage->dbh->do(<<"SQL");
CREATE TABLE track (
   trackid INT IDENTITY PRIMARY KEY,
   cd INT NULL,
   position INT NULL,
   title VARCHAR(100) NULL,
   last_updated_on DATETIME NULL,
   last_updated_at AS getdate(),
   small_dt SMALLDATETIME NULL
)
SQL

  my $now     = DateTime->now;
  sleep 1;
  my $new_row = $schema->resultset('Track')->create({});
  $new_row->discard_changes;

  lives_and {
    cmp_ok (($new_row->last_updated_at - $now)->seconds, '>=', 1)
  } 'getdate() computed column works';
}

done_testing;

# clean up our mess
END {
  if (my $dbh = eval { $schema->storage->_dbh }) {
    $dbh->do('DROP TABLE track');
  }
}
