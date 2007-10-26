use Test;
BEGIN { plan tests => 5 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('//BBB[position() mod 2 = 0 ]');
ok(@nodes, 4);

@nodes = $xp->findnodes('//BBB
        [ position() = floor(last() div 2 + 0.5) 
            or
          position() = ceiling(last() div 2 + 0.5) ]');

ok(@nodes, 2);

@nodes = $xp->findnodes('//CCC
        [ position() = floor(last() div 2 + 0.5) 
            or
          position() = ceiling(last() div 2 + 0.5) ]');

ok(@nodes, 1);

__DATA__
<AAA>
    <BBB/>
    <BBB/>
    <BBB/>
    <BBB/>
    <BBB/>
    <BBB/>
    <BBB/>
    <BBB/>
    <CCC/>
    <CCC/>
    <CCC/>
</AAA>
