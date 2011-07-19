use strict;
use warnings;

use Test::More;
use Test::Exception;

use lib qw(t/lib);
use DBIC::SqlMakerTest;
use DBICTest;

my $schema = DBICTest->init_schema();

my $rs = $schema->resultset('CD')->search (
  { 'tracks.id' => { '!=', 666 }},
  { join => 'artist', prefetch => 'tracks', rows => 2 }
);

my $rel_rs = $rs->search_related ('tags', { 'tags.tag' => { '!=', undef }}, { distinct => 1});

is_same_sql_bind (
  $rel_rs->as_query,
  '(
    SELECT tags.tagid, tags.cd, tags.tag
      FROM (
        SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track
          FROM cd me
          JOIN artist artist ON artist.artistid = me.artist
          LEFT JOIN track tracks ON tracks.cd = me.cdid 
        WHERE ( tracks.id != ? )
        LIMIT 2
      ) me
      JOIN artist artist ON artist.artistid = me.artist
      JOIN tags tags ON tags.cd = me.cdid
    WHERE ( tags.tag IS NOT NULL )
    GROUP BY tags.tagid, tags.cd, tags.tag
  )',

  [ [ 'tracks.id' => 666 ] ],
  'Prefetch spec successfully stripped on search_related'
);

done_testing;
