use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

eval { require DateTime };
plan skip_all => "Need DateTime for inflation tests" if $@;

plan tests => 21;

$schema->class('CD')
#DBICTest::Schema::CD
->inflate_column( 'year',
    { inflate => sub { DateTime->new( year => shift ) },
      deflate => sub { shift->year } }
);
Class::C3->reinitialize;

# inflation test
my $cd = $schema->resultset("CD")->find(3);

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

$cd = $schema->resultset("CD")->find(3);
is( $cd->year->year, $now->year, 'deflate ok' );

# set_inflated_column test
eval { $cd->set_inflated_column('year', $now) };
ok(!$@, 'set_inflated_column with DateTime object');
$cd->update;

$cd = $schema->resultset("CD")->find(3);                 
is( $cd->year->year, $now->year, 'deflate ok' );

$cd = $schema->resultset("CD")->find(3);                 
my $before_year = $cd->year->year;
eval { $cd->set_inflated_column('year', \'year + 1') };
ok(!$@, 'set_inflated_column to "year + 1"');
$cd->update;

$cd = $schema->resultset("CD")->find(3);                 
is( $cd->year->year, $before_year+1, 'deflate ok' );

# store_inflated_column test
$cd = $schema->resultset("CD")->find(3);                 
eval { $cd->store_inflated_column('year', $now) };
ok(!$@, 'store_inflated_column with DateTime object');
$cd->update;

is( $cd->year->year, $now->year, 'deflate ok' );

# update tests
$cd = $schema->resultset("CD")->find(3);                 
eval { $cd->update({'year' => $now}) };
ok(!$@, 'update using DateTime object ok');
is($cd->year->year, $now->year, 'deflate ok');

$cd = $schema->resultset("CD")->find(3);                 
$before_year = $cd->year->year;
eval { $cd->update({'year' => \'year + 1'}) };
ok(!$@, 'update using scalarref ok');

$cd = $schema->resultset("CD")->find(3);                 
is($cd->year->year, $before_year + 1, 'deflate ok');

# discard_changes test
$cd = $schema->resultset("CD")->find(3);                 
# inflate the year
$before_year = $cd->year->year;
$cd->update({ year => \'year + 1'});
$cd->discard_changes;

is($cd->year->year, $before_year + 1, 'discard_changes clears the inflated value');

my $copy = $cd->copy({ year => $now, title => "zemoose" });

isnt( $copy->year->year, $before_year, "copy" );
 
# eval { $cd->store_inflated_column('year', \'year + 1') };
# print STDERR "ERROR: $@" if($@);
# ok(!$@, 'store_inflated_column to "year + 1"');

# is_deeply( $cd->year, \'year + 1', 'deflate ok' );

