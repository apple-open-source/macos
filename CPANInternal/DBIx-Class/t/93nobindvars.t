use strict;
use warnings;  

# Copied from 71mysql.t, manually using NoBindVars.  This is to give that code
#  wider testing, since virtually nobody who regularly runs the test suite
#  is using DBD::Sybase+FreeTDS+MSSQL -- blblack

use Test::More;
use lib qw(t/lib);
use DBICTest;
use DBI::Const::GetInfoType;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_MYSQL_${_}" } qw/DSN USER PASS/};

#warn "$dsn $user $pass";

plan skip_all => 'Set $ENV{DBICTEST_MYSQL_DSN}, _USER and _PASS to run this test'
  unless ($dsn && $user);

plan tests => 4;

{ # Fake storage driver for mysql + no bind variables
    package DBIx::Class::Storage::DBI::MySQLNoBindVars;
    use Class::C3;
    use base qw/
        DBIx::Class::Storage::DBI::NoBindVars
        DBIx::Class::Storage::DBI::mysql
    /;
    $INC{'DBIx/Class/Storage/DBI/MySQLNoBindVars.pm'} = 1;
}

# XXX Class::C3 doesn't like some of the Storage stuff happening late...
Class::C3::reinitialize();

my $schema = DBICTest::Schema->clone;
$schema->storage_type('::DBI::MySQLNoBindVars');
$schema->connection($dsn, $user, $pass);

my $dbh = $schema->storage->dbh;

$dbh->do("DROP TABLE IF EXISTS artist;");

$dbh->do("CREATE TABLE artist (artistid INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, name VARCHAR(255), charfield CHAR(10));");

$schema->class('Artist')->load_components('PK::Auto');

# test primary key handling
my $new = $schema->resultset('Artist')->create({ name => 'foo' });
ok($new->artistid, "Auto-PK worked");

# test LIMIT support
for (1..6) {
    $schema->resultset('Artist')->create({ name => 'Artist ' . $_ });
}
my $it = $schema->resultset('Artist')->search( {},
    { rows => 3,
      offset => 2,
      order_by => 'artistid' }
);
is( $it->count, 3, "LIMIT count ok" );
is( $it->next->name, "Artist 2", "iterator->next ok" );
$it->next;
$it->next;
is( $it->next, undef, "next past end of resultset ok" );

# clean up our mess
END {
    $dbh->do("DROP TABLE artist") if $dbh;
}
