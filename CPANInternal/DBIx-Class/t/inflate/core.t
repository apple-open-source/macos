use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

eval { require DateTime };
plan skip_all => "Need DateTime for inflation tests" if $@;

$schema->class('CD') ->inflate_column( 'year',
    { inflate => sub { DateTime->new( year => shift ) },
      deflate => sub { shift->year } }
);

my $rs = $schema->resultset('CD');

# inflation test
my $cd = $rs->find(3);

is( ref($cd->year), 'DateTime', 'year is a DateTime, ok' );

is( $cd->year->year, 1997, 'inflated year ok' );

is( $cd->year->month, 1, 'inflated month ok' );

eval { $cd->year(\'year +1'); };
ok(!$@, 'updated year using a scalarref');
$cd->update();
$cd->discard_changes();

is( ref($cd->year), 'DateTime', 'year is still a DateTime, ok' );

is( $cd->year->year, 1998, 'updated year, bypassing inflation' );

is( $cd->year->month, 1, 'month is still 1' );  

# get_inflated_column test

is( ref($cd->get_inflated_column('year')), 'DateTime', 'get_inflated_column produces a DateTime');

# deflate test
my $now = DateTime->now;
$cd->year( $now );
$cd->update;

$cd = $rs->find(3);
is( $cd->year->year, $now->year, 'deflate ok' );

# set_inflated_column test
eval { $cd->set_inflated_column('year', $now) };
ok(!$@, 'set_inflated_column with DateTime object');
$cd->update;

$cd = $rs->find(3);
is( $cd->year->year, $now->year, 'deflate ok' );

$cd = $rs->find(3);
my $before_year = $cd->year->year;
eval { $cd->set_inflated_column('year', \'year + 1') };
ok(!$@, 'set_inflated_column to "year + 1"');
$cd->update;

$cd->store_inflated_column('year', \'year + 1');
is_deeply( $cd->year, \'year + 1', 'scalarref deflate passthrough ok' );

$cd = $rs->find(3);
is( $cd->year->year, $before_year+1, 'deflate ok' );

# store_inflated_column test
$cd = $rs->find(3);
eval { $cd->store_inflated_column('year', $now) };
ok(!$@, 'store_inflated_column with DateTime object');
$cd->update;

is( $cd->year->year, $now->year, 'deflate ok' );

# update tests
$cd = $rs->find(3);
eval { $cd->update({'year' => $now}) };
ok(!$@, 'update using DateTime object ok');
is($cd->year->year, $now->year, 'deflate ok');

$cd = $rs->find(3);
$before_year = $cd->year->year;
eval { $cd->update({'year' => \'year + 1'}) };
ok(!$@, 'update using scalarref ok');

$cd = $rs->find(3);
is($cd->year->year, $before_year + 1, 'deflate ok');

# discard_changes test
$cd = $rs->find(3);
# inflate the year
$before_year = $cd->year->year;
$cd->update({ year => \'year + 1'});
$cd->discard_changes;

is($cd->year->year, $before_year + 1, 'discard_changes clears the inflated value');

my $copy = $cd->copy({ year => $now, title => "zemoose" });

is( $copy->year->year, $now->year, "copy" );



my $artist = $cd->artist;
my $sval = \ '2012';

$cd = $rs->create ({
        artist => $artist,
        year => $sval,
        title => 'create with scalarref',
});

is ($cd->year, $sval, 'scalar value retained');
my $cd2 = $cd->copy ({ title => 'copy with scalar in coldata' });
is ($cd2->year, $sval, 'copied scalar value retained');

$cd->discard_changes;
is ($cd->year->year, 2012, 'infation upon reload');

$cd2->discard_changes;
is ($cd2->year->year, 2012, 'infation upon reload of copy');


my $precount = $rs->count;
$cd = $rs->update_or_create ({artist => $artist, title => 'nonexisting update/create test row', year => $sval });
is ($rs->count, $precount + 1, 'Row created');

is ($cd->year, $sval, 'scalar value retained on creating update_or_create');
$cd->discard_changes;
is ($cd->year->year, 2012, 'infation upon reload');

my $sval2 = \ '2013';

$cd = $rs->update_or_create ({artist => $artist, title => 'nonexisting update/create test row', year => $sval2 });
is ($rs->count, $precount + 1, 'No more rows created');

is ($cd->year, $sval2, 'scalar value retained on updating update_or_create');
$cd->discard_changes;
is ($cd->year->year, 2013, 'infation upon reload');

done_testing;
