BEGIN { $| = 1; print "1..3\n"; }
END {print "not ok 1\n" unless $loaded;}
use Convert::UUlib ':all';
$loaded = 1;
print "ok 1\n";

SetFNameFilter(sub { $_[0]+1 });
print FNameFilter(5) == 6 ? "" : "not ","ok 2\n";
SetFNameFilter();
print FNameFilter(5) == 5 ? "" : "not ","ok 3\n";

