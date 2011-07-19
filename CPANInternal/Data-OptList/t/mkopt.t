#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests test option list cannonization (from an option list into a aref).

=cut

use Sub::Install;
use Test::More tests => 19;

BEGIN { use_ok('Data::OptList'); }

# let's get a convenient copy to use:
Sub::Install::install_sub({
  code => 'mkopt',
  from => 'Data::OptList',
});

sub OPT {
  # specifying moniker is tedious (also, these tests predate them)
  splice @_, 1, 0, 'test' if @_ > 1;
  mkopt(@_);
}

is_deeply(
  OPT([]),
  [],
  "empty opt list expands properly",
);

is_deeply(
  OPT(),
  [],
  "undef expands into []",
);

is_deeply(
  OPT([ qw(foo bar baz) ]),
  [ [ foo => undef ], [ bar => undef ], [ baz => undef ] ],
  "opt list of just names expands",
);

{
  my $options = OPT({ foo => undef, bar => 10, baz => [] });
     $options = [ sort { $a->[0] cmp $b->[0] } @$options ];

  is_deeply(
    $options,
    [ [ bar => undef ], [ baz => [] ], [ foo => undef ] ],
    "hash opt list expands properly"
  );
}

is_deeply(
  OPT([ qw(foo bar baz) ], 0, "ARRAY"),
  [ [ foo => undef ], [ bar => undef ], [ baz => undef ] ],
  "opt list of just names expands with must_be",
);

is_deeply(
  OPT([ qw(foo :bar baz) ]),
  [ [ foo => undef ], [ ':bar' => undef ], [ baz => undef ] ],
  "opt list of names expands with :group names",
);

is_deeply(
  OPT([ foo => { a => 1 }, ':bar', 'baz' ]),
  [ [ foo => { a => 1 } ], [ ':bar' => undef ], [ baz => undef ] ],
  "opt list of names and values expands",
);

is_deeply(
  OPT([ foo => { a => 1 }, ':bar', 'baz' ], 0, 'HASH'),
  [ [ foo => { a => 1 } ], [ ':bar' => undef ], [ baz => undef ] ],
  "opt list of names and values expands with must_be",
);

is_deeply(
  OPT([ foo => { a => 1 }, ':bar', 'baz' ], 0, ['HASH']),
  [ [ foo => { a => 1 } ], [ ':bar' => undef ], [ baz => undef ] ],
  "opt list of names and values expands with [must_be]",
);

{
  bless((my $object = {}), 'Test::DOL::Obj');
  is_deeply(
    OPT([ foo => $object, ':bar', 'baz' ], 0, 'Test::DOL::Obj'),
    [ [ foo => $object ], [ ':bar' => undef ], [ baz => undef ] ],
    "opt list of names and values expands with must_be, must_be object",
  );

  is_deeply(
    OPT([ foo => $object, ':bar', 'baz' ], 0, ['Test::DOL::Obj']),
    [ [ foo => $object ], [ ':bar' => undef ], [ baz => undef ] ],
    "opt list of names and values expands with [must_be], must_be object",
  );
}

eval { OPT([ foo => { a => 1 }, ':bar', 'baz' ], 0, 'ARRAY'); };
like($@, qr/HASH-ref values are not/, "exception tossed on invaild ref value");

eval { OPT([ foo => { a => 1 }, ':bar', 'baz' ], 0, ['ARRAY']); };
like($@, qr/HASH-ref values are not/, "exception tossed on invaild ref value");

eval { OPT([ foo => { a => 1 }, ':bar', 'baz' ], 0, 'Test::DOL::Obj'); };
like($@, qr/HASH-ref values are not/, "exception tossed on invaild ref value");

eval { OPT([ foo => { a => 1 }, ':bar', 'baz' ], 0, ['Test::DOL::Obj']); };
like($@, qr/HASH-ref values are not/, "exception tossed on invaild ref value");

is_deeply(
  OPT([ foo => { a => 1 }, ':bar' => undef, 'baz' ]),
  [ [ foo => { a => 1 } ], [ ':bar' => undef ], [ baz => undef ] ],
  "opt list of names and values expands, ignoring undef",
);

eval { OPT([ foo => { a => 1 }, ':bar' => undef, ':bar' ], 1); };
like($@, qr/multiple definitions/, "require_unique constraint catches repeat");

is_deeply(
  OPT([ foo => { a => 1 }, ':bar' => undef, 'baz' ], 1),
  [ [ foo => { a => 1 } ], [ ':bar' => undef ], [ baz => undef ] ],
  "previously tested expansion OK with require_unique",
);
