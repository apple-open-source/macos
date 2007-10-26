# $Id: stress.t,v 1.3 2000/04/17 17:08:58 matt Exp $

print "1..7\n";
my $x; $x++;
use XML::XPath;
use XML::XPath::Parser;

my $xp = XML::XPath->new( filename => 'examples/test.xml' );

print "ok $x\n" if $xp;
print "not ok $x\n" unless $xp;
$x++;
		
my $pp = XML::XPath::Parser->new();

print "ok $x\n" if $pp;
print "not ok $x\n" unless $pp;
$x++;

# test path parse time		
for (1..5000) {
	$pp->parse('//project/wednesday');
}

print "ok $x\n" if $pp;
print "not ok $x\n" unless $pp;
$x++;

my $parser = XML::XPath::XMLParser->new(
		filename => 'examples/test.xml'
		);

print "ok $x\n" if $parser;
print "not ok $x\n" unless $parser;
$x++;

my $root = $parser->parse;

print "ok $x\n" if $root;
print "not ok $x\n" unless $root;
$x++;

# test evaluation time
my $path = $pp->parse('/timesheet/projects/project/wednesday');

print "ok $x\n" if $path;
print "not ok $x\n" unless $path;
$x++;

for (1..1000) {
	$path->evaluate($root);
}
		
print "ok $x\n";
$x++;


