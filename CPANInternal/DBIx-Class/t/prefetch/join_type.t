use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBIC::SqlMakerTest;
use DBICTest;

my $schema = DBICTest->init_schema();


# a regular belongs_to prefetch
my $cds = $schema->resultset('CD')->search ({}, { prefetch => 'artist' } );

my $nulls = {
  hashref => {},
  arrayref => [],
  undef => undef,
};

# make sure null-prefetches do not screw with the final sql:
for my $type (keys %$nulls) {
#  is_same_sql_bind (
#    $cds->search({}, { prefetch => { artist => $nulls->{$type} } })->as_query,
#    $cds->as_query,
#    "same sql with null $type prefetch"
#  );
}

# make sure left join is carried only starting from the first has_many
is_same_sql_bind (
  $cds->search({}, { prefetch => { artist => { cds => 'artist' } } })->as_query,
  '(
    SELECT  me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track,
            artist.artistid, artist.name, artist.rank, artist.charfield,
            cds.cdid, cds.artist, cds.title, cds.year, cds.genreid, cds.single_track,
            artist_2.artistid, artist_2.name, artist_2.rank, artist_2.charfield
      FROM cd me
      JOIN artist artist ON artist.artistid = me.artist
      LEFT JOIN cd cds ON cds.artist = artist.artistid
      LEFT JOIN artist artist_2 ON artist_2.artistid = cds.artist
    ORDER BY cds.artist, cds.year
  )',
  [],
);

done_testing;
