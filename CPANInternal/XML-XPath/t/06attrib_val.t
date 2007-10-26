use Test;
BEGIN { plan tests => 5 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('//BBB[@id = "b1"]');
ok(@nodes, 1);

@nodes = $xp->findnodes('//BBB[@name = "bbb"]');
ok(@nodes, 1);

@nodes = $xp->findnodes('//BBB[normalize-space(@name) = "bbb"]');
ok(@nodes, 2);

__DATA__
<AAA>
<BBB id='b1'/>
<BBB name=' bbb '/>
<BBB name='bbb'/>
</AAA>
