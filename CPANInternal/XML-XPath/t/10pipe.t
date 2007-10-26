use Test;
BEGIN { plan tests => 6, todo => [] }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('//CCC | //BBB');
ok(@nodes, 3);
ok($nodes[0]->getName, "BBB"); # test document order

@nodes = $xp->findnodes('/AAA/EEE | //BBB');
ok(@nodes, 2);

@nodes = $xp->findnodes('/AAA/EEE | //DDD/CCC | /AAA | //BBB');
ok(@nodes, 4);

__DATA__
<AAA>
<BBB/>
<CCC/>
<DDD><CCC/></DDD>
<EEE/>
</AAA>
