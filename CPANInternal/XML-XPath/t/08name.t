use Test;
BEGIN { plan tests => 5 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('//*[name() = "BBB"]');
ok(@nodes, 5);

@nodes = $xp->findnodes('//*[starts-with(name(), "B")]');
ok(@nodes, 7);

@nodes = $xp->findnodes('//*[contains(name(), "C")]');
ok(@nodes, 3);

__DATA__
<AAA>
<BCC><BBB/><BBB/><BBB/></BCC>
<DDB><BBB/><BBB/></DDB>
<BEC><CCC/><DBD/></BEC>
</AAA>
