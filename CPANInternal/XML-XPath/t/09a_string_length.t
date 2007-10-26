use Test;
BEGIN { plan tests => 5 }

use XML::XPath;

my $doc_one = qq|<doc><para>para one</para></doc>|;

my $xp = XML::XPath->new(xml => $doc_one);
ok($xp);

my $doc_one_chars = $xp->find('string-length(/doc/text())');
ok($doc_one_chars == 0, 1);

my $doc_two = qq|
<doc>
  <para>para one has <b>bold</b> text</para>
</doc>
|;

$xp = undef;

$xp = XML::XPath->new(xml => $doc_two);
ok($xp);

my $doc_two_chars = $xp->find('string-length(/doc/text())');
ok($doc_two_chars == 3, 1);

my $doc_two_para_chars = $xp->find('string-length(/doc/para/text())');
ok($doc_two_para_chars == 13, 1);

