use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 3;

# add some rows inside a transaction and commit it
# XXX: Is storage->dbh the only way to get a dbh?
$schema->storage->txn_begin;
for (10..15) {
    $schema->resultset("Artist")->create( { 
        artistid => $_,
        name => "artist number $_",
    } );
}
$schema->storage->txn_commit;
my ($artist) = $schema->resultset("Artist")->find(15);
is($artist->name, 'artist number 15', "Commit ok");

# add some rows inside a transaction and roll it back
$schema->storage->txn_begin;
for (21..30) {
    $schema->resultset("Artist")->create( {
        artistid => $_,
        name => "artist number $_",
    } );
}
$schema->storage->txn_rollback;
($artist) = $schema->resultset("Artist")->search( artistid => 25 );
is($artist, undef, "Rollback ok");

my $type_info = $schema->storage->columns_info_for('artist');

# I know this is gross but SQLite reports the size differently from release
# to release. At least this way the test still passes.

delete $type_info->{artistid}{size};
delete $type_info->{name}{size};

my $test_type_info = {
    'artistid' => {
        'data_type' => 'INTEGER',
        'is_nullable' => 0,
    },
    'name' => {
        'data_type' => 'varchar',
        'is_nullable' => 0,
    },
};
is_deeply($type_info, $test_type_info, 'columns_info_for - column data types');

