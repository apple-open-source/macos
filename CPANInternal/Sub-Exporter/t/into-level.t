#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests exercise the "into" and "into_level" special arguments to the built
exporter.

=cut

use Test::More tests => 14;

BEGIN { 
  use_ok('Sub::Exporter'); 
}

BEGIN {
  package Test::SubExport::FROM;
  use strict;
  use warnings;    
  use Sub::Exporter -setup => {
    exports => [ qw(A B) ],
    groups  => {
      default => [ ':all' ],
      a       => [ 'A'    ],
      b       => [ 'B'    ]
    }
  };

  sub A { 'A' }
  sub B { 'B' }

  1;    
}

BEGIN {
  package Test::SubExport::HAS_DEFAULT_INTO_LEVEL;
  use strict;
  use warnings;    
  use Sub::Exporter -setup => {
    exports    => [ qw(C) ],
    into_level => 1,
  };

  sub C { 'C' }

  1;    
}

BEGIN {
  package Test::SubExport::HAS_DEFAULT_INTO;
  use strict;
  use warnings;

  use Sub::Exporter -setup => {
    exports => [ qw(foo) ],
    into    => 'Test::SubExport::DEFAULT_INTO',
  };

  sub foo { 'foo' }

  1;
}

BEGIN {
  package Test::SubExport::INTO;
  use strict;
  use warnings;

  sub import {
    my $package = shift;
    my $caller  = caller(0);
    Test::SubExport::FROM->import( { into => $caller }, @_ );
  }

  1;
}

BEGIN {
  package Test::SubExport::LEVEL;
  use strict;
  use warnings;

  sub import {
    my $package = shift;
    Test::SubExport::FROM->import( { into_level => 1 }, @_ );
  }

  1;
}

BEGIN {
  package Test::SubExport::DEFAULT_LEVEL;
  use strict;
  use warnings;

  sub import {
    my $package = shift;
    Test::SubExport::HAS_DEFAULT_INTO_LEVEL->import(@_);
  }

  1;
}

package Test::SubExport::INTO::A;
Test::SubExport::INTO->import('A');

main::can_ok(__PACKAGE__, 'A' );
main::cmp_ok(
  __PACKAGE__->can('A'), '==', Test::SubExport::FROM->can('A'),
  'sub A was exported'
);

package Test::SubExport::INTO::ALL;
Test::SubExport::INTO->import(':all');

main::can_ok(__PACKAGE__, 'A', 'B' );

main::cmp_ok(
  __PACKAGE__->can('A'), '==', Test::SubExport::FROM->can('A'),
  'sub A was exported'
);

main::cmp_ok(
  __PACKAGE__->can('B'), '==', Test::SubExport::FROM->can('B'),
  'sub B was exported'
);

package Test::SubExport::LEVEL::ALL;
Test::SubExport::LEVEL->import(':all');

main::can_ok(__PACKAGE__, 'A', 'B' );

main::cmp_ok(
  __PACKAGE__->can('A'), '==', Test::SubExport::FROM->can('A'),
  'sub A was exported'
);

main::cmp_ok(
  __PACKAGE__->can('B'), '==', Test::SubExport::FROM->can('B'),
  'sub B was exported'
);

package Test::SubExport::LEVEL::DEFAULT;
Test::SubExport::DEFAULT_LEVEL->import(':all');

main::can_ok(__PACKAGE__, 'C');

main::cmp_ok(
  __PACKAGE__->can('C'),
  '==',
  Test::SubExport::HAS_DEFAULT_INTO_LEVEL->can('C'),

  'sub C was exported'
);

package Test::SubExport::NON_DEFAULT_INTO;

main::is(
  Test::SubExport::DEFAULT_INTO->can('foo'),
  undef,
  "before import, 'default into' target can't foo",
);

Test::SubExport::HAS_DEFAULT_INTO->import('-all');

main::is(
  __PACKAGE__->can('foo'),
  undef,
  "after import, calling package can't foo",
);

main::is(
  Test::SubExport::DEFAULT_INTO->can('foo'),
  \&Test::SubExport::HAS_DEFAULT_INTO::foo,
  "after import, calling package can't foo",
);
