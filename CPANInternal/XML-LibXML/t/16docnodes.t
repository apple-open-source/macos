use XML::LibXML;
use Test;

# this test fails under XML-LibXML-1.00 with a segfault after the
# second parsing.  it was fixed by putting in code in getChildNodes
# to handle the special case where the node was the document node

BEGIN { plan tests => 9 }

  my $input = <<EOD;
<doc>
   <clean>   </clean>
   <dirty>   A   B   </dirty>
   <mixed>
      A
      <clean>   </clean>
      B
      <dirty>   A   B   </dirty>
      C
   </mixed>
</doc>
EOD

for (0 .. 2) {
  my $parser = XML::LibXML->new();
  my $doc = $parser->parse_string($input);
  my @a = $doc->getChildnodes;
  ok(scalar(@a),1);
}

my $parser = XML::LibXML->new();
my $doc = $parser->parse_string($input);
for (0 .. 2) {
  my $a = $doc->getFirstChild;
  ok(ref($a),'XML::LibXML::Element');
}

for (0 .. 2) {
  my $a = $doc->getLastChild;
  ok(ref($a),'XML::LibXML::Element');
}

