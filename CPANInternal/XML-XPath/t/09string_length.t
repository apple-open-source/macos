use Test;
BEGIN { plan tests => 5 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('//*[string-length(name()) = 3]');
ok(@nodes, 2);

@nodes = $xp->findnodes('//*[string-length(name()) < 3]');
ok(@nodes, 2);

@nodes = $xp->findnodes('//*[string-length(name()) > 3]');
ok(@nodes, 3);

__DATA__
<AAA>
<Q/>
<SSSS/>
<BB/>
<CCC/>
<DDDDDDDD/>
<EEEE/>
</AAA>
