use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

my $schema = DBICTest->init_schema;

# Trick the sqlite DB to use Top limit emulation
# We could test all of this via $sq->$op directly,
# but some conditions need a $rsrc
delete $schema->storage->_sql_maker->{_cached_syntax};
$schema->storage->_sql_maker->limit_dialect ('Top');

my $rs = $schema->resultset ('BooksInLibrary')->search ({}, { prefetch => 'owner', rows => 1, offset => 3 });

sub default_test_order {
   my $order_by = shift;
   is_same_sql_bind(
      $rs->search ({}, {order_by => $order_by})->as_query,
      "(SELECT
        TOP 1 me__id, source, owner, title, price, owner__id, name FROM
         (SELECT
           TOP 4 me.id AS me__id, me.source, me.owner, me.title, me.price, owner.id AS owner__id, owner.name
           FROM books me
           JOIN owners owner ON
           owner.id = me.owner
           WHERE ( source = ? )
           ORDER BY me__id ASC
         ) me ORDER BY me__id DESC
       )",
    [ [ source => 'Library' ] ],
  );
}

sub test_order {
  my $args = shift;

  my $req_order = $args->{order_req}
    ? "ORDER BY $args->{order_req}"
    : ''
  ;

  is_same_sql_bind(
    $rs->search ({}, {order_by => $args->{order_by}})->as_query,
    "(SELECT
      me__id, source, owner, title, price, owner__id, name FROM
      (SELECT
        TOP 1 me__id, source, owner, title, price, owner__id, name FROM
         (SELECT
           TOP 4 me.id AS me__id, me.source, me.owner, me.title, me.price, owner.id AS owner__id, owner.name FROM
           books me
           JOIN owners owner ON owner.id = me.owner
           WHERE ( source = ? )
           ORDER BY $args->{order_inner}
         ) me ORDER BY $args->{order_outer}
      ) me $req_order
    )",
    [ [ source => 'Library' ] ],
  );
}

my @tests = (
  {
    order_by => \'foo DESC',
    order_req => 'foo DESC',
    order_inner => 'foo DESC',
    order_outer => 'foo ASC'
  },
  {
    order_by => { -asc => 'foo'  },
    order_req => 'foo ASC',
    order_inner => 'foo ASC',
    order_outer => 'foo DESC',
  },
  {
    order_by => 'foo',
    order_req => 'foo',
    order_inner => 'foo ASC',
    order_outer => 'foo DESC',
  },
  {
    order_by => [ qw{ foo bar}   ],
    order_req => 'foo, bar',
    order_inner => 'foo ASC, bar ASC',
    order_outer => 'foo DESC, bar DESC',
  },
  {
    order_by => { -desc => 'foo' },
    order_req => 'foo DESC',
    order_inner => 'foo DESC',
    order_outer => 'foo ASC',
  },
  {
    order_by => ['foo', { -desc => 'bar' } ],
    order_req => 'foo, bar DESC',
    order_inner => 'foo ASC, bar DESC',
    order_outer => 'foo DESC, bar ASC',
  },
  {
    order_by => { -asc => [qw{ foo bar }] },
    order_req => 'foo ASC, bar ASC',
    order_inner => 'foo ASC, bar ASC',
    order_outer => 'foo DESC, bar DESC',
  },
  {
    order_by => [
      { -asc => 'foo' },
      { -desc => [qw{bar}] },
      { -asc  => [qw{hello sensors}]},
    ],
    order_req => 'foo ASC, bar DESC, hello ASC, sensors ASC',
    order_inner => 'foo ASC, bar DESC, hello ASC, sensors ASC',
    order_outer => 'foo DESC, bar ASC, hello DESC, sensors DESC',
  },
);

my @default_tests = ( undef, '', {}, [] );

plan (tests => scalar @tests + scalar @default_tests + 1);

test_order ($_) for @tests;
default_test_order ($_) for @default_tests;


is_same_sql_bind (
  $rs->search ({}, { group_by => 'title', order_by => 'title' })->as_query,
'(SELECT
me.id, me.source, me.owner, me.title, me.price, owner.id, owner.name FROM
   ( SELECT
      id, source, owner, title, price FROM
      ( SELECT
         TOP 1 id, source, owner, title, price FROM
         ( SELECT
            TOP 4 me.id, me.source, me.owner, me.title, me.price FROM
            books me  JOIN
            owners owner ON owner.id = me.owner
            WHERE ( source = ? )
            GROUP BY title
            ORDER BY title ASC
         ) me
         ORDER BY title DESC
      ) me
      ORDER BY title
   ) me  JOIN
   owners owner ON owner.id = me.owner WHERE
   ( source = ? )
   ORDER BY title)' ,
  [ [ source => 'Library' ], [ source => 'Library' ] ],
);
