use strict;
use warnings;

use Test::More;

use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

plan tests => 8;

my $schema = DBICTest->init_schema();
my $art_rs = $schema->resultset('Artist');
my $cdrs = $schema->resultset('CD');

{
  my $cdrs2 = $cdrs->search({
    artist_id => { 'in' => $art_rs->search({}, { rows => 1 })->get_column( 'id' )->as_query },
  });

  is_same_sql_bind(
    $cdrs2->as_query,
    "(SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track FROM cd me WHERE artist_id IN ( SELECT id FROM artist me LIMIT 1 ))",
    [],
  );
}

{
  my $rs = $art_rs->search(
    {},
    {
      'select' => [
        $cdrs->search({}, { rows => 1 })->get_column('id')->as_query,
      ],
    },
  );

  is_same_sql_bind(
    $rs->as_query,
    "(SELECT (SELECT id FROM cd me LIMIT 1) FROM artist me)",
    [],
  );
}

{
  my $rs = $art_rs->search(
    {},
    {
      '+select' => [
        $cdrs->search({}, { rows => 1 })->get_column('id')->as_query,
      ],
    },
  );

  is_same_sql_bind(
    $rs->as_query,
    "(SELECT me.artistid, me.name, me.rank, me.charfield, (SELECT id FROM cd me LIMIT 1) FROM artist me)",
    [],
  );
}

# simple from
{
  my $rs = $cdrs->search(
    {},
    {
      alias => 'cd2',
      from => [
        { cd2 => $cdrs->search({ id => { '>' => 20 } })->as_query },
      ],
    },
  );

  is_same_sql_bind(
    $rs->as_query,
    "(SELECT cd2.cdid, cd2.artist, cd2.title, cd2.year, cd2.genreid, cd2.single_track FROM (
        SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track FROM cd me WHERE ( id > ? )
     ) cd2)",
    [
      [ 'id', 20 ]
    ],
  );
}

# nested from
{
  my $art_rs2 = $schema->resultset('Artist')->search({}, 
  {
    from => [ { 'me' => 'artist' }, 
      [ { 'cds' => $cdrs->search({},{ 'select' => [\'me.artist as cds_artist' ]})->as_query },
      { 'me.artistid' => 'cds_artist' } ] ]
  });

  is_same_sql_bind(
    $art_rs2->as_query,
    "(SELECT me.artistid, me.name, me.rank, me.charfield FROM artist me JOIN (SELECT me.artist as cds_artist FROM cd me) cds ON me.artistid = cds_artist)",
    []
  );


}

# nested subquery in from
{
  my $rs = $cdrs->search(
    {},
    {
      alias => 'cd2',
      from => [
        { cd2 => $cdrs->search(
            { id => { '>' => 20 } }, 
            { 
                alias => 'cd3',
                from => [ 
                { cd3 => $cdrs->search( { id => { '<' => 40 } } )->as_query }
                ],
            }, )->as_query },
      ],
    },
  );

  is_same_sql_bind(
    $rs->as_query,
    "(SELECT cd2.cdid, cd2.artist, cd2.title, cd2.year, cd2.genreid, cd2.single_track
      FROM
        (SELECT cd3.cdid, cd3.artist, cd3.title, cd3.year, cd3.genreid, cd3.single_track
          FROM
            (SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track
              FROM cd me WHERE ( id < ? ) ) cd3
          WHERE ( id > ? ) ) cd2)",
    [
      [ 'id', 40 ], 
      [ 'id', 20 ]
    ],
  );

}

{
  my $rs = $cdrs->search({
    year => {
      '=' => $cdrs->search(
        { artistid => { '=' => \'me.artistid' } },
        { alias => 'inner' }
      )->get_column('year')->max_rs->as_query,
    },
  });
  is_same_sql_bind(
    $rs->as_query,
    "(SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track FROM cd me WHERE year = (SELECT MAX(inner.year) FROM cd inner WHERE artistid = me.artistid))",
    [],
  );
}

{
  my $rs = $cdrs->search(
    {},
    {
      alias => 'cd2',
      from => [
        { cd2 => $cdrs->search({ title => 'Thriller' })->as_query },
      ],
    },
  );

  is_same_sql_bind(
    $rs->as_query,
    "(SELECT cd2.cdid, cd2.artist, cd2.title, cd2.year, cd2.genreid, cd2.single_track FROM (
        SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track FROM cd me WHERE ( title = ? )
     ) cd2)",
    [ [ 'title', 'Thriller' ] ],
  );
}
