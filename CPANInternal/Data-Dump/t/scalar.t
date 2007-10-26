print "1..3\n";

use Data::Dump qw(dump);

$a = 42;
@a = (\$a);

$d = dump($a, $a, \$a, \\$a, "$a", $a+0, \@a);

print "$d;\n";

print "not " unless $d eq q(do {
  my $a = 42;
  ($a, $a, \\$a, \\\\$a, 42, 42, [\\$a]);
});
print "ok 1\n";

$d = dump(\\$a, \$a, $a, \@a);
print "$d;\n";

print "not " unless $d eq q(do {
  my $a = \\\\42;
  ($a, $$a, $$$a, [$$a]);
});
print "ok 2\n";

# not really a scalar test, but anyway
$a = [];
$d = dump(\$a, $a);

print "$d;\n";
print "not " unless $d eq q(do {
  my $a = \[];
  ($a, $$a);
});
print "ok 3\n";
