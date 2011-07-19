use strict;
use warnings;

use Test::Tester;
use Test::More;

BEGIN
{
  require Scalar::Util;
  if (grep /^weaken$/, @Scalar::Util::EXPORT_FAIL)
  {
    plan(skip_all => "no weak refs in this version of perl");
    exit(0);
  }
}

use t::std;

use Scalar::Util qw( isweak weaken);

sub left
{
  my $ref = shift;
  eq_deeply($ref, []);
  return "left";
}

sub right
{
  my $ref = shift;
  eq_deeply([], $ref);
  return "right";
}

my @subs = (\&left, \&right);
for my $sub (@subs)
{
  my $ref = [];

  my $weak = $ref;
  weaken($weak);
  my $side = &$sub($ref, []);
  $ref = 1;
  ok((! $weak), "$side didn't capture") || diag "weak = $weak";
}
