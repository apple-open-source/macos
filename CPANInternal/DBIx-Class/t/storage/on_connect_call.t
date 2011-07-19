use strict;
use warnings;
no warnings qw/once redefine/;

use lib qw(t/lib);
use DBI;
use DBICTest;
use DBICTest::Schema;
use DBIx::Class::Storage::DBI;

# !!! do not replace this with done_testing - tests reside in the callbacks
# !!! number of calls is important
use Test::More tests => 16;
# !!!

my $schema = DBICTest::Schema->clone;

{
  *DBIx::Class::Storage::DBI::connect_call_foo = sub {
    isa_ok $_[0], 'DBIx::Class::Storage::DBI',
      'got storage in connect_call method';
    is $_[1], 'bar', 'got param in connect_call method';
  };

  *DBIx::Class::Storage::DBI::disconnect_call_foo = sub {
    isa_ok $_[0], 'DBIx::Class::Storage::DBI',
      'got storage in disconnect_call method';
  };

  ok $schema->connection(
      DBICTest->_database,
    {
      on_connect_call => [
          [ do_sql => 'create table test1 (id integer)' ],
          [ do_sql => [ 'insert into test1 values (?)', {}, 1 ] ],
          [ do_sql => sub { ['insert into test1 values (2)'] } ],
          [ sub { $_[0]->dbh->do($_[1]) }, 'insert into test1 values (3)' ],
          # this invokes $storage->connect_call_foo('bar') (above)
          [ foo => 'bar' ],
      ],
      on_connect_do => 'insert into test1 values (4)',
      on_disconnect_call => 'foo',
    },
  ), 'connection()';

  ok (! $schema->storage->connected, 'start disconnected');

  is_deeply (
    $schema->storage->dbh->selectall_arrayref('select * from test1'),
    [ [ 1 ], [ 2 ], [ 3 ], [ 4 ] ],
    'on_connect_call/do actions worked'
  );

  $schema->storage->disconnect;
}

{
  *DBIx::Class::Storage::DBI::connect_call_foo = sub {
    isa_ok $_[0], 'DBIx::Class::Storage::DBI',
      'got storage in connect_call method';
  };

  *DBIx::Class::Storage::DBI::connect_call_bar = sub {
    isa_ok $_[0], 'DBIx::Class::Storage::DBI',
      'got storage in connect_call method';
  };


  ok $schema->connection(
    DBICTest->_database,
    {
      # method list form
      on_connect_call => [ 'foo', sub { ok 1, "coderef in list form" }, 'bar' ],
    },
  ), 'connection()';

  ok (! $schema->storage->connected, 'start disconnected');
  $schema->storage->ensure_connected;
  $schema->storage->disconnect; # this should not fire any tests
}

{
  ok $schema->connection(
    sub { DBI->connect(DBICTest->_database) },
    {
      # method list form
      on_connect_call => [ sub { ok 1, "on_connect_call after DT parser" }, ],
      on_disconnect_call => [ sub { ok 1, "on_disconnect_call after DT parser" }, ],
    },
  ), 'connection()';

  ok (! $schema->storage->connected, 'start disconnected');

  $schema->storage->_determine_driver;  # this should connect due to the coderef

  ok ($schema->storage->connected, 'determine driver connects');
  $schema->storage->disconnect;
}
