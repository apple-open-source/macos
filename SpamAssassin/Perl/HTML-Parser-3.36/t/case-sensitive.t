use strict;
print "1..8\n";

use HTML::Parser ();
my $p = HTML::Parser->new();
$p->case_sensitive(1);

my $text = "";
$p->handler(start =>
	    sub {
		 my($tag, $attr, $attrseq) = @_;
		 $text .= "S[$tag";
		 for my $k (sort keys %$attr) {
		     my $v =  $attr->{$k};
		     $text .= " $k=$v";
		 }
		 if (@$attrseq) { $text.=" Order:" ; }
		 for my $k (@$attrseq) {
		     $text .= " $k";
		 }
		 $text .= "]";
	     }, "tagname,attr,attrseq");
$p->handler(end =>
	    sub {
		 my ($tag) = @_;
		 $text .= "E[$tag]";
	     }, "tagname");

my $html = <<'EOT';
<tAg aRg="Value" arg="other value"></tAg>
EOT
my $cs = 'S[tAg aRg=Value arg=other value Order: aRg arg]E[tAg]';
my $ci = 'S[tag arg=Value Order: arg arg]E[tag]';

$p->parse($html)->eof;
print "not " unless $text eq $cs;  print "ok 1\n";

$text = "";
$p->case_sensitive(0);
$p->parse($html)->eof;
print "not " unless $text eq $ci;  print "ok 2\n";

$text = "";
$p->case_sensitive(1);
$p->xml_mode(1);
$p->parse($html)->eof;
print "not " unless $text eq $cs;  print "ok 3\n";

$text = "";
$p->case_sensitive(0);
$p->parse($html)->eof;
print "not " unless $text eq $cs;  print "ok 4\n";

$html = <<'EOT';
<tAg aRg="Value" arg="other value"></tAg>
<iGnOrE></ignore>
EOT
$p->ignore_tags('ignore');
$cs = 'S[tAg aRg=Value arg=other value Order: aRg arg]E[tAg]S[iGnOrE]';
$ci = 'S[tag arg=Value Order: arg arg]E[tag]';

$text = "";
$p->case_sensitive(0);
$p->xml_mode(0);
$p->parse($html)->eof;
print "not " unless $text eq $ci;  print "ok 5\n";
 
$text = "";
$p->case_sensitive(1);
$p->xml_mode(0);
$p->parse($html)->eof;
print "not " unless $text eq $cs;  print "ok 6\n";
 

$text = "";
$p->case_sensitive(0);
$p->xml_mode(1);
$p->parse($html)->eof;
print "not " unless $text eq $cs;  print "ok 7\n";
 
$text = "";
$p->case_sensitive(1);
$p->xml_mode(1);
$p->parse($html)->eof;
print "not " unless $text eq $cs;  print "ok 8\n";
 
