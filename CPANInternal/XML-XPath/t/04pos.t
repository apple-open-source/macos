use Test;
BEGIN { plan tests => 4 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my $first = $xp->findvalue('/AAA/BBB[1]/@id');
ok($first, "first");

my $last = $xp->findvalue('/AAA/BBB[last()]/@id');
ok($last, "last");

__DATA__
<AAA>
<BBB id="first"/>
<BBB/>
<BBB/>
<BBB id="last"/>
</AAA>
