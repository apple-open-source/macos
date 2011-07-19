use strict;
use warnings;

use Test::More;
use Test::Exception;

use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

my $schema = DBICTest->init_schema();
my $sdebug = $schema->storage->debug;

my $cd_rs = $schema->resultset('CD')->search (
  { 'tracks.cd' => { '!=', undef } },
  { prefetch => 'tracks' },
);

# Database sanity check
is($cd_rs->count, 5, 'CDs with tracks count');
for ($cd_rs->all) {
  is ($_->tracks->count, 3, '3 tracks for CD' . $_->id );
}

# Test a belongs_to prefetch of a has_many
{
  my $track_rs = $schema->resultset ('Track')->search (
    { 'me.cd' => { -in => [ $cd_rs->get_column ('cdid')->all ] } },
    {
      select => [
        'me.cd',
        { count => 'me.trackid' },
      ],
      as => [qw/
        cd
        track_count
      /],
      group_by => [qw/me.cd/],
      prefetch => 'cd',
    },
  );

  # this used to fuck up ->all, do not remove!
  ok ($track_rs->first, 'There is stuff in the rs');

  is($track_rs->count, 5, 'Prefetched count with groupby');
  is($track_rs->all, 5, 'Prefetched objects with groupby');

  {
    my $query_cnt = 0;
    $schema->storage->debugcb ( sub { $query_cnt++ } );
    $schema->storage->debug (1);

    while (my $collapsed_track = $track_rs->next) {
      my $cdid = $collapsed_track->get_column('cd');
      is($collapsed_track->get_column('track_count'), 3, "Correct count of tracks for CD $cdid" );
      ok($collapsed_track->cd->title, "Prefetched title for CD $cdid" );
    }

    is ($query_cnt, 1, 'Single query on prefetched titles');
    $schema->storage->debugcb (undef);
    $schema->storage->debug ($sdebug);
  }

  # Test sql by hand, as the sqlite db will simply paper over
  # improper group/select combinations
  #
  is_same_sql_bind (
    $track_rs->count_rs->as_query,
    '(
      SELECT COUNT( * )
        FROM (
          SELECT me.cd
            FROM track me
            JOIN cd cd ON cd.cdid = me.cd
          WHERE ( me.cd IN ( ?, ?, ?, ?, ? ) )
          GROUP BY me.cd
        )
      count_subq
    )',
    [ map { [ 'me.cd' => $_] } ($cd_rs->get_column ('cdid')->all) ],
    'count() query generated expected SQL',
  );

  is_same_sql_bind (
    $track_rs->as_query,
    '(
      SELECT me.cd, me.track_count, cd.cdid, cd.artist, cd.title, cd.year, cd.genreid, cd.single_track
        FROM (
          SELECT me.cd, COUNT (me.trackid) AS track_count
            FROM track me
            JOIN cd cd ON cd.cdid = me.cd
          WHERE ( me.cd IN ( ?, ?, ?, ?, ? ) )
          GROUP BY me.cd
          ) me
        JOIN cd cd ON cd.cdid = me.cd
      WHERE ( me.cd IN ( ?, ?, ?, ?, ? ) )
    )',
    [ map { [ 'me.cd' => $_] } ( ($cd_rs->get_column ('cdid')->all) x 2 ) ],
    'next() query generated expected SQL',
  );


  # add an extra track to one of the cds, and then make sure we can get it on top
  # (check if limit works)
  my $top_cd = $cd_rs->slice (1,1)->next;
  $top_cd->create_related ('tracks', {
    title => 'over the top',
  });

  my $top_cd_collapsed_track = $track_rs->search ({}, {
    rows => 2,
    order_by => [
      { -desc => 'track_count' },
    ],
  });

  is ($top_cd_collapsed_track->count, 2);

  is (
    $top_cd->title,
    $top_cd_collapsed_track->first->cd->title,
    'Correct collapsed track with prefetched CD returned on top'
  );
}

# test a has_many/might_have prefetch at the same level
# Note that one of the CDs now has 4 tracks instead of 3
{
  my $most_tracks_rs = $schema->resultset ('CD')->search (
    {
      'me.cdid' => { '!=' => undef },  # duh - this is just to test WHERE
    },
    {
      prefetch => [qw/tracks liner_notes/],
      select => ['me.cdid', { count => 'tracks.trackid' }, { max => 'tracks.trackid', -as => 'maxtr'} ],
      as => [qw/cdid track_count max_track_id/],
      group_by => 'me.cdid',
      order_by => [ { -desc => 'track_count' }, { -asc => 'maxtr' } ],
      rows => 2,
    }
  );

  is_same_sql_bind (
    $most_tracks_rs->count_rs->as_query,
    '(
      SELECT COUNT( * )
        FROM (
          SELECT me.cdid
            FROM cd me
          WHERE ( me.cdid IS NOT NULL )
          GROUP BY me.cdid
          LIMIT 2
        ) count_subq
    )',
    [],
    'count() query generated expected SQL',
  );

  is_same_sql_bind (
    $most_tracks_rs->as_query,
    '(
      SELECT  me.cdid, me.track_count, me.maxtr,
              tracks.trackid, tracks.cd, tracks.position, tracks.title, tracks.last_updated_on, tracks.last_updated_at, tracks.small_dt,
              liner_notes.liner_id, liner_notes.notes
        FROM (
          SELECT me.cdid, COUNT( tracks.trackid ) AS track_count, MAX( tracks.trackid ) AS maxtr
            FROM cd me
            LEFT JOIN track tracks ON tracks.cd = me.cdid
          WHERE ( me.cdid IS NOT NULL )
          GROUP BY me.cdid
          ORDER BY track_count DESC, maxtr ASC
          LIMIT 2
        ) me
        LEFT JOIN track tracks ON tracks.cd = me.cdid
        LEFT JOIN liner_notes liner_notes ON liner_notes.liner_id = me.cdid
      WHERE ( me.cdid IS NOT NULL )
      ORDER BY track_count DESC, maxtr ASC, tracks.cd
    )',
    [],
    'next() query generated expected SQL',
  );

  is ($most_tracks_rs->count, 2, 'Limit works');
  my $top_cd = $most_tracks_rs->first;
  is ($top_cd->id, 2, 'Correct cd fetched on top'); # 2 because of the slice(1,1) earlier

  my $query_cnt = 0;
  $schema->storage->debugcb ( sub { $query_cnt++ } );
  $schema->storage->debug (1);

  is ($top_cd->get_column ('track_count'), 4, 'Track count fetched correctly');
  is ($top_cd->tracks->count, 4, 'Count of prefetched tracks rs still correct');
  is ($top_cd->tracks->all, 4, 'Number of prefetched track objects still correct');
  is (
    $top_cd->liner_notes->notes,
    'Buy Whiskey!',
    'Correct liner pre-fetched with top cd',
  );

  is ($query_cnt, 0, 'No queries executed during prefetched data access');
  $schema->storage->debugcb (undef);
  $schema->storage->debug ($sdebug);
}

# make sure that distinct still works
{
  my $rs = $schema->resultset("CD")->search({}, {
    prefetch => 'tags',
    order_by => 'cdid',
    distinct => 1,
  });

  is_same_sql_bind (
    $rs->as_query,
    '(
      SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track,
             tags.tagid, tags.cd, tags.tag
        FROM (
          SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track
            FROM cd me
          GROUP BY me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track, cdid
          ORDER BY cdid
        ) me
        LEFT JOIN tags tags ON tags.cd = me.cdid
      ORDER BY cdid, tags.cd, tags.tag
    )',
    [],
    'Prefetch + distinct resulted in correct group_by',
  );

  is ($rs->all, 5, 'Correct number of CD objects');
  is ($rs->count, 5, 'Correct count of CDs');
}

# RT 47779, test group_by as a scalar ref
{
  my $track_rs = $schema->resultset ('Track')->search (
    { 'me.cd' => { -in => [ $cd_rs->get_column ('cdid')->all ] } },
    {
      select => [
        'me.cd',
        { count => 'me.trackid' },
      ],
      as => [qw/
        cd
        track_count
      /],
      group_by => \'SUBSTR(me.cd, 1, 1)',
      prefetch => 'cd',
    },
  );

  is_same_sql_bind (
    $track_rs->count_rs->as_query,
    '(
      SELECT COUNT( * )
        FROM (
          SELECT SUBSTR(me.cd, 1, 1)
            FROM track me
            JOIN cd cd ON cd.cdid = me.cd
          WHERE ( me.cd IN ( ?, ?, ?, ?, ? ) )
          GROUP BY SUBSTR(me.cd, 1, 1)
        )
      count_subq
    )',
    [ map { [ 'me.cd' => $_] } ($cd_rs->get_column ('cdid')->all) ],
    'count() query generated expected SQL',
  );
}

{
    my $cd_rs = $schema->resultset('CD')->search({}, {
            distinct => 1,
            join     => [qw/ tracks /],
            prefetch => [qw/ artist /],
        });
    is($cd_rs->count, 5, 'complex prefetch + non-prefetching has_many join count correct');
    is($cd_rs->all, 5, 'complex prefetch + non-prefetching has_many join number of objects correct');

    # make sure join tracks was thrown out
    is_same_sql_bind (
      $cd_rs->as_query,
      '(
        SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track,
               artist.artistid, artist.name, artist.rank, artist.charfield
          FROM (
            SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track
              FROM cd me
              JOIN artist artist ON artist.artistid = me.artist
            GROUP BY me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track
          ) me
          JOIN artist artist ON artist.artistid = me.artist
      )',
      [],
    );



    # try the same as above, but add a condition so the tracks join can not be thrown away
    my $cd_rs2 = $cd_rs->search ({ 'tracks.title' => { '!=' => 'ugabuganoexist' } });
    is($cd_rs2->count, 5, 'complex prefetch + non-prefetching restricted has_many join count correct');
    is($cd_rs2->all, 5, 'complex prefetch + non-prefetching restricted has_many join number of objects correct');

    # the outer group_by seems like a necessary evil, if someone can figure out how to take it away
    # without breaking compat - be my guest
    is_same_sql_bind (
      $cd_rs2->as_query,
      '(
        SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track,
               artist.artistid, artist.name, artist.rank, artist.charfield
          FROM (
            SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track
              FROM cd me
              LEFT JOIN track tracks ON tracks.cd = me.cdid
              JOIN artist artist ON artist.artistid = me.artist
            WHERE ( tracks.title != ? )
            GROUP BY me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track
          ) me
          LEFT JOIN track tracks ON tracks.cd = me.cdid
          JOIN artist artist ON artist.artistid = me.artist
        WHERE ( tracks.title != ? )
        GROUP BY me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track,
                 artist.artistid, artist.name, artist.rank, artist.charfield
      )',
      [ map { [ 'tracks.title' => 'ugabuganoexist' ] } (1 .. 2) ],
    );
}

{
    my $rs = $schema->resultset('CD')->search({},
        {
           '+select' => [{ count => 'tags.tag' }],
           '+as' => ['test_count'],
           prefetch => ['tags'],
           distinct => 1,
           order_by => {'-asc' => 'tags.tag'},
           rows => 1
        }
    );
    is_same_sql_bind($rs->as_query, q{
        (SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track, me.test_count, tags.tagid, tags.cd, tags.tag
          FROM (SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track, COUNT( tags.tag ) AS test_count
                FROM cd me LEFT JOIN tags tags ON tags.cd = me.cdid
            GROUP BY me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track, tags.tag
            ORDER BY tags.tag ASC LIMIT 1)
            me
          LEFT JOIN tags tags ON tags.cd = me.cdid
         ORDER BY tags.tag ASC, tags.cd, tags.tag
        )
    }, []);
}

done_testing;
