#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests check the output of build_installer when handed an alternate
installer that returns its plan.

=cut

use Test::More tests => 11;

BEGIN { use_ok('Sub::Exporter'); }

use lib 't/lib';
use Test::SubExporter::Faux;

my $config = {
  exports => [
    qw(circsaw drill handsaw nailgun),
    hammer => sub { sub { print "BANG BANG BANG\n" } },
  ],
  groups => {
    default => [
      'handsaw',
      'hammer'  => { claw => 1 },
    ],
    cutters => [ qw(circsaw handsaw), circsaw => { -as => 'buzzsaw' } ],
  },
  collectors => [
    'defaults',
    'brand_preference' => sub { 0 },
  ]
};

{
  my ($generator, $installer, $reset, $exports) = faux_installer;
  my $code = sub {
    $reset->();
    splice @_, 1, 0, { generator => $generator, installer => $installer };
    Sub::Exporter::build_exporter($config)->(@_);
  };

  $code->('Tools::Power');
  exports_ok(
    $exports,
    [ [ handsaw => {} ], [ hammer => { claw => 1 } ] ],
    "exporting with no arguments gave us default group"
  );

  $code->('Tools::Power', ':all');
  exports_ok(
    [ sort { $a->[0] cmp $b->[0] } @$exports ],
    [ map { [ $_ => {} ] } sort qw(circsaw drill handsaw nailgun hammer), ],
    "exporting :all gave us all exports",
  );

  $code->('Tools::Power', drill => { -as => 'auger' });
  exports_ok(
    $exports,
    [ [ drill => {} ] ],
    "'-as' parameter is not passed to generators",
  );

  $code->('Tools::Power', ':cutters');
  exports_ok(
    $exports,
    [ [ circsaw => {} ], [ handsaw => {} ], [ circsaw => {} ] ], 
    "group with two export instances of one export",
  );

  eval { $code->('Tools::Power', 'router') };
  like($@, qr/not exported/, "can't export un-exported export (got that?)");

  eval { $code->('Tools::Power', ':sockets') };
  like($@, qr/not exported/, "can't export nonexistent group, either");

  # because the brand_preference validator always fails, this should die
  eval { $code->('Tools::Power', brand_preference => [ '...' ]) };
  like(
    $@,
    qr/brand_preference failed validation/,
    "collector validator prevents bad export"
  );
}

{
  my ($generator, $installer, $reset, $exports) = faux_installer;
  my $code = sub {
    $reset->();
    splice @_, 1, 0, { generator => $generator, installer => $installer };
    Sub::Exporter::build_exporter({ exports => [ 'foo' ] })->(@_);
  };

  $code->('Example::Foo');
  exports_ok(
    $exports,
    [ ],
    "exporting with no arguments gave us default default group, i.e., nothing"
  );

  $code->('Tools::Power', ':all');
  exports_ok(
    $exports,
    [ [ foo => {} ] ],
    "exporting :all gave us all exports, i.e., foo",
  );
}

{
  package Test::SubExport::FAUX;
  my ($generator, $installer, $reset, $exports) = main::faux_installer;

  Sub::Exporter::setup_exporter({
    exports   => [ 'X' ],
    installer => $installer,
    generator => $generator,
  });
  __PACKAGE__->import(':all');

  main::exports_ok($exports, [ [ X => {} ] ], "setup (not built) exporter");
}
