#!perl -T
use strict;
use warnings;

use Test::More tests => 11;
BEGIN { use_ok("Sub::Exporter"); }

use lib 't/lib';
use Test::SubExporter::Faux;

my ($generator, $installer, $reset, $exports);
BEGIN { ($generator, $installer, $reset, $exports) = faux_installer; }

my %generator;
BEGIN {
  %generator = (
    foo   => sub { sub { 1 } },
    bar   => sub { sub { 2 } },
    baz   => sub { sub { 3 } },
    BAR   => sub { sub { 4 } },
    xyzzy => sub { sub { 5 } },
  );
}

  BEGIN {
    isa_ok($installer, 'CODE');

    package Thing;
    BEGIN { main::use_ok('Sub::Exporter::Util', 'like'); }
    use Sub::Exporter -setup => {
      installer  => $installer,
      generator  => $generator,
      collectors => {
        -like => like
      },
      exports => \%generator,
    };
  }

package main;

my $code = sub {
  $reset->();
  Thing->import(@_);
};

$code->(qw(foo xyzzy));
exports_ok(
  $exports,
  [ [ foo => {} ], [ xyzzy => {} ] ],
  "the basics work normally"
);

$code->(-like => qr/^b/i);
exports_ok(
  $exports,
  [ [ BAR => {} ], [ baz => {} ], [ bar => {} ] ],
  "give me everything starting with b or B (qr//)"
);

$code->(-like => [ qr/^b/i ]);
exports_ok(
  $exports,
  [ [ BAR => {} ], [ baz => {} ], [ bar => {} ] ],
  "give me everything starting with b or B ([qr//])"
);

$code->(-like => [ qr/^b/i => undef ]);
exports_ok(
  $exports,
  [ [ BAR => {} ], [ baz => {} ], [ bar => {} ] ],
  "give me everything starting with b or B ([qr//=>undef])"
);

# XXX: must use verbose exporter
my %col = ( -like => [
  qr/^b/i => { -prefix => 'like_' },
  qr/zz/i => { -suffix => '_y2'   },
]);

$code->(%col);

everything_ok(
  $exports,
  [
    [
      BAR => {
        class      => 'Thing',
        generator  => $generator{BAR},
        name       => 'BAR',
        arg        => {},
        collection => \%col,
        as         => 'like_BAR',
        into       => 'main',
      },
    ],
    [
      bar => {
        class      => 'Thing',
        generator  => $generator{bar},
        name       => 'bar',
        arg        => {},
        collection => \%col,
        as         => 'like_bar',
        into       => 'main',
      },
    ],
    [
      baz => {
        class      => 'Thing',
        generator  => $generator{baz},
        name       => 'baz',
        arg        => {},
        collection => \%col,
        as         => 'like_baz',
        into       => 'main',
      },
    ],
    [
      xyzzy => {
        class      => 'Thing',
        generator  => $generator{xyzzy},
        name       => 'xyzzy',
        arg        => {},
        collection => \%col,
        as         => 'xyzzy_y2',
        into       => 'main',
      },
    ],
  ],
  'give me everything starting with b or B as like_$_ ([qr//=>{...}])'
);

{
  my $like = Sub::Exporter::Util::like();
  is(ref($like), 'CODE', 'like() gives us a generator');

  eval { $like->() };
  like($@, qr/no regex supplied/, "exception with no args to like->()");

  eval { $like->([ "fake*reg{3}exp" => { a => 1 } ]) };
  like($@, qr/not a regex/i, "exception with non qr// pattern in like");
}
