print "1..3\n";

use Data::Dump qw(dump); 

$a = 42;
bless \$a, "Foo";

my $d = dump($a);

print "$d\n";
print "not " unless $d eq q(do {
  my $a = 42;
  bless \$a, "Foo";
  $a;
});
print "ok 1\n";

$d = dump(\$a);
print "$d\n";
print "not " unless $d eq q(bless(do{\\(my $o = 42)}, "Foo"));
print "ok 2\n";

$d = dump(\\$a);
print "$d\n";
print "not " unless $d eq q(\\bless(do{\\(my $o = 42)}, "Foo"));
print "ok 3\n";
