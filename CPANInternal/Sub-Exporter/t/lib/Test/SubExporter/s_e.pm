#!perl
package Test::SubExporter::s_e;

use strict;
use warnings;

use Sub::Exporter;

Sub::Exporter::setup_exporter({
  exports => {
    xyzzy        => undef,
    hello_sailor => \&_hs_gen,
    hi_sailor    => \"_hs_gen",
  },
  groups => {
    default => [ qw(xyzzy hello_sailor) ],
    sailor  => [
      xyzzy => undef,
      hello_sailor => { -as => 'hs_works', game => 'zork3' },
      hello_sailor => { -as => 'hs_fails', game => 'zork1' },
    ]
  },
  collectors => [ 'defaults' ],
});

sub xyzzy { return "Nothing happens." };

sub _hs_gen {
  my ($class, $name, $arg, $collection) = @_;

  if (($arg->{game}||'') eq 'zork3') {
    return sub { return "Something happens!" };
  } else {
    return sub { return "Nothing happens yet." };
  }
}

"y2";
