use strict;
use warnings;

use Test::More;
use DBIx::Class::Storage::DBI;

plan tests => 1;

my $sa = new DBIC::SQL::Abstract;

$sa->limit_dialect( 'Top' );

is(
    $sa->select( 'rubbish', [ 'foo.id', 'bar.id' ], undef, { order_by => 'artistid' }, 1, 3 ),
    'SELECT * FROM
(
    SELECT TOP 1 * FROM
    (
        SELECT TOP 4  foo.id, bar.id FROM rubbish ORDER BY artistid ASC
    ) AS foo
    ORDER BY artistid DESC
) AS bar
ORDER BY artistid ASC
',
    "make sure limit_dialect( 'Top' ) is working okay"
);
