use Test;
BEGIN { plan tests => 7 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('/AAA/XXX/preceding-sibling::*');
ok(@nodes, 1);
ok($nodes[0]->getName, "BBB");

@nodes = $xp->findnodes('//CCC/preceding-sibling::*');
ok(@nodes, 4);

@nodes = $xp->findnodes('/AAA/CCC/preceding-sibling::*[1]');
ok($nodes[0]->getName, "XXX");

@nodes = $xp->findnodes('/AAA/CCC/preceding-sibling::*[2]');
ok($nodes[0]->getName, "BBB");

__DATA__
<AAA>
    <BBB>
        <CCC/>
        <DDD/>
    </BBB>
    <XXX>
        <DDD>
            <EEE/>
            <DDD/>
            <CCC/>
            <FFF/>
            <FFF>
                <GGG/>
            </FFF>
        </DDD>
    </XXX>
    <CCC>
        <DDD/>
    </CCC>
</AAA>
