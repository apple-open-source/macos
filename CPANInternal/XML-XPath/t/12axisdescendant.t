use Test;
BEGIN { plan tests => 6 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('/descendant::*');
ok(@nodes, 11);

@nodes = $xp->findnodes('/AAA/BBB/descendant::*');
ok(@nodes, 4);

@nodes = $xp->findnodes('//CCC/descendant::*');
ok(@nodes, 6);

@nodes = $xp->findnodes('//CCC/descendant::DDD');
ok(@nodes, 3);

__DATA__
<AAA>
<BBB><DDD><CCC><DDD/><EEE/></CCC></DDD></BBB>
<CCC><DDD><EEE><DDD><FFF/></DDD></EEE></DDD></CCC>
</AAA>
