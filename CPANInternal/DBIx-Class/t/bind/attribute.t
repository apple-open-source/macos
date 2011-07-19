use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBIC::SqlMakerTest;

use_ok('DBICTest');

my $schema = DBICTest->init_schema;

my $where_bind = {
    where => \'name like ?',
    bind  => [ 'Cat%' ],
};

my $rs;

TODO: {
    local $TODO = 'bind args order needs fixing (semifor)';

    # First, the simple cases...
    $rs = $schema->resultset('Artist')->search(
            { artistid => 1 },
            $where_bind,
    );

    is ( $rs->count, 1, 'where/bind combined' );

    $rs= $schema->resultset('Artist')->search({}, $where_bind)
        ->search({ artistid => 1});

    is ( $rs->count, 1, 'where/bind first' );

    $rs = $schema->resultset('Artist')->search({ artistid => 1})
        ->search({}, $where_bind);

    is ( $rs->count, 1, 'where/bind last' );
}

{
  # More complex cases, based primarily on the Cookbook
  # "Arbitrary SQL through a custom ResultSource" technique,
  # which seems to be the only place the bind attribute is
  # documented.  Breaking this technique probably breaks existing
  # application code.
  my $source = DBICTest::Artist->result_source_instance;
  my $new_source = $source->new($source);
  $new_source->source_name('Complex');

  $new_source->name(\<<'');
  ( SELECT a.*, cd.cdid AS cdid, cd.title AS title, cd.year AS year 
    FROM artist a
    JOIN cd ON cd.artist = a.artistid
    WHERE cd.year = ?)

  $schema->register_extra_source('Complex' => $new_source);

  $rs = $schema->resultset('Complex')->search({}, { bind => [ 1999 ] });
  is ( $rs->count, 1, 'cookbook arbitrary sql example' );

  $rs = $schema->resultset('Complex')->search({ 'artistid' => 1 }, { bind => [ 1999 ] });
  is ( $rs->count, 1, '...cookbook + search condition' );

  $rs = $schema->resultset('Complex')->search({}, { bind => [ 1999 ] })
      ->search({ 'artistid' => 1 });
  is ( $rs->count, 1, '...cookbook (bind first) + chained search' );

  $rs = $schema->resultset('Complex')->search({}, { bind => [ 1999 ] })->search({}, { where => \"title LIKE ?", bind => [ 'Spoon%' ] });
  is_same_sql_bind(
    $rs->as_query,
    "(SELECT me.artistid, me.name, me.rank, me.charfield FROM (SELECT a.*, cd.cdid AS cdid, cd.title AS title, cd.year AS year FROM artist a JOIN cd ON cd.artist = a.artistid WHERE cd.year = ?) me WHERE title LIKE ?)",
    [
      [ '!!dummy' => '1999' ], 
      [ '!!dummy' => 'Spoon%' ]
    ],
    'got correct SQL'
  );
}

{
  # More complex cases, based primarily on the Cookbook
  # "Arbitrary SQL through a custom ResultSource" technique,
  # which seems to be the only place the bind attribute is
  # documented.  Breaking this technique probably breaks existing
  # application code.

  $rs = $schema->resultset('CustomSql')->search({}, { bind => [ 1999 ] });
  is ( $rs->count, 1, 'cookbook arbitrary sql example (in separate file)' );

  $rs = $schema->resultset('CustomSql')->search({ 'artistid' => 1 }, { bind => [ 1999 ] });
  is ( $rs->count, 1, '...cookbook (in separate file) + search condition' );

  $rs = $schema->resultset('CustomSql')->search({}, { bind => [ 1999 ] })
      ->search({ 'artistid' => 1 });
  is ( $rs->count, 1, '...cookbook (bind first, in separate file) + chained search' );

  $rs = $schema->resultset('CustomSql')->search({}, { bind => [ 1999 ] })->search({}, { where => \"title LIKE ?", bind => [ 'Spoon%' ] });
  is_same_sql_bind(
    $rs->as_query,
    "(SELECT me.artistid, me.name, me.rank, me.charfield FROM (SELECT a.*, cd.cdid AS cdid, cd.title AS title, cd.year AS year FROM artist a JOIN cd ON cd.artist = a.artistid WHERE cd.year = ?) me WHERE title LIKE ?)",
    [
      [ '!!dummy' => '1999' ], 
      [ '!!dummy' => 'Spoon%' ]
    ],
    'got correct SQL (cookbook arbitrary SQL, in separate file)'
  );
}

TODO: {
    local $TODO = 'bind args order needs fixing (semifor)';
    $rs = $schema->resultset('Complex')->search({}, { bind => [ 1999 ] })
        ->search({ 'artistid' => 1 }, {
            where => \'title like ?',
            bind => [ 'Spoon%' ] });
    is ( $rs->count, 1, '...cookbook + chained search with extra bind' );
}

done_testing;
