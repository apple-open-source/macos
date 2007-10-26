use Test;
BEGIN { plan tests => 6 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @ids = $xp->findnodes('//BBB[@id]');
ok(@ids, 2);

my @names = $xp->findnodes('//BBB[@name]');
ok(@names, 1);

my @attribs = $xp->findnodes('//BBB[@*]');
ok(@attribs, 3);

my @noattribs = $xp->findnodes('//BBB[not(@*)]');
ok(@noattribs, 1);

__DATA__
<AAA>
<BBB id='b1'/>
<BBB id='b2'/>
<BBB name='bbb'/>
<BBB/>
</AAA>
