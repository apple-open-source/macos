use Test;
BEGIN { plan tests => 5 }

use XML::XPath;

ok(1);
my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @root = $xp->findnodes('/AAA');
ok(@root, 1);

my @ccc = $xp->findnodes('/AAA/CCC');
ok(@ccc, 3);

my @bbb = $xp->findnodes('/AAA/DDD/BBB');
ok(@bbb, 2);

__DATA__
<AAA>
    <BBB/>
    <CCC/>
    <BBB/>
    <CCC/>
    <BBB/>
    <!-- comment -->
    <DDD>
        <BBB/>
        Text
        <BBB/>
    </DDD>
    <CCC/>
</AAA>
