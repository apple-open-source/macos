package Variable::Magic::TestScopeEnd;

use Test::More;

use Variable::Magic qw/wizard cast/;

my $wiz;

BEGIN {
 $wiz = wizard
  data => sub { $_[1] },
  free => sub { $_[1]->(); () };
}

sub hook (&) {
 $^H |= 0x020000;
 cast %^H, $wiz, shift;
}

BEGIN {
 hook { pass 'in hints hash destructor' };
 die 'turnip';
}

1;
