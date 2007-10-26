use Test;
BEGIN { plan tests => 4 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @en = $xp->findnodes('//*[lang("en")]');
ok(@en, 2);

my @de = $xp->findnodes('//content[lang("de")]');
ok(@de, 1);

__DATA__
<page xml:lang="en">
  <content>Here we go...</content>
  <content xml:lang="de">und hier deutschsprachiger Text :-)</content>
</page>
