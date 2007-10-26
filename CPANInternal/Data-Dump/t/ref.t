#!perl -w

use strict;
use Test qw(plan ok);

plan tests => 2;

use Data::Dump qw(dump);

my $s = \\1;
ok(nl(dump($s)), <<'EOT');
\\1
EOT

my %s;
$s{C1} = \$s{C2};
$s{C2} = \$s{C1};
ok(nl(dump(\%s)), <<'EOT');
do {
  my $a = { C1 => \\do{my $fix}, C2 => 'fix' };
  ${${$a->{C1}}} = $a->{C1};
  $a->{C2} = ${$a->{C1}};
  $a;
}
EOT

sub nl { shift(@_) . "\n" }
