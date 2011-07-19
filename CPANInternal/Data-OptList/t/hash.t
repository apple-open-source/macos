#!/usr/bin/perl
use strict;
use warnings;

=head1 TEST PURPOSE

These tests test option list expansion (from an option list into a hashref).

=cut

use Sub::Install;
use Test::More tests => 13;

BEGIN { use_ok('Data::OptList'); }

# let's get a convenient copy to use:
Sub::Install::install_sub({
  code => 'mkopt_hash',
  from => 'Data::OptList',
  as   => 'OPTH',
});

is_deeply(
  OPTH(),
  {},
  "empty opt list expands properly",
);

is_deeply(
  OPTH(undef),
  {},
  "undef opt list expands properly",
);

is_deeply(
  OPTH([]),
  {},
  "empty arrayref opt list expands properly",
);

is_deeply(
  OPTH({}),
  {},
  "empty hashref opt list expands properly",
);

is_deeply(
  OPTH([ qw(foo bar baz) ]),
  { foo => undef, bar => undef, baz => undef },
  "opt list of just names expands",
);

is_deeply(
  OPTH([ qw(foo :bar baz) ]),
  { foo => undef, ':bar' => undef, baz => undef },
  "opt list of names expands with :group names",
);

is_deeply(
  OPTH([ foo => { a => 1 }, ':bar', 'baz' ]),
  { foo => { a => 1 }, ':bar' => undef, baz => undef },
  "opt list of names and values expands",
);

is_deeply(
  OPTH([ foo => { a => 1 }, ':bar' => undef, 'baz' ]),
  { foo => { a => 1 }, ':bar' => undef, baz => undef },
  "opt list of names and values expands, ignoring undef",
);

is_deeply(
  OPTH({ foo => { a => 1 }, -bar => undef, baz => undef }, 0, 'HASH'),
  { foo => { a => 1 }, -bar => undef, baz => undef },
  "opt list of names and values expands with must_be",
);

is_deeply(
  OPTH({ foo => { a => 1 }, -bar => undef, baz => undef }, 0, ['HASH']),
  { foo => { a => 1 }, -bar => undef, baz => undef },
  "opt list of names and values expands with [must_be]",
);

eval { OPTH({ foo => { a => 1 }, -bar => undef, baz => undef }, 0, 'ARRAY'); };
like($@, qr/HASH-ref values are not/, "exception tossed on invaild ref value");

eval { OPTH({ foo => { a => 1 }, -bar => undef, baz => undef }, 0, ['ARRAY']); };
like($@, qr/HASH-ref values are not/, "exception tossed on invaild ref value");
