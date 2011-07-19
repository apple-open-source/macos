#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests check export group expansion, specifically the expansion of groups
that use group generators.

=cut

# XXX: The framework is stolen from expand-group.  I guess it should be
# factored out.  Whatever. -- rjbs, 2006-03-12

use Test::More tests => 12;

BEGIN { use_ok('Sub::Exporter'); }

my $alfa  = sub { 'alfa'  };
my $bravo = sub { 'bravo' };

my $returner = sub {
  my ($class, $group, $arg, $collection) = @_;

  my %given = (
    class => $class,
    group => $group,
    arg   => $arg,
    collection => $collection,
  );

  return {
    foo => sub { return { name => 'foo', %given }; },
    bar => sub { return { name => 'bar', %given }; },
  };
};

my $config = {
  exports => [ ],
  groups  => {
    alphabet  => sub { { A => $alfa, b => $bravo } },
    broken    => sub { [ qw(this is broken because it is not a hashref) ] },
    generated => $returner,
    nested    => [qw( :generated )],
  },
  collectors => [ 'col1' ],
};

my @single_tests = (
  # [ comment, \@group, \@output ]
  # [ "simple group 1", [ ':A' => undef ] => [ [ a => undef ] ] ],
  [
    "simple group generator",
    [ -alphabet => undef ],
    [ [ A => $alfa ], [ b => $bravo ] ],
  ],
  [
    "simple group generator with prefix",
    [ -alphabet => { -prefix => 'prefix_' } ],
    [ [ prefix_A => $alfa ], [ prefix_b => $bravo ] ],
  ],
);

for my $test (@single_tests) {
  my ($label, $given, $expected) = @$test;
  
  my @got = Sub::Exporter::_expand_group(
    'Class',
    $config,
    $given,
    {},
  );

  is_deeply(
    [ sort { lc $a->[0] cmp lc $b->[0] } @got ],
    $expected,
    "expand_group: $label",
  );
}

for my $test (@single_tests) {
  my ($label, $given, $expected) = @$test;
  
  my $got = Sub::Exporter::_expand_groups(
    'Class',
    $config,
    [ $given ],
  );

  is_deeply(
    [ sort { lc $a->[0] cmp lc $b->[0] } @$got ],
    $expected,
    "expand_groups: $label [single test]",
  );
}

my @multi_tests = (
  # [ $comment, \@groups, \@output ]
);

for my $test (@multi_tests) {
  my ($label, $given, $expected) = @$test;
  
  my $got = Sub::Exporter::_expand_groups(
    'Class',
    $config,
    $given,
  );

  is_deeply($got, $expected, "expand_groups: $label");
}

##

eval {
  Sub::Exporter::_expand_groups('Class', $config, [[ -broken => undef ]])
};

like($@,
  qr/did not return a hash/,
  "exception on non-hashref groupgen return",
);

##

{
  my $got = Sub::Exporter::_expand_groups(
    'Class',
    $config,
    [ [ -alphabet => undef ] ],
    {},
  );

  my %code = map { $_->[0] => $_->[1] } @$got;

  my $a = $code{A};
  my $b = $code{b};

  is($a->(), 'alfa',  "generated 'a' sub does what we think");
  is($b->(), 'bravo', "generated 'b' sub does what we think");
}

{
  my $got = Sub::Exporter::_expand_groups(
    'Class',
    $config,
    [ [ -generated => { xyz => 1 } ] ],
    { col1 => { value => 2 } },
  );

  my %code = map { $_->[0] => $_->[1] } @$got;

  for (qw(foo bar)) {
    is_deeply(
      $code{$_}->(),
      {
        name  => $_,
        class => 'Class',
        group => 'generated',
        arg   => { xyz => 1 }, 
        collection => { col1 => { value => 2 } },
      },
      "generated foo does what we expect",
    );
  }
}

{
  my $got = Sub::Exporter::_expand_groups(
    'Class',
    $config,
    [ [ -nested => { xyz => 1 } ] ],
    { col1 => { value => 2 } },
  );

  my %code = map { $_->[0] => $_->[1] } @$got;

  for (qw(foo bar)) {
    is_deeply(
      $code{$_}->(),
      {
        name  => $_,
        class => 'Class',
        group => 'generated',
        arg   => { xyz => 1 }, 
        collection => { col1 => { value => 2 } },
      },
      "generated foo (via nested group) does what we expect",
    );
  }
}
