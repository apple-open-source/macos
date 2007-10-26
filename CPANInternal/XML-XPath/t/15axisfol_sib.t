use Test;
BEGIN { plan tests => 6 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('/AAA/BBB/following-sibling::*');
ok(@nodes, 2);
ok($nodes[1]->getName, "CCC"); # test document order

@nodes = $xp->findnodes('//CCC/following-sibling::*');
ok(@nodes, 3);
ok($nodes[1]->getName, "FFF");

__DATA__
<AAA>
<BBB><CCC/><DDD/></BBB>
<XXX><DDD><EEE/><DDD/><CCC/><FFF/><FFF><GGG/></FFF></DDD></XXX>
<CCC><DDD/></CCC>
</AAA>
