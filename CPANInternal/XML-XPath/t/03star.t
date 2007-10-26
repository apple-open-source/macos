use Test;
BEGIN { plan tests => 5 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;

@nodes = $xp->findnodes('/AAA/CCC/DDD/*');
ok(@nodes, 4);

@nodes = $xp->findnodes('/*/*/*/BBB');
ok(@nodes, 5);

@nodes = $xp->findnodes('//*');
ok(@nodes, 17);

__DATA__
<AAA>
<XXX><DDD><BBB/><BBB/><EEE/><FFF/></DDD></XXX>
<CCC><DDD><BBB/><BBB/><EEE/><FFF/></DDD></CCC>
<CCC><BBB><BBB><BBB/></BBB></BBB></CCC>
</AAA>
