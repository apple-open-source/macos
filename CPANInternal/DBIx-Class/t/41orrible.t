use strict;
use warnings;

use Test::More;
use DBIx::Class::Storage::DBI;

plan tests => 1;

my $sa = new DBIC::SQL::Abstract;

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
