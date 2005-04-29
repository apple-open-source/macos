print "1..1\n";

use strict;
use HTML::Parser;

my $p = HTML::Parser->new(api_version => 3);
my @com;
$p->handler(comment => sub { push(@com, shift) }, "token0");
$p->handler(default => sub { push(@com, shift() . "[" . shift() . "]") }, "event, text");

$p->parse("<foo><!><!-><!--><!---><!----><!-----><!------>");
$p->parse("<!--+--");
$p->parse("\n\n");
$p->parse(">");
$p->parse("<!--foo--->");
$p->parse("<!--foo---->");
$p->parse("<!--foo----->-->");
$p->parse("<foo>");
$p->parse("<!3453><!-3456><!FOO>");
$p->eof;

my $com = join(":", @com);
print "$com\n";
print "not " unless $com eq "start_document[]:start[<foo>]::-:><!-::-:--:+:foo-:foo--:foo---:text[-->]:start[<foo>]:3453:-3456:FOO:end_document[]";
print "ok 1\n";
