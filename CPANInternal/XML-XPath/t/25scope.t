use Test;
BEGIN { plan tests => 4 }

use XML::XPath;
ok(1);

eval
{
  # Removing the 'my' makes this work?!?
  my $xp = XML::XPath->new(xml => '<test/>');
  ok($xp);

  $xp->findnodes('/test');

  ok(1);

  die "This should be caught\n";

};

if ($@)
{
  ok(1);
}
else {
    ok(0);
}
