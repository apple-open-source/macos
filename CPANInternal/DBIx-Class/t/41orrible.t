use strict;
use warnings;

use Test::More;
use DBIx::Class::SQLAHacks::OracleJoins;

use lib qw(t/lib);
use DBICTest; # do not remove even though it is not used
use DBIC::SqlMakerTest;

plan tests => 4;

my $sa = new DBIx::Class::SQLAHacks::OracleJoins;

$sa->limit_dialect('RowNum');

is($sa->select('rubbish',
                  [ 'foo.id', 'bar.id', \'TO_CHAR(foo.womble, "blah")' ],
                  undef, undef, 1, 3),
   'SELECT * FROM
(
    SELECT A.*, ROWNUM r FROM
    (
        SELECT foo.id AS col1, bar.id AS col2, TO_CHAR(foo.womble, "blah") AS col3 FROM rubbish 
    ) A
    WHERE ROWNUM < 5
) B
WHERE r >= 4
', 'Munged stuff to make Oracle not explode');

# test WhereJoins
# search with undefined or empty $cond

#  my ($self, $table, $fields, $where, $order, @rest) = @_;
my ($sql, @bind) = $sa->select(
    [
        { me => "cd" },
        [
            { "-join_type" => "LEFT", artist => "artist" },
            { "artist.artistid" => "me.artist" },
        ],
    ],
    [ 'cd.cdid', 'cd.artist', 'cd.title', 'cd.year', 'artist.artistid', 'artist.name' ],
    undef,
    undef
);
is_same_sql_bind(
  $sql, \@bind,
  'SELECT cd.cdid, cd.artist, cd.title, cd.year, artist.artistid, artist.name FROM cd me, artist artist WHERE ( artist.artistid(+) = me.artist )', [],
  'WhereJoins search with empty where clause'
);

($sql, @bind) = $sa->select(
    [
        { me => "cd" },
        [
            { "-join_type" => "", artist => "artist" },
            { "artist.artistid" => "me.artist" },
        ],
    ],
    [ 'cd.cdid', 'cd.artist', 'cd.title', 'cd.year', 'artist.artistid', 'artist.name' ],
    { 'artist.artistid' => 3 },
    undef
);
is_same_sql_bind(
  $sql, \@bind,
  'SELECT cd.cdid, cd.artist, cd.title, cd.year, artist.artistid, artist.name FROM cd me, artist artist WHERE ( ( ( artist.artistid = me.artist ) AND ( artist.artistid = ? ) ) )', [3],
  'WhereJoins search with where clause'
);

($sql, @bind) = $sa->select(
    [
        { me => "cd" },
        [
            { "-join_type" => "LEFT", artist => "artist" },
            { "artist.artistid" => "me.artist" },
        ],
    ],
    [ 'cd.cdid', 'cd.artist', 'cd.title', 'cd.year', 'artist.artistid', 'artist.name' ],
    [{ 'artist.artistid' => 3 }, { 'me.cdid' => 5 }],
    undef
);
is_same_sql_bind(
  $sql, \@bind,
  'SELECT cd.cdid, cd.artist, cd.title, cd.year, artist.artistid, artist.name FROM cd me, artist artist WHERE ( ( ( artist.artistid(+) = me.artist ) AND ( ( ( artist.artistid = ? ) OR ( me.cdid = ? ) ) ) ) )', [3, 5],
  'WhereJoins search with or in where clause'
);


