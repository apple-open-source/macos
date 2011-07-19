use strict;
use warnings;  

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;
use DBI::Const::GetInfoType;
use DBIC::SqlMakerTest;

my ($dsn, $user, $pass) = @ENV{map { "DBICTEST_MYSQL_${_}" } qw/DSN USER PASS/};

#warn "$dsn $user $pass";

plan skip_all => 'Set $ENV{DBICTEST_MYSQL_DSN}, _USER and _PASS to run this test'
  unless ($dsn && $user);

my $schema = DBICTest::Schema->connect($dsn, $user, $pass);

my $dbh = $schema->storage->dbh;

$dbh->do("DROP TABLE IF EXISTS artist;");

$dbh->do("CREATE TABLE artist (artistid INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, name VARCHAR(100), rank INTEGER NOT NULL DEFAULT '13', charfield CHAR(10));");

$dbh->do("DROP TABLE IF EXISTS cd;");

$dbh->do("CREATE TABLE cd (cdid INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, artist INTEGER, title TEXT, year DATE, genreid INTEGER, single_track INTEGER);");

$dbh->do("DROP TABLE IF EXISTS producer;");

$dbh->do("CREATE TABLE producer (producerid INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, name TEXT);");

$dbh->do("DROP TABLE IF EXISTS cd_to_producer;");

$dbh->do("CREATE TABLE cd_to_producer (cd INTEGER,producer INTEGER);");

$dbh->do("DROP TABLE IF EXISTS owners;");

$dbh->do("CREATE TABLE owners (id INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, name VARCHAR(100) NOT NULL);");

$dbh->do("DROP TABLE IF EXISTS books;");

$dbh->do("CREATE TABLE books (id INTEGER NOT NULL AUTO_INCREMENT PRIMARY KEY, source VARCHAR(100) NOT NULL, owner integer NOT NULL, title varchar(100) NOT NULL,  price integer);");

#'dbi:mysql:host=localhost;database=dbic_test', 'dbic_test', '');

# make sure sqlt_type overrides work (::Storage::DBI::mysql does this) 
{
  my $schema = DBICTest::Schema->connect($dsn, $user, $pass);

  ok (!$schema->storage->_dbh, 'definitely not connected');
  is ($schema->storage->sqlt_type, 'MySQL', 'sqlt_type correct pre-connection');
}

# This is in Core now, but it's here just to test that it doesn't break
$schema->class('Artist')->load_components('PK::Auto');

# test primary key handling
my $new = $schema->resultset('Artist')->create({ name => 'foo' });
ok($new->artistid, "Auto-PK worked");

# test LIMIT support
for (1..6) {
    $schema->resultset('Artist')->create({ name => 'Artist ' . $_ });
}
my $it = $schema->resultset('Artist')->search( {},
    { rows => 3,
      offset => 2,
      order_by => 'artistid' }
);
is( $it->count, 3, "LIMIT count ok" );  # ask for 3 rows out of 7 artists
is( $it->next->name, "Artist 2", "iterator->next ok" );
$it->next;
$it->next;
is( $it->next, undef, "next past end of resultset ok" );

my $test_type_info = {
    'artistid' => {
        'data_type' => 'INT',
        'is_nullable' => 0,
        'size' => 11,
        'default_value' => undef,
    },
    'name' => {
        'data_type' => 'VARCHAR',
        'is_nullable' => 1,
        'size' => 100,
        'default_value' => undef,
    },
    'rank' => {
        'data_type' => 'INT',
        'is_nullable' => 0,
        'size' => 11,
        'default_value' => 13,
    },
    'charfield' => {
        'data_type' => 'CHAR',
        'is_nullable' => 1,
        'size' => 10,
        'default_value' => undef,
    },
};

$schema->populate ('Owners', [
  [qw/id  name  /],
  [qw/1   wiggle/],
  [qw/2   woggle/],
  [qw/3   boggle/],
]);

$schema->populate ('BooksInLibrary', [
  [qw/source  owner title   /],
  [qw/Library 1     secrets1/],
  [qw/Eatery  1     secrets2/],
  [qw/Library 2     secrets3/],
]);

#
# try a distinct + prefetch on tables with identically named columns 
# (mysql doesn't seem to like subqueries with equally named columns)
#

{
  # try a ->has_many direction (due to a 'multi' accessor the select/group_by group is collapsed)
  my $owners = $schema->resultset ('Owners')->search (
    { 'books.id' => { '!=', undef }},
    { prefetch => 'books', distinct => 1 }
  );
  my $owners2 = $schema->resultset ('Owners')->search ({ id => { -in => $owners->get_column ('me.id')->as_query }});
  for ($owners, $owners2) {
    is ($_->all, 2, 'Prefetched grouped search returns correct number of rows');
    is ($_->count, 2, 'Prefetched grouped search returns correct count');
  }

  # try a ->belongs_to direction (no select collapse)
  my $books = $schema->resultset ('BooksInLibrary')->search (
    { 'owner.name' => 'wiggle' },
    { prefetch => 'owner', distinct => 1 }
  );
  my $books2 = $schema->resultset ('BooksInLibrary')->search ({ id => { -in => $books->get_column ('me.id')->as_query }});
  for ($books, $books2) {
    is ($_->all, 1, 'Prefetched grouped search returns correct number of rows');
    is ($_->count, 1, 'Prefetched grouped search returns correct count');
  }
}

SKIP: {
    my $mysql_version = $dbh->get_info( $GetInfoType{SQL_DBMS_VER} );
    skip "Cannot determine MySQL server version", 1 if !$mysql_version;

    my ($v1, $v2, $v3) = $mysql_version =~ /^(\d+)\.(\d+)(?:\.(\d+))?/;
    skip "Cannot determine MySQL server version", 1 if !$v1 || !defined($v2);

    $v3 ||= 0;

    if( ($v1 < 5) || ($v1 == 5 && $v2 == 0 && $v3 <= 3) ) {
        $test_type_info->{charfield}->{data_type} = 'VARCHAR';
    }

    my $type_info = $schema->storage->columns_info_for('artist');
    is_deeply($type_info, $test_type_info, 'columns_info_for - column data types');
}

my $cd = $schema->resultset ('CD')->create ({});
my $producer = $schema->resultset ('Producer')->create ({});
lives_ok { $cd->set_producers ([ $producer ]) } 'set_relationship doesnt die';

{
  my $artist = $schema->resultset('Artist')->next;
  my $cd = $schema->resultset('CD')->next;
  $cd->set_from_related ('artist', $artist);
  $cd->update;

  my $rs = $schema->resultset('CD')->search ({}, { prefetch => 'artist' });

  lives_ok sub {
    my $cd = $rs->next;
    is ($cd->artist->name, $artist->name, 'Prefetched artist');
  }, 'join does not throw (mysql 3 test)';

  # induce a jointype override, make sure it works even if we don't have mysql3
  local $schema->storage->sql_maker->{_default_jointype} = 'inner';
  is_same_sql_bind (
    $rs->as_query,
    '(
      SELECT me.cdid, me.artist, me.title, me.year, me.genreid, me.single_track,
             artist.artistid, artist.name, artist.rank, artist.charfield
        FROM cd me
        INNER JOIN artist artist ON artist.artistid = me.artist
    )',
    [],
    'overriden default join type works',
  );
}

## Can we properly deal with the null search problem?
##
## Only way is to do a SET SQL_AUTO_IS_NULL = 0; on connect
## But I'm not sure if we should do this or not (Ash, 2008/06/03)
#
# There is now a built-in function to do this, test that everything works
# with it (ribasushi, 2009/07/03)

NULLINSEARCH: {
    my $ansi_schema = DBICTest::Schema->connect ($dsn, $user, $pass, { on_connect_call => 'set_strict_mode' });

    $ansi_schema->resultset('Artist')->create ({ name => 'last created artist' });

    ok my $artist1_rs = $ansi_schema->resultset('Artist')->search({artistid=>6666})
      => 'Created an artist resultset of 6666';

    is $artist1_rs->count, 0
      => 'Got no returned rows';

    ok my $artist2_rs = $ansi_schema->resultset('Artist')->search({artistid=>undef})
      => 'Created an artist resultset of undef';

    is $artist2_rs->count, 0
      => 'got no rows';

    my $artist = $artist2_rs->single;

    is $artist => undef
      => 'Nothing Found!';
}

# check for proper grouped counts
{
  my $ansi_schema = DBICTest::Schema->connect ($dsn, $user, $pass, { on_connect_call => 'set_strict_mode' });
  my $rs = $ansi_schema->resultset('CD');

  my $years;
  $years->{$_->year|| scalar keys %$years}++ for $rs->all;  # NULL != NULL, thus the keys eval

  lives_ok ( sub {
    is (
      $rs->search ({}, { group_by => 'year'})->count,
      scalar keys %$years,
      'grouped count correct',
    );
  }, 'Grouped count does not throw');
}

ZEROINSEARCH: {
  my $cds_per_year = {
    2001 => 2,
    2002 => 1,
    2005 => 3,
  };

  my $rs = $schema->resultset ('CD');
  $rs->delete;
  for my $y (keys %$cds_per_year) {
    for my $c (1 .. $cds_per_year->{$y} ) {
      $rs->create ({ title => "CD $y-$c", artist => 1, year => "$y-01-01" });
    }
  }

  is ($rs->count, 6, 'CDs created successfully');

  $rs = $rs->search ({}, {
    select => [ \ 'YEAR(year)' ], as => ['y'], distinct => 1,
  });

  is_deeply (
    [ sort ($rs->get_column ('y')->all) ],
    [ sort keys %$cds_per_year ],
    'Years group successfully',
  );

  $rs->create ({ artist => 1, year => '0-1-1', title => 'Jesus Rap' });

  is_deeply (
    [ sort $rs->get_column ('y')->all ],
    [ 0, sort keys %$cds_per_year ],
    'Zero-year groups successfully',
  );

  # convoluted search taken verbatim from list 
  my $restrict_rs = $rs->search({ -and => [
    year => { '!=', 0 },
    year => { '!=', undef }
  ]});

  is_deeply (
    [ $restrict_rs->get_column('y')->all ],
    [ $rs->get_column ('y')->all ],
    'Zero year was correctly excluded from resultset',
  );
}

## If find() is the first query after connect()
## DBI::Storage::sql_maker() will be called before
## _determine_driver() and so the ::SQLHacks class for MySQL
## will not be used

my $schema2 = DBICTest::Schema->connect($dsn, $user, $pass);
$schema2->resultset("Artist")->find(4);
isa_ok($schema2->storage->sql_maker, 'DBIx::Class::SQLAHacks::MySQL');

done_testing;
