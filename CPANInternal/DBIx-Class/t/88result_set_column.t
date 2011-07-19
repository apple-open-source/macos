use strict;
use warnings;

use Test::More;
use Test::Warn;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

my $schema = DBICTest->init_schema();

my $rs = $schema->resultset("CD");

cmp_ok (
  $rs->count,
    '!=',
  $rs->search ({}, {columns => ['year'], distinct => 1})->count,
  'At least one year is the same in rs'
);

my $rs_title = $rs->get_column('title');
my $rs_year = $rs->get_column('year');
my $max_year = $rs->get_column(\'MAX (year)');

is($rs_title->next, 'Spoonful of bees', "next okay");
is_deeply( [ sort $rs_year->func('DISTINCT') ], [ 1997, 1998, 1999, 2001 ],  "wantarray context okay");
ok ($max_year->next == $rs_year->max, q/get_column (\'FUNC') ok/);

my @all = $rs_title->all;
cmp_ok(scalar @all, '==', 5, "five titles returned");

cmp_ok($rs_year->max, '==', 2001, "max okay for year");
is($rs_title->min, 'Caterwaulin\' Blues', "min okay for title");

cmp_ok($rs_year->sum, '==', 9996, "three artists returned");

$rs_year->reset;
is($rs_year->next, 1999, "reset okay");

is($rs_year->first, 1999, "first okay");

warnings_exist (sub {
  is($rs_year->single, 1999, "single okay");
}, qr/Query returned more than one row/, 'single warned');


# test distinct propagation
is_deeply (
  [$rs->search ({}, { distinct => 1 })->get_column ('year')->all],
  [$rs_year->func('distinct')],
  'distinct => 1 is passed through properly',
);

# test +select/+as for single column
my $psrs = $schema->resultset('CD')->search({},
    {
        '+select'   => \'MAX(year)',
        '+as'       => 'last_year'
    }
);
lives_ok(sub { $psrs->get_column('last_year')->next }, '+select/+as additional column "last_year" present (scalar)');
dies_ok(sub { $psrs->get_column('noSuchColumn')->next }, '+select/+as nonexistent column throws exception');

# test +select/+as for overriding a column
$psrs = $schema->resultset('CD')->search({},
    {
        'select'   => \"'The Final Countdown'",
        'as'       => 'title'
    }
);
is($psrs->get_column('title')->next, 'The Final Countdown', '+select/+as overridden column "title"');


# test +select/+as for multiple columns
$psrs = $schema->resultset('CD')->search({},
    {
        '+select'   => [ \'LENGTH(title) AS title_length', 'title' ],
        '+as'       => [ 'tlength', 'addedtitle' ]
    }
);
lives_ok(sub { $psrs->get_column('tlength')->next }, '+select/+as multiple additional columns, "tlength" column present');
lives_ok(sub { $psrs->get_column('addedtitle')->next }, '+select/+as multiple additional columns, "addedtitle" column present');

# test that +select/+as specs do not leak
is_same_sql_bind (
  $psrs->get_column('year')->as_query,
  '(SELECT me.year FROM cd me)',
  [],
  'Correct SQL for get_column/as'
);

is_same_sql_bind (
  $psrs->get_column('addedtitle')->as_query,
  '(SELECT me.title FROM cd me)',
  [],
  'Correct SQL for get_column/+as col'
);

is_same_sql_bind (
  $psrs->get_column('tlength')->as_query,
  '(SELECT LENGTH(title) AS title_length FROM cd me)',
  [],
  'Correct SQL for get_column/+as func'
);

# test that order_by over a function forces a subquery
lives_ok ( sub {
  is_deeply (
    [ $psrs->search ({}, { order_by => { -desc => 'title_length' } })->get_column ('title')->all ],
    [
      "Generic Manufactured Singles",
      "Come Be Depressed With Us",
      "Caterwaulin' Blues",
      "Spoonful of bees",
      "Forkful of bees",
    ],
    'Subquery count induced by aliased ordering function',
  );
});

# test for prefetch not leaking
{
  my $rs = $schema->resultset("CD")->search({}, { prefetch => 'artist' });
  my $rsc = $rs->get_column('year');
  is( $rsc->{_parent_resultset}->{attrs}->{prefetch}, undef, 'prefetch wiped' );
}

# test sum()
is ($schema->resultset('BooksInLibrary')->get_column ('price')->sum, 125, 'Sum of a resultset works correctly');

# test sum over search_related
my $owner = $schema->resultset('Owners')->find ({ name => 'Newton' });
ok ($owner->books->count > 1, 'Owner Newton has multiple books');
is ($owner->search_related ('books')->get_column ('price')->sum, 60, 'Correctly calculated price of all owned books');


# make sure joined/prefetched get_column of a PK dtrt

$rs->reset;
my $j_rs = $rs->search ({}, { join => 'tracks' })->get_column ('cdid');
is_deeply (
  [ $j_rs->all ],
  [ map { my $c = $rs->next; ( ($c->id) x $c->tracks->count ) } (1 .. $rs->count) ],
  'join properly explodes amount of rows from get_column',
);

$rs->reset;
my $p_rs = $rs->search ({}, { prefetch => 'tracks' })->get_column ('cdid');
is_deeply (
  [ $p_rs->all ],
  [ $rs->get_column ('cdid')->all ],
  'prefetch properly collapses amount of rows from get_column',
);

done_testing;
