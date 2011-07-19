#!perl -T
use strict;
use warnings;

use Test::More tests => 8;
BEGIN { use_ok("Sub::Exporter"); }

  BEGIN {
    package Thing;
    BEGIN { main::use_ok("Sub::Exporter::Util", 'merge_col'); }

    use Sub::Exporter -setup => {
      collectors => [ qw(defaults etc) ],
      exports    => {
        merge_col(
          defaults => {
            stack => sub { my @x = @_; sub { return @x } },
            kcats => \'_kcats_gen',
          },
          empty    => {
            bogus => sub { my @x = @_; sub { return @x } },
            klame => sub { my @x = @_; sub { return @x } },
          },
          etc      => {
            other => sub { my @x = @_; sub { return @x } },
          },
        ),
        plain => sub { my @x = @_; sub { return @x } },
      },
    };

    sub _kcats_gen {
      my @x = @_;
      sub { return reverse @x }
    }
  }

package Test::SubExporter::MERGE::0;

my %col;

BEGIN {
  Thing->import(
    defaults => ($col{defaults} = { x => 10 }),
    etc      => ($col{etc}      = { home => "Kansas" }),
    stack    => { x => 20, y => 30 },
    kcats    => {          y =>  3 },
    bogus    => undef,
    klame    => { bar => 99 },
    other    => undef,
    plain    => { foo => 10 },
  );
}

my %tests = (
  stack => [ 'Thing', 'stack', { x => 20, y => 30 }, \%col ],
  kcats => [ \%col, { x => 10, y =>  3 }, 'kcats', 'Thing' ],
  bogus => [ 'Thing', 'bogus', {}, \%col ],
  klame => [ 'Thing', 'klame', { bar => 99 }, \%col ],
  other => [ 'Thing', 'other', { home => "Kansas" }, \%col ],
  plain => [ 'Thing', 'plain', { foo  => 10 }, \%col ],
);

while (my ($name, $expected) = each %tests) {
  main::is_deeply(
    [ __PACKAGE__->$name ],
    $expected,
    "$name returned proper value",
  );
}
