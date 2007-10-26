use Test;
BEGIN { plan tests => 5 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('/AAA/BBB/DDD/CCC/EEE/ancestor::*');
ok(@nodes, 4);
ok($nodes[1]->getName, "BBB"); # test document order

@nodes = $xp->findnodes('//FFF/ancestor::*');
ok(@nodes, 5);

__DATA__
<AAA>
<BBB><DDD><CCC><DDD/><EEE/></CCC></DDD></BBB>
<CCC><DDD><EEE><DDD><FFF/></DDD></EEE></DDD></CCC>
</AAA>
