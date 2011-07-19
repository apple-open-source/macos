#!perl
package Test::SubExporter::GroupGen;

use strict;
use warnings;

use Sub::Exporter;

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

sub gen_group_by_name {
  my ($class, $group, $arg, $collection) = @_;

  my %given = (
    class => $class,
    group => $group,
    arg   => $arg,
    collection => $collection,
  );

  return {
    baz => sub { return { name => 'baz', %given }; },
  };
}

my $config = {
  exports => [ ],
  groups  => {
    alphabet  => sub { { a => $alfa, b => $bravo } },
    generated => $returner,
    # symbolic  => \&gen_group_by_name,
    # symbolic  => sub { shift->gen_group_by_name(@_) },
    symbolic  => \'gen_group_by_name',
  },
  collectors => [ 'col1' ],
};

Sub::Exporter::setup_exporter($config);

"gg";
