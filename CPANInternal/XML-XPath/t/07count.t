use Test;
BEGIN { plan tests => 7 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('//*[count(BBB) = 2]');
ok($nodes[0]->getName, "DDD");

@nodes = $xp->findnodes('//*[count(*) = 2]');
ok(@nodes, 2);

@nodes = $xp->findnodes('//*[count(*) = 3]');
ok(@nodes, 2);
ok($nodes[0]->getName, "AAA");
ok($nodes[1]->getName, "CCC");

__DATA__
<AAA>
<CCC><BBB/><BBB/><BBB/></CCC>
<DDD><BBB/><BBB/></DDD>
<EEE><CCC/><DDD/></EEE>
</AAA>
