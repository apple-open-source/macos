use warnings;  

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBIC::SqlMakerTest;
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 1;

# While this is a rather GIGO case, make sure it behaves as pre-103,
# as it may result in hard-to-track bugs
my $cds = $schema->resultset('Artist')
            ->search_related ('cds')
              ->search ({}, {
                  prefetch => [ 'single_track', { single_track => 'cd' } ],
                });

is_same_sql(
  ${$cds->as_query}->[0],
  '(
    SELECT
      cds.cdid, cds.artist, cds.title, cds.year, cds.genreid, cds.single_track,
      single_track.trackid, single_track.cd, single_track.position, single_track.title, single_track.last_updated_on, single_track.last_updated_at, single_track.small_dt,
      single_track_2.trackid, single_track_2.cd, single_track_2.position, single_track_2.title, single_track_2.last_updated_on, single_track_2.last_updated_at, single_track_2.small_dt,
      cd.cdid, cd.artist, cd.title, cd.year, cd.genreid, cd.single_track
    FROM artist me
      JOIN cd cds ON cds.artist = me.artistid
      LEFT JOIN track single_track ON single_track.trackid = cds.single_track
      LEFT JOIN track single_track_2 ON single_track_2.trackid = cds.single_track
      LEFT JOIN cd cd ON cd.cdid = single_track_2.cd
  )',
);
