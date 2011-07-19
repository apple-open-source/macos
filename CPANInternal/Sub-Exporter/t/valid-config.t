#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests make sure that invalid configurations passed to
setup/build_exporter throw exceptions.

=cut

use Test::More tests => 6;

BEGIN { use_ok('Sub::Exporter'); }

eval {
  Sub::Exporter::build_exporter({
    exports    => [ qw(foo) ],
    collectors => [ qw(foo) ],
  })
};

like($@, qr/used in both/, "can't use one name in exports and collectors");

eval {
  Sub::Exporter::build_exporter({
    collections => [ qw(foo) ], # This one gets me all the time.  Live & learn.
  })
};

like($@, qr/unknown options/, "unknown options raise an exception");

eval {
  Sub::Exporter::setup_exporter({
    into       => 'Your::Face',
    into_level => 5,
  })
};

like(
  $@,
  qr/may not both/,
  "into and into_level are mutually exclusive (in setup_exporter)"
);

eval { 
  Sub::Exporter::build_exporter({})->(
    Class => {
      into       => 'Your::Face',
      into_level => 1
    }
  );
};

like(
  $@,
  qr/may not both/,
  "into and into_level are mutually exclusive (in exporter)"
);

eval {
  Sub::Exporter::build_exporter({
    into       => "This::Doesnt::Matter",
    into_level => 0,
  })
};

like(
  $@,
  qr(^into and into_level may not both be supplied to exporter),
  "can't use one name in exports and collectors"
);

