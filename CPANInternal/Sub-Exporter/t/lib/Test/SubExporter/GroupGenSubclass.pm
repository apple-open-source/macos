use strict;
use warnings;

package Test::SubExporter::GroupGenSubclass;
use base qw(Test::SubExporter::GroupGen);

sub gen_group_by_name {
  my ($class, $group, $arg, $collection) = @_;

  my %given = (
    class => $class,
    group => $group,
    arg   => $arg,
    collection => $collection,
  );

  return {
    baz => sub { return { name => 'baz-sc', %given }; },
  };
}

"power overwhelming";
