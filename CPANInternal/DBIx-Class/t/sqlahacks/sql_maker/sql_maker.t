use strict;
use warnings;

use Test::More;
use Test::Exception;

use lib qw(t/lib);
use DBIC::SqlMakerTest;

use_ok('DBICTest');

my $schema = DBICTest->init_schema(no_deploy => 1);

my $sql_maker = $schema->storage->sql_maker;


{
  my ($sql, @bind) = $sql_maker->insert(
            'lottery',
            {
              'day' => '2008-11-16',
              'numbers' => [13, 21, 34, 55, 89]
            }
  );

  is_same_sql_bind(
    $sql, \@bind,
    q/INSERT INTO lottery (day, numbers) VALUES (?, ?)/,
      [ ['day' => '2008-11-16'], ['numbers' => [13, 21, 34, 55, 89]] ],
    'sql_maker passes arrayrefs in insert'
  );


  ($sql, @bind) = $sql_maker->update(
            'lottery',
            {
              'day' => '2008-11-16',
              'numbers' => [13, 21, 34, 55, 89]
            }
  );

  is_same_sql_bind(
    $sql, \@bind,
    q/UPDATE lottery SET day = ?, numbers = ?/,
      [ ['day' => '2008-11-16'], ['numbers' => [13, 21, 34, 55, 89]] ],
    'sql_maker passes arrayrefs in update'
  );
}

# make sure the cookbook caveat of { $op, \'...' } no longer applies
{
  my ($sql, @bind) = $sql_maker->where({
    last_attempt => \ '< now() - interval "12 hours"',
    next_attempt => { '<', \ 'now() - interval "12 hours"' },
    created => [
      { '<=', \ '1969' },
      \ '> 1984',
    ],
  });
  is_same_sql_bind(
    $sql,
    \@bind,
    'WHERE
          (created <= 1969 OR created > 1984 )
      AND last_attempt < now() - interval "12 hours"
      AND next_attempt < now() - interval "12 hours"
    ',
    [],
  );
}

# Make sure the carp/croak override in SQLA works (via SQLAHacks)
my $file = quotemeta (__FILE__);
throws_ok (sub {
  $schema->resultset ('Artist')->search ({}, { order_by => { -asc => 'stuff', -desc => 'staff' } } )->as_query;
}, qr/$file/, 'Exception correctly croak()ed');

done_testing;
