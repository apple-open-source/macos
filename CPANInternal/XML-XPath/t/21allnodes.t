use Test;
BEGIN { plan tests => 11 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('//GGG/ancestor::*');
ok(@nodes, 4);

@nodes = $xp->findnodes('//GGG/descendant::*');
ok(@nodes, 3);

@nodes = $xp->findnodes('//GGG/following::*');
ok(@nodes, 3);
ok($nodes[0]->getName, "VVV");

@nodes = $xp->findnodes('//GGG/preceding::*');
ok(@nodes, 5);
ok($nodes[0]->getName, "BBB"); # document order, not HHH

@nodes = $xp->findnodes('//GGG/self::*');
ok(@nodes, 1);
ok($nodes[0]->getName, "GGG");

@nodes = $xp->findnodes('//GGG/ancestor::* | 
        //GGG/descendant::* | 
        //GGG/following::* |
        //GGG/preceding::* |
        //GGG/self::*');
ok(@nodes, 16);

__DATA__
<AAA>
    <BBB>
        <CCC/>
        <ZZZ/>
    </BBB>
    <XXX>
        <DDD>
            <EEE/>
            <FFF>
                <HHH/>
                <GGG> <!-- Watch this node -->
                    <JJJ>
                        <QQQ/>
                    </JJJ>
                    <JJJ/>
                </GGG>
                <VVV/>
            </FFF>
        </DDD>
    </XXX>
    <CCC>
        <DDD/>
    </CCC>
</AAA>
