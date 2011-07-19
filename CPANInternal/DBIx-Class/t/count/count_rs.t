use strict;
use warnings;

use lib qw(t/lib);

use Test::More;
use DBICTest;
use DBIC::SqlMakerTest;
use DBIC::DebugObj;

plan tests => 10;

my $schema = DBICTest->init_schema();

# non-collapsing prefetch (no multi prefetches)
{
  my $rs = $schema->resultset("CD")
            ->search_related('tracks',
                { position => [1,2] },
                { prefetch => [qw/disc lyrics/], rows => 3, offset => 8 },
            );
  is ($rs->all, 2, 'Correct number of objects');


  my ($sql, @bind);
  $schema->storage->debugobj(DBIC::DebugObj->new(\$sql, \@bind));
  $schema->storage->debug(1);

  is ($rs->count, 2, 'Correct count via count()');

  is_same_sql_bind (
    $sql,
    \@bind,
    'SELECT COUNT( * )
      FROM cd me
      JOIN track tracks ON tracks.cd = me.cdid
      JOIN cd disc ON disc.cdid = tracks.cd
     WHERE ( ( position = ? OR position = ? ) )
    ',
    [ qw/'1' '2'/ ],
    'count softlimit applied',
  );

  my $crs = $rs->count_rs;
  is ($crs->next, 2, 'Correct count via count_rs()');

  is_same_sql_bind (
    $crs->as_query,
    '(SELECT COUNT( * )
       FROM (
        SELECT tracks.trackid
          FROM cd me
          JOIN track tracks ON tracks.cd = me.cdid
          JOIN cd disc ON disc.cdid = tracks.cd
        WHERE ( ( position = ? OR position = ? ) )
        LIMIT 3 OFFSET 8
       ) count_subq
    )',
    [ [ position => 1 ], [ position => 2 ] ],
    'count_rs db-side limit applied',
  );
}

# has_many prefetch with limit
{
  my $rs = $schema->resultset("Artist")
            ->search_related('cds',
                { 'tracks.position' => [1,2] },
                { prefetch => [qw/tracks artist/], rows => 3, offset => 4 },
            );
  is ($rs->all, 1, 'Correct number of objects');

  my ($sql, @bind);
  $schema->storage->debugobj(DBIC::DebugObj->new(\$sql, \@bind));
  $schema->storage->debug(1);

  is ($rs->count, 1, 'Correct count via count()');

  is_same_sql_bind (
    $sql,
    \@bind,
    'SELECT COUNT( * )
      FROM (
        SELECT cds.cdid
          FROM artist me
          JOIN cd cds ON cds.artist = me.artistid
          LEFT JOIN track tracks ON tracks.cd = cds.cdid
          JOIN artist artist ON artist.artistid = cds.artist
        WHERE tracks.position = ? OR tracks.position = ?
        GROUP BY cds.cdid
      ) count_subq
    ',
    [ qw/'1' '2'/ ],
    'count softlimit applied',
  );

  my $crs = $rs->count_rs;
  is ($crs->next, 1, 'Correct count via count_rs()');

  is_same_sql_bind (
    $crs->as_query,
    '(SELECT COUNT( * )
      FROM (
        SELECT cds.cdid
          FROM artist me
          JOIN cd cds ON cds.artist = me.artistid
          LEFT JOIN track tracks ON tracks.cd = cds.cdid
          JOIN artist artist ON artist.artistid = cds.artist
        WHERE tracks.position = ? OR tracks.position = ?
        GROUP BY cds.cdid
        LIMIT 3 OFFSET 4
      ) count_subq
    )',
    [ [ 'tracks.position' => 1 ], [ 'tracks.position' => 2 ] ],
    'count_rs db-side limit applied',
  );
}
