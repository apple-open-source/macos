use Test;
BEGIN { plan tests => 5 }

use XML::XPath;
ok(1);

my $xp = XML::XPath->new(ioref => *DATA);
ok($xp);

my @nodes;
@nodes = $xp->findnodes('//Footnote');
ok(@nodes, 1);

my $footnote = $nodes[0];

@nodes = $footnote->findnodes('ancestor::*');
ok(@nodes, 3);

@nodes = $footnote->findnodes('ancestor::text:footnote');
ok(@nodes, 1);

__DATA__
<foo xmlns:text="http://example.com/text">
<text:footnote text:id="ftn2">
<text:footnote-citation>2</text:footnote-citation>
<text:footnote-body>
<Footnote style="font-size: 10pt; margin-left: 0.499cm;
margin-right: 0cm; text-indent: -0.499cm; font-family: ; ">AxKit
is very flexible in how it lets you transform the XML on the
server, and there are many modules you can plug in to AxKit to
allow you to do these transformations. For this reason, the AxKit
installation does not mandate any particular modules to use,
instead it will simply suggest modules that might help when you
install AxKit.</Footnote>
</text:footnote-body>
</text:footnote>
</foo>
