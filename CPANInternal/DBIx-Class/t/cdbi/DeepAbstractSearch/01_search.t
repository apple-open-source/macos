use strict;
use Test::More;

BEGIN {
    eval "use DBIx::Class::CDBICompat;";
    if ($@) {
        plan (skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@");
        next;
    }

    plan skip_all => 'needs DBD::SQLite for testing'
        unless eval { require DBD::SQLite };
    
    plan skip_all => 'needs Class::DBI::Plugin::DeepAbstractSearch'
        unless eval { require Class::DBI::Plugin::DeepAbstractSearch };
    
    plan tests => 19;
}

my $DB  = "t/var/cdbi_testdb";
unlink $DB if -e $DB;

my @DSN = ("dbi:SQLite:dbname=$DB", '', '', { AutoCommit => 0 });

package Music::DBI;
use base qw(DBIx::Class::CDBICompat);
use Class::DBI::Plugin::DeepAbstractSearch;
__PACKAGE__->connection(@DSN);

my $sql = <<'SQL_END';

---------------------------------------
-- Artists
---------------------------------------
CREATE TABLE artists (
    id INTEGER NOT NULL PRIMARY KEY,
    name VARCHAR(32)
);

INSERT INTO artists VALUES (1, "Willie Nelson");
INSERT INTO artists VALUES (2, "Patsy Cline");

---------------------------------------
-- Labels
---------------------------------------
CREATE TABLE labels (
    id INTEGER NOT NULL PRIMARY KEY,
    name VARCHAR(32)
);

INSERT INTO labels VALUES (1, "Columbia");
INSERT INTO labels VALUES (2, "Sony");
INSERT INTO labels VALUES (3, "Supraphon");

---------------------------------------
-- CDs
---------------------------------------
CREATE TABLE cds (
    id INTEGER NOT NULL PRIMARY KEY,
    label INTEGER,
    artist INTEGER,
    title VARCHAR(32),
    year INTEGER
);
INSERT INTO cds VALUES (1, 1, 1, "Songs", 2005);
INSERT INTO cds VALUES (2, 2, 1, "Read Headed Stanger", 2000);
INSERT INTO cds VALUES (3, 1, 1, "Wanted! The Outlaws", 2004);
INSERT INTO cds VALUES (4, 2, 1, "The Very Best of Willie Nelson", 1999);

INSERT INTO cds VALUES (5, 1, 2, "12 Greates Hits", 1999);
INSERT INTO cds VALUES (6, 2, 2, "Sweet Dreams", 1995);
INSERT INTO cds VALUES (7, 3, 2, "The Best of Patsy Cline", 1991);

---------------------------------------
-- Tracks
---------------------------------------
CREATE TABLE tracks (
    id INTEGER NOT NULL PRIMARY KEY,
    cd INTEGER,
    position INTEGER,
    title VARCHAR(32)
);
INSERT INTO tracks VALUES (1, 1, 1, "Songs: Track 1");
INSERT INTO tracks VALUES (2, 1, 2, "Songs: Track 2");
INSERT INTO tracks VALUES (3, 1, 3, "Songs: Track 3");
INSERT INTO tracks VALUES (4, 1, 4, "Songs: Track 4");

INSERT INTO tracks VALUES (5, 2, 1, "Read Headed Stanger: Track 1");
INSERT INTO tracks VALUES (6, 2, 2, "Read Headed Stanger: Track 2");
INSERT INTO tracks VALUES (7, 2, 3, "Read Headed Stanger: Track 3");
INSERT INTO tracks VALUES (8, 2, 4, "Read Headed Stanger: Track 4");

INSERT INTO tracks VALUES (9, 3, 1, "Wanted! The Outlaws: Track 1");
INSERT INTO tracks VALUES (10, 3, 2, "Wanted! The Outlaws: Track 2");

INSERT INTO tracks VALUES (11, 4, 1, "The Very Best of Willie Nelson: Track 1");
INSERT INTO tracks VALUES (12, 4, 2, "The Very Best of Willie Nelson: Track 2");
INSERT INTO tracks VALUES (13, 4, 3, "The Very Best of Willie Nelson: Track 3");
INSERT INTO tracks VALUES (14, 4, 4, "The Very Best of Willie Nelson: Track 4");
INSERT INTO tracks VALUES (15, 4, 5, "The Very Best of Willie Nelson: Track 5");
INSERT INTO tracks VALUES (16, 4, 6, "The Very Best of Willie Nelson: Track 6");

INSERT INTO tracks VALUES (17, 5, 1, "12 Greates Hits: Track 1");
INSERT INTO tracks VALUES (18, 5, 2, "12 Greates Hits: Track 2");
INSERT INTO tracks VALUES (19, 5, 3, "12 Greates Hits: Track 3");
INSERT INTO tracks VALUES (20, 5, 4, "12 Greates Hits: Track 4");

INSERT INTO tracks VALUES (21, 6, 1, "Sweet Dreams: Track 1");
INSERT INTO tracks VALUES (22, 6, 2, "Sweet Dreams: Track 2");
INSERT INTO tracks VALUES (23, 6, 3, "Sweet Dreams: Track 3");
INSERT INTO tracks VALUES (24, 6, 4, "Sweet Dreams: Track 4");

INSERT INTO tracks VALUES (25, 7, 1, "The Best of Patsy Cline: Track 1");
INSERT INTO tracks VALUES (26, 7, 2, "The Best of Patsy Cline: Track 2");

SQL_END

foreach my $statement (split /;/, $sql) {
    $statement =~ s/^\s*//gs;
    $statement =~ s/\s*$//gs;
    next unless $statement;
    Music::DBI->db_Main->do($statement) or die "$@ $!";
}

Music::DBI->dbi_commit;

package Music::Artist;
use base 'Music::DBI';
Music::Artist->table('artists');
Music::Artist->columns(All => qw/id name/);


package Music::Label;
use base 'Music::DBI';
Music::Label->table('labels');
Music::Label->columns(All => qw/id name/);

package Music::CD;
use base 'Music::DBI';
Music::CD->table('cds');
Music::CD->columns(All => qw/id label artist title year/);


package Music::Track;
use base 'Music::DBI';
Music::Track->table('tracks');
Music::Track->columns(All => qw/id cd position title/);

Music::Artist->has_many(cds => 'Music::CD');
Music::Label->has_many(cds => 'Music::CD');
Music::CD->has_many(tracks => 'Music::Track');
Music::CD->has_a(artist => 'Music::Artist');
Music::CD->has_a(label => 'Music::Label');
Music::Track->has_a(cd => 'Music::CD');

package main;

{
    my $where = { };
    my $attr;
    my @artists = Music::Artist->deep_search_where($where, $attr);
    is_deeply [ sort @artists ], [ 1, 2 ],      "all without order";
}

{
    my $where = { };
    my $attr = { order_by => 'name' };
    my @artists = Music::Artist->deep_search_where($where, $attr);
    is_deeply \@artists, [ 2, 1 ],      "all with ORDER BY name";
}

{
    my $where = { };
    my $attr = { order_by => 'name DESC' };
    my @artists = Music::Artist->deep_search_where($where, $attr);
    is_deeply \@artists, [ 1, 2 ],      "all with ORDER BY name DESC";
}

{
    my $where = { name => { -like => 'Patsy Cline' }, };
    my $attr;
    my @artists = Music::Artist->deep_search_where($where, $attr);
    is_deeply \@artists, [ 2 ],         "simple search";
}

{
    my $where = { 'artist.name' => 'Patsy Cline' };
    my $attr = { } ;
    my @cds = Music::CD->deep_search_where($where, $attr);
    is_deeply [ sort @cds ], [ 5, 6, 7 ],   "Patsy's CDs";
}

{
    my $where = { 'artist.name' => 'Patsy Cline' };
    my $attr = { order_by => "title" } ;
    my @cds = Music::CD->deep_search_where($where, $attr);
    is_deeply [ @cds ], [ 5, 6, 7 ],        "Patsy's CDs by title";

    my $count = Music::CD->count_deep_search_where($where);
    is_deeply $count, 3,        "count Patsy's CDs by title";
}

{
    my $where = { 'cd.title' => { -like => 'S%' }, };
    my $attr = { order_by => "cd.title, title" } ;
    my @cds = Music::Track->deep_search_where($where, $attr);
    is_deeply [ @cds ], [1, 2, 3, 4, 21, 22, 23, 24 ],      "Tracks from CDs whose name starts with 'S'";
}

{
    my $where = {
        'cd.artist.name' => { -like => 'W%' },
        'cd.year' => { '>' => 2000 },
        'position' => { '<' => 3 }
        };
    my $attr = { order_by => "cd.title DESC, title" } ;
    my @cds = Music::Track->deep_search_where($where, $attr);
    is_deeply [ @cds ], [ 9, 10, 1, 2 ],        "First 2 tracks from W's albums after 2000 ";

    my $count = Music::Track->count_deep_search_where($where);
    is_deeply $count, 4,        "Count First 2 tracks from W's albums after 2000";
}

{
    my $where = {
        'cd.artist.name' => { -like => 'W%' },
        'cd.year' => { '>' => 2000 },
        'position' => { '<' => 3 }
        };
    my $attr = { order_by => [ 'cd.title DESC' , 'title' ] } ;
    my @cds = Music::Track->deep_search_where($where, $attr);
    is_deeply [ @cds ], [ 9, 10, 1, 2 ],        "First 2 tracks from W's albums after 2000, array ref order ";

    my $count = Music::Track->count_deep_search_where($where);
    is_deeply $count, 4,        "Count First 2 tracks from W's albums after 2000, array ref order";
}

{
    my $where = { 'cd.title' => [ -and => { -like => '%o%' }, { -like => '%W%' } ] };
    my $attr = { order_by => [ 'cd.id' ] } ;

    my @tracks = Music::Track->deep_search_where($where, $attr);
    is_deeply [ @tracks ], [ 3, 3, 4, 4, 4, 4, 4, 4 ],      "Tracks from CD titles containing 'o' AND 'W'";
}

{
    my $where = { 'cd.year' => [ 1995, 1999 ] };
    my $attr = { order_by => [ 'cd.id' ] } ;

    my @tracks = Music::Track->deep_search_where($where, $attr);
    is_deeply [ @tracks ], [ 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6 ],
            "Tracks from CDs from 1995, 1999";
}

{
    my $where = { 'cd.year' => { -in => [ 1995, 1999 ] } };
    my $attr = { order_by => [ 'cd.id' ] } ;

    my @tracks = Music::Track->deep_search_where($where, $attr);
    is_deeply [ @tracks ], [ 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6 ],
            "Tracks from CDs in 1995, 1999";
}

{
    my $where = { -and => [ 'cd.year' => [ 1995, 1999 ], position => { '<=', 2 } ] };
    my $attr = { order_by => [ 'cd.id' ] } ;

    my @tracks = Music::Track->deep_search_where($where, $attr);
    is_deeply [ @tracks ], [ 4, 4, 5, 5, 6, 6 ],
            "First 2 tracks Tracks from CDs from 1995, 1999";
}

{
    my $where = { -and => [ 'cd.year' => { -in => [ 1995, 1999 ] }, position => { '<=', 2 } ] };
    my $attr = { order_by => [ 'cd.id' ] } ;

    my @tracks = Music::Track->deep_search_where($where, $attr);
    is_deeply [ @tracks ], [ 4, 4, 5, 5, 6, 6 ],
            "First 2 tracks Tracks from CDs in 1995, 1999";
}

{
    my $where = { 'label.name' => { -in => [ 'Sony', 'Supraphon', 'Bogus' ] } };
    my $attr = { order_by => [ 'id' ] } ;

    my @cds = Music::CD->deep_search_where($where, $attr);
    is_deeply [ @cds ], [ 2, 4, 6, 7 ],
            "CDs from Sony or Supraphon";
}

{
    my $where = { 'label.name' => [ 'Sony', 'Supraphon', 'Bogus' ] };
    my $attr = { order_by => [ 'id' ] } ;

    my @cds = Music::CD->deep_search_where($where, $attr);
    is_deeply [ @cds ], [ 2, 4, 6, 7 ],
            "CDs from Sony or Supraphon";
}

END { unlink $DB if -e $DB }

