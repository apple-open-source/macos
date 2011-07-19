use strict;
use warnings;

use Test::More;

use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

my $cd_rs = $schema->resultset('CD')->search ({genreid => { '!=', undef } }, { order_by => 'cdid' });
my $track_cnt = $cd_rs->search({}, { rows => 1 })->search_related ('tracks')->count;

my %basecols = $cd_rs->first->get_columns;

# the current implementation of get_inflated_columns will "inflate"
# relationships by simply calling the accessor, when you have
# identically named columns and relationships (you shouldn't anyway)
# I consider this wrong, but at the same time appreciate the
# ramifications of changing this. Thus the value override  and the
# TODO to go with it. Delete all of this if ever resolved.
my %todo_rel_inflation_override = ( artist => $basecols{artist} );
TODO: {
  local $TODO = 'Treating relationships as inflatable data is wrong - see comment in ' . __FILE__;
  ok (! keys %todo_rel_inflation_override);
}

my $plus_rs = $cd_rs->search (
  {},
  { join => 'tracks', distinct => 1, '+select' => { count => 'tracks.trackid' }, '+as' => 'tr_cnt' },
);

is_deeply (
  { $plus_rs->first->get_columns },
  { %basecols, tr_cnt => $track_cnt },
  'extra columns returned by get_columns',
);

is_deeply (
  { $plus_rs->first->get_inflated_columns, %todo_rel_inflation_override },
  { %basecols, tr_cnt => $track_cnt },
  'extra columns returned by get_inflated_columns without inflatable columns',
);

SKIP: {
  eval { require DateTime };
  skip "Need DateTime for +select/get_inflated_columns tests", 1 if $@;

  $schema->class('CD')->inflate_column( 'year',
    { inflate => sub { DateTime->new( year => shift ) },
      deflate => sub { shift->year } }
  );

  $basecols{year} = DateTime->new ( year => $basecols{year} );

  is_deeply (
    { $plus_rs->first->get_inflated_columns, %todo_rel_inflation_override },
    { %basecols, tr_cnt => $track_cnt },
    'extra columns returned by get_inflated_columns',
  );
}

done_testing;
