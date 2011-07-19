use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 4;

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

is_deeply (
  get_storage_column_info ($schema->storage, 'collection', qw/size is_nullable/),
  {
    collectionid => {
      data_type => 'INTEGER',
    },
    name => {
      data_type => 'varchar',
    },
  },
  'Correctly retrieve column info (no size or is_nullable)'
);

TODO: {
  local $TODO = 'All current versions of SQLite seem to mis-report is_nullable';

  is_deeply (
    get_storage_column_info ($schema->storage, 'artist', qw/size/),
    {
      'artistid' => {
          'data_type' => 'INTEGER',
          'is_nullable' => 0,
      },
      'name' => {
          'data_type' => 'varchar',
          'is_nullable' => 1,
      },
      'rank' => {
          'data_type' => 'integer',
          'is_nullable' => 0,
          'default_value' => '13',
      },
      'charfield' => {
          'data_type' => 'char',
          'is_nullable' => 1,
      },
    },
    'Correctly retrieve column info (mixed null and non-null columns)'
  );
};


# Depending on test we need to strip away certain column info.
#  - SQLite is known to report the size differently from release to release
#  - Current DBD::SQLite versions do not implement NULLABLE
#  - Some SQLite releases report stuff that isn't there as undef

sub get_storage_column_info {
  my ($storage, $table, @ignore) = @_;

  my $type_info = $storage->columns_info_for($table);

  for my $col (keys %$type_info) {
    for my $type (keys %{$type_info->{$col}}) {
      if (
        grep { $type eq $_ } (@ignore)
          or
        not defined $type_info->{$col}{$type}
      ) {
        delete $type_info->{$col}{$type};
      }
    }
  }

  return $type_info;
}
