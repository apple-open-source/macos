# This example is from the Perl::Tidy man page
use Perl::Tidy;

# some messy source code to format
my $source = <<'EOM';
use strict;
my @editors=('Emacs', 'Vi   '); my $rand = rand();
print "A poll of 10 random programmers gave these results:\n";
foreach(0..10) {
my $i=int ($rand+rand());
print " $editors[$i] users are from Venus" . ", " . 
"$editors[1-$i] users are from Mars" . 
"\n";
}
EOM

# We'll pass it as ref to SCALAR and receive it in a ref to ARRAY
my @dest;
perltidy( source => \$source, destination => \@dest );
foreach (@dest) {print}
