#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests exercise the use of Sub::Exporter via its setup_exporter routine.

They use Test::SubExporter::s_e, bundled in ./t/lib, which uses this calling
style.

=cut

use Test::More tests => 48;

BEGIN { use_ok('Sub::Exporter'); }

our $exporting_class = 'Test::SubExporter::s_e';

use lib 't/lib';

for my $iteration (1..2) {
  {
    package Test::SubExporter::BUILT;

    my $import = Sub::Exporter::build_exporter({ exports => [ 'X' ] });

    Sub::Exporter::setup_exporter({
      exports => [ 'X' ],
      into    => 'Test::SubExporter::VIOLATED' . "_$iteration",
      as      => 'gimme_X_from',
    });

    sub X { return "expected" }

    package Test::SubExporter::BUILT::CONSUMER;

    $import->('Test::SubExporter::BUILT', ':all');
    main::is(X(), "expected", "manually constructed importer worked");

    eval <<END_TEST;
    package Test::SubExporter::VIOLATED_$iteration;

    gimme_X_from('Test::SubExporter::BUILT', ':all');
    main::is(X(), "expected", "manually constructed importer worked");
END_TEST
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
  main::use_ok($exporting_class, ':sailor');
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
  main::use_ok(
    $exporting_class,
    hello_sailor => { game => 'zork3' },
    hi_sailor    => undef,
  );
  use subs qw(hello_sailor hi_sailor);

  main::is(
    hello_sailor,
    "Something happens!",
    "Z3: custom hello_sailor works as expected"
  );

  main::is(
    hi_sailor,
    "Nothing happens yet.",
    "Z3: hi_sailor, using symbolic import and no args, works as expected"
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

  package Test::SubExporter::Z3_REF;

  my $hello;
  main::use_ok(
    $exporting_class,
    hello_sailor => { game => 'zork3', -as => \$hello }
  );

  eval "hello_sailor;";
  main::like(
    $@,
    qr/Bareword "hello_sailor" not allowed/,
    "Z3_REF: hello_sailor isn't actually imported to package"
  );

  main::is(
    $hello->(),
    "Something happens!",
    "Z3_REF: hello_sailor properly exported to scalar ref",
  );

  package Test::SubExporter::Z3_BADREF;

  main::require_ok($exporting_class);

  eval {
    Test::SubExporter::s_e->import(hello_sailor => { game => 'zork3', -as => {} });
  };

  main::like(
    $@,
    qr/invalid reference type/,
    "can't pass a non-scalar ref to -as",
  );
}

sub install_upstream {
  Sub::Exporter::setup_exporter({
    exports    => [ 'X' ],
    as         => 'gimme_X_from',
    into_level => 1,
  });
}

package Test::SubExporter::LEVEL_1;

sub X { return 1 };

main::install_upstream;

package Test::SubExporter::CALLS_LEVEL_1;

Test::SubExporter::LEVEL_1->gimme_X_from(X => { -as => 'x_from_1' });
use subs 'x_from_1';

main::is(x_from_1(), 1, "imported from uplevel-installed exporter");
