use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

{ # Fake storage driver for sqlite with autocast
    package DBICTest::SQLite::AutoCast;
    use base qw/
        DBIx::Class::Storage::DBI::AutoCast
        DBIx::Class::Storage::DBI::SQLite
    /;
    use mro 'c3';

    my $type_map = {
      datetime => 'DateTime',
      integer => 'INT',
      int => undef, # no conversion
    };

    sub _native_data_type {
      return $type_map->{$_[1]};
    }
}

my $schema = DBICTest->init_schema (storage_type => 'DBICTest::SQLite::AutoCast');

# 'me.id' will be cast unlike the unqualified 'id'
my $rs = $schema->resultset ('CD')->search ({
  cdid => { '>', 5 },
  'tracks.last_updated_at' => { '!=', undef },
  'tracks.last_updated_on' => { '<', 2009 },
  'tracks.position' => 4,
  'tracks.single_track' => \[ '= ?', [ single_track => [1, 2, 3 ] ] ],
}, { join => 'tracks' });

my $bind = [
  [ cdid => 5 ],
  [ 'tracks.last_updated_on' => 2009 ],
  [ 'tracks.position' => 4 ],
  [ 'single_track' => [ 1, 2, 3] ],
];

is_same_sql_bind (
  $rs->as_query,
  '(
    SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track
      FROM cd me
      LEFT JOIN track tracks ON tracks.cd = me.cdid
    WHERE
          cdid > ?
      AND tracks.last_updated_at IS NOT NULL
      AND tracks.last_updated_on < ?
      AND tracks.position = ?
      AND tracks.single_track = ?
  )',
  $bind,
  'expected sql with casting off',
);

$schema->storage->auto_cast (1);

is_same_sql_bind (
  $rs->as_query,
  '(
    SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track
      FROM cd me
      LEFT JOIN track tracks ON tracks.cd = me.cdid
    WHERE
          cdid > CAST(? AS INT)
      AND tracks.last_updated_at IS NOT NULL
      AND tracks.last_updated_on < CAST (? AS DateTime)
      AND tracks.position = ?
      AND tracks.single_track = CAST(? AS INT)
  )',
  $bind,
  'expected sql with casting on',
);

done_testing;
