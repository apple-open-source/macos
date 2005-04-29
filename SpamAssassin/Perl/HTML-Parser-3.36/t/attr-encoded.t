use strict;
print "1..2\n";

use HTML::Parser ();
my $p = HTML::Parser->new();
$p->attr_encoded(1);

my $text = "";
$p->handler(start =>
	    sub {
		 my($tag, $attr) = @_;
		 $text .= "S[$tag";
		 for my $k (sort keys %$attr) {
		     my $v =  $attr->{$k};
		     $text .= " $k=$v";
		 }
		 $text .= "]";
	     }, "tagname,attr");

my $html = <<'EOT';
<tag arg="&amp;&lt;&gt">
EOT

$p->parse($html)->eof;

print "not " unless $text eq 'S[tag arg=&amp;&lt;&gt]';  print "ok 1\n";

$text = "";
$p->attr_encoded(0);
$p->parse($html)->eof;

print "not " unless $text eq 'S[tag arg=&<>]';  print "ok 2\n";
