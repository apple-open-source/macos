use strict;
use warnings;

use Test::More tests => 9;

use lib qw(t/lib);
use base 'DBICTest';


my $schema = DBICTest->init_schema(
    no_connect  => 1,
    no_deploy   => 1,
);
ok $schema->connection(
    DBICTest->_database,
    {
        on_connect_do       => [
            'CREATE TABLE TEST_empty (id INTEGER)',
            [ 'INSERT INTO TEST_empty VALUES (?)', {}, 2 ],
            \&insert_from_subref,
        ],
        on_disconnect_do    =>
            [\&check_exists, 'DROP TABLE TEST_empty', \&check_dropped],
    },
), 'connection()';

is_deeply
  $schema->storage->dbh->selectall_arrayref('SELECT * FROM TEST_empty'),
  [ [ 2 ], [ 3 ], [ 7 ] ],
  'on_connect_do() worked';
eval { $schema->storage->dbh->do('SELECT 1 FROM TEST_nonexistent'); };
ok $@, 'Searching for nonexistent table dies';

$schema->storage->disconnect();

my($connected, $disconnected);
ok $schema->connection(
    DBICTest->_database,
    {
        on_connect_do       => sub { $connected = 1 },
        on_disconnect_do    => sub { $disconnected = 1 },
    },
), 'second connection()';
$schema->storage->dbh->do('SELECT 1');
ok $connected, 'on_connect_do() called after connect()';
ok ! $disconnected, 'on_disconnect_do() not called after connect()';
$schema->storage->disconnect();
ok $disconnected, 'on_disconnect_do() called after disconnect()';


sub check_exists {
    my $storage = shift;
    ok $storage->dbh->do('SELECT 1 FROM TEST_empty'), 'Table still exists';
    return;
}

sub check_dropped {
    my $storage = shift;
    eval { $storage->dbh->do('SELECT 1 FROM TEST_empty'); };
    ok $@, 'Reading from dropped table fails';
    return;
}

sub insert_from_subref {
    my $storage = shift;
    return [
        [ 'INSERT INTO TEST_empty VALUES (?)', {}, 3 ],
        'INSERT INTO TEST_empty VALUES (7)',
    ];
}
