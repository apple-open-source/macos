use strict;
use warnings;
use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan skip_all => "Class::Trigger and DBIx::ContextualFetch required"
    if $@;

  eval { require DateTime };
  plan skip_all => "Need DateTime for inflation tests" if $@;

  eval { require Clone };
  plan skip_all => "Need Clone for CDBICompat inflation tests" if $@;
}

plan tests => 6;

use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

DBICTest::Schema::CD->load_components(qw/CDBICompat::Relationships/);

DBICTest::Schema::CD->has_a( 'year', 'DateTime',
      inflate => sub { DateTime->new( year => shift ) },
      deflate => sub { shift->year }
);
Class::C3->reinitialize;

# inflation test
my $cd = $schema->resultset("CD")->find(3);

is( ref($cd->year), 'DateTime', 'year is a DateTime, ok' );

is( $cd->year->month, 1, 'inflated month ok' );

# deflate test
my $now = DateTime->now;
$cd->year( $now );
$cd->update;

($cd) = $schema->resultset("CD")->search( year => $now->year );
is( $cd->year->year, $now->year, 'deflate ok' );

# re-test using alternate deflate syntax
$schema->class("CD")->has_a( 'year', 'DateTime',
      inflate => sub { DateTime->new( year => shift ) },
      deflate => 'year'
);

# inflation test
$cd = $schema->resultset("CD")->find(3);

is( ref($cd->year), 'DateTime', 'year is a DateTime, ok' );

is( $cd->year->month, 1, 'inflated month ok' );

# deflate test
$now = DateTime->now;
$cd->year( $now );
$cd->update;

($cd) = $schema->resultset("CD")->search( year => $now->year );
is( $cd->year->year, $now->year, 'deflate ok' );

