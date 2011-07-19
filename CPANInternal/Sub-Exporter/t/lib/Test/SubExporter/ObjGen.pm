#!perl
package Test::SubExporter::ObjGen::Obj;

use strict;
use warnings;

sub new {
  my $class = shift;
  my $code  = $class->can(shift);

  bless { code => $code } => $class;
}

sub group {
  return {
    foo => sub { return 'FOO' },
    bar => sub { return 'BAR' },
  };
}

sub baz {
  return sub {
    return 'BAZ';
  };
}

use overload
  '&{}'  => sub { $_[0]->{code} },
  'bool' => sub { 1 };

package Test::SubExporter::ObjGen;

my ($group_o, $group_b, $baz, $quux);
BEGIN {
  $quux  = sub { sub { 'QUUX' } };
  bless $quux => 'Test::SubExporter::Whatever';

  $group_o = sub { return {
    ringo   => sub { 'starr' },
    richard => sub { 'starkey' },
  } };
  bless $group_o => 'Test::SubExporter::Whatever';

  $baz     = Test::SubExporter::ObjGen::Obj->new('baz');
  $group_b = Test::SubExporter::ObjGen::Obj->new('group');
}

use Sub::Exporter -setup => {
  exports => { baz  => $baz, quux => $quux },
  groups  => { meta => $group_b, ringo => $group_o },
};
  

"call me";
