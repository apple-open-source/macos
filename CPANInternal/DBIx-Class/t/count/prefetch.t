use strict;
use warnings;

use lib qw(t/lib);

use Test::More;
use DBICTest;
use DBIC::SqlMakerTest;

my $schema = DBICTest->init_schema();

# collapsing prefetch
{
  my $rs = $schema->resultset("Artist")
            ->search_related('cds',
                { 'tracks.position' => [1,2] },
                { prefetch => [qw/tracks artist/] },
            );
  is ($rs->all, 5, 'Correct number of objects');
  is ($rs->count, 5, 'Correct count');

  is_same_sql_bind (
    $rs->count_rs->as_query,
    '(
      SELECT COUNT( * )
        FROM (
          SELECT cds.cdid
            FROM artist me
            JOIN cd cds ON cds.artist = me.artistid
            LEFT JOIN track tracks ON tracks.cd = cds.cdid
            JOIN artist artist ON artist.artistid = cds.artist
          WHERE tracks.position = ? OR tracks.position = ?
          GROUP BY cds.cdid
        ) count_subq
    )',
    [ map { [ 'tracks.position' => $_ ] } (1, 2) ],
  );
}

# collapsing prefetch with distinct
{
  my $rs = $schema->resultset("Artist")->search(undef, {distinct => 1})
            ->search_related('cds')->search_related('genre',
                { 'genre.name' => 'emo' },
                { prefetch => q(cds) },
            );
  is ($rs->all, 1, 'Correct number of objects');
  is ($rs->count, 1, 'Correct count');

  is_same_sql_bind (
    $rs->count_rs->as_query,
    '(
      SELECT COUNT( * )
        FROM (
          SELECT genre.genreid
            FROM (
              SELECT me.artistid, me.name, me.rank, me.charfield
                FROM artist me
              GROUP BY me.artistid, me.name, me.rank, me.charfield
            ) me
            JOIN cd cds ON cds.artist = me.artistid
            JOIN genre genre ON genre.genreid = cds.genreid
          WHERE ( genre.name = ? )
          GROUP BY genre.genreid
        )
      count_subq
    )',
    [ [ 'genre.name' => 'emo' ] ],
  );
}

# non-collapsing prefetch (no multi prefetches)
{
  my $rs = $schema->resultset("CD")
            ->search_related('tracks',
                { position => [1,2], 'lyrics.lyric_id' => undef },
                { prefetch => [qw/disc lyrics/] },
            );
  is ($rs->all, 10, 'Correct number of objects');


  is ($rs->count, 10, 'Correct count');

  is_same_sql_bind (
    $rs->count_rs->as_query,
    '(
      SELECT COUNT( * )
        FROM cd me
        JOIN track tracks ON tracks.cd = me.cdid
        JOIN cd disc ON disc.cdid = tracks.cd
        LEFT JOIN lyrics lyrics ON lyrics.track_id = tracks.trackid
      WHERE lyrics.lyric_id IS NULL AND (position = ? OR position = ?)
    )',
    [ map { [ position => $_ ] } (1, 2) ],
  );
}

done_testing;
