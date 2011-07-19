#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests exercise that the polymorphic exporter-builder used when
Sub::Exporter's -import group is invoked.

They use Test::SubExporter::DashSetup, bundled in ./t/lib, which uses this
calling style.

=cut

use Test::More tests => 40;

BEGIN { use_ok('Sub::Exporter'); }

our $exporting_class = 'Test::SubExporter::DashSetup';

use lib 't/lib';

for my $iteration (1..2) {
  {
    package Test::SubExporter::SETUP;
    use Sub::Exporter -setup => [ qw(X) ];

    sub X { return "desired" }

    package Test::SubExporter::SETUP::CONSUMER;

    Test::SubExporter::SETUP->import(':all');
    main::is(X(), "desired", "constructed importer (via -setup [LIST]) worked");
  }

  {
    package Test::SubExporter::EXPORT_MISSING;
    use Sub::Exporter -setup => [ qw(X) ];

    package Test::SubExporter::SETUP::CONSUMER_OF_MISSING;

    eval { Test::SubExporter::EXPORT_MISSING->import(':all') };
    main::like(
      $@,
      qr/can't locate export/,
      "croak if we're configured to export something that can't be found",
    );
  }

  {
    package Test::SubExporter::SETUPFAILURE;
    eval { Sub::Exporter->import( -setup => sub { 1 }) };
    main::like($@, qr/-setup failed validation/, "only [],{} ok for -setup");
  }

  package Test::SubExporter::DEFAULT;
  main::use_ok($exporting_class);
  use subs qw(xyzzy hello_sailor);

  main::is(
    xyzzy,
    "Nothing happens.",
    "DEFAULT: default export xyzzy works as expected"
  );

  main::is(
    hello_sailor,
    "Nothing happens yet.",
    "DEFAULT: default export hello_sailor works as expected"
  );

  package Test::SubExporter::RENAME;
  main::use_ok($exporting_class, xyzzy => { -as => 'plugh' });
  use subs qw(plugh);

  main::is(
    plugh,
    "Nothing happens.",
    "RENAME: default export xyzzy=>plugh works as expected"
  );

  package Test::SubExporter::SAILOR;
  main::use_ok($exporting_class, ':sailor');;
  use subs qw(xyzzy hs_works hs_fails);

  main::is(
    xyzzy,
    "Nothing happens.",
    "SAILOR: default export xyzzy works as expected"
  );

  main::is(
    hs_works,
    "Something happens!",
    "SAILOR: hs_works export works as expected"
  );

  main::is(
    hs_fails,
    "Nothing happens yet.",
    "SAILOR: hs_fails export works as expected"
  );

  package Test::SubExporter::Z3;
  main::use_ok($exporting_class, hello_sailor => { game => 'zork3' });
  use subs qw(hello_sailor);

  main::is(
    hello_sailor,
    "Something happens!",
    "Z3: custom hello_sailor works as expected"
  );

  package Test::SubExporter::FROTZ_SAILOR;
  main::use_ok($exporting_class, -sailor => { -prefix => 'frotz_' });
  use subs map { "frotz_$_" }qw(xyzzy hs_works hs_fails);

  main::is(
    frotz_xyzzy,
    "Nothing happens.",
    "FROTZ_SAILOR: default export xyzzy works as expected"
  );

  main::is(
    frotz_hs_works,
    "Something happens!",
    "FROTZ_SAILOR: hs_works export works as expected"
  );

  main::is(
    frotz_hs_fails,
    "Nothing happens yet.",
    "FROTZ_SAILOR: hs_fails export works as expected"
  );
}

{
  package Test::SubExporter::SETUPALT;
  use Sub::Exporter -setup => {
    -as      => 'alternimport',
    exports => [ qw(Y) ],
  };

  sub X { return "desired" }
  sub Y { return "other" }

  package Test::SubExporter::SETUP::ALTCONSUMER;

  Test::SubExporter::SETUPALT->import(':all');
  eval { X() };
  main::like($@, qr/undefined subroutine/i, "X didn't get imported");

  eval { Y() };
  main::like($@, qr/undefined subroutine/i, "Y didn't get imported");

  Test::SubExporter::SETUPALT->alternimport(':all');
  main::is(Y(), "other", "other importer (via -setup { -as ...}) worked");
}
