use Test;
BEGIN { plan tests => 4 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @bbb = $xp->findnodes('//BBB');
ok(@bbb, 5);

my @subbbb = $xp->findnodes('//DDD/BBB');
ok(@subbbb, 3);

__DATA__
<AAA>
<BBB/>
<CCC/>
<BBB/>
<DDD><BBB/></DDD>
<CCC><DDD><BBB/><BBB/></DDD></CCC>
</AAA>
