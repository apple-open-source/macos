package # hide from PAUSE 
    DBICTest::Plain;

use strict;
use warnings;
use base qw/DBIx::Class::Schema/;
use DBI;

my $db_file = "t/var/Plain.db";

unlink($db_file) if -e $db_file;
unlink($db_file . "-journal") if -e $db_file . "-journal";
mkdir("t/var") unless -d "t/var";

my $dsn = "dbi:SQLite:${db_file}";

__PACKAGE__->load_classes("Test");
my $schema = __PACKAGE__->compose_connection(
  __PACKAGE__,
  $dsn,
  undef,
  undef,
  { AutoCommit => 1 }
);

my $dbh = DBI->connect($dsn);

my $sql = <<EOSQL;
CREATE TABLE test (
  id INTEGER NOT NULL,
  name VARCHAR(32) NOT NULL
);

INSERT INTO test (id, name) VALUES (1, 'DBIC::Plain is broken!');

EOSQL

$dbh->do($_) for split(/\n\n/, $sql);

1;
