use Test;
BEGIN { plan tests => 4 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('/AAA/XXX/descendant-or-self::*');
ok(@nodes, 8);

@nodes = $xp->findnodes('//CCC/descendant-or-self::*');
ok(@nodes, 4);

__DATA__
<AAA>
<BBB><CCC/><ZZZ><DDD/></ZZZ></BBB>
<XXX><DDD><EEE/><DDD/><CCC/><FFF/><FFF><GGG/></FFF></DDD></XXX>
<CCC><DDD/></CCC>
</AAA>
