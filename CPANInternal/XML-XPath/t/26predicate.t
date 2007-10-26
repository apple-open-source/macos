use Test;
BEGIN { plan tests => 4 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @bbb = $xp->findnodes('//a/b[2]');
ok(@bbb, 2);

@bbb = $xp->findnodes('(//a/b)[2]');
ok(@bbb, 1);

__DATA__
<xml>
    <a>
        <b>some 1</b>
        <b>value 1</b>
    </a>
    <a>
        <b>some 2</b>
        <b>value 2</b>
    </a>
</xml>
