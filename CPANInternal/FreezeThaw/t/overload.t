print "1.." . &last() . "\n";
use Carp;
$SIG{__WARN__} = sub { warn Carp::longmess(@_) };
use FreezeThaw qw(freeze thaw);

{
  package Overloaded;
  use overload '""' => sub { shift()->[0] };
  sub new { my $p = shift; bless [shift], $p }
}

my $a = new Overloaded 'xyz';
my $f = freeze $a;
print "# '$f'\n";
my ($o) = thaw $f;

print "not " unless "$o" eq 'xyz';
print "ok 1\n";

print "not " unless ref $o eq 'Overloaded';
print "ok 2\n";

$f = freeze [$a, $a];
print "# '$f'\n";
($o) = thaw $f;

print "# '$o->[0]'\nnot " unless "$o->[0]" eq 'xyz';
print "ok 3\n";

print "not " unless $o->[0][0] eq 'xyz';
print "ok 4\n";

print "not " unless ref $o->[0] eq 'Overloaded';
print "ok 5\n";

print "not " unless "$o->[1]" eq 'xyz';
print "ok 6\n";

print "not " unless $o->[1][0] eq 'xyz';
print "ok 7\n";

print "not " unless ref $o->[1] eq 'Overloaded';
print "ok 8\n";

print "not " unless @$o == 2;
print "ok 9\n";

bless $o->[0], 'Something';

print "not " unless ref $o->[0] eq 'Something';
print "ok 10\n";

# SvAMAGIC() is a property of a reference, not of a referent!
# Thus $o->[1] would preserve overloadness unless this:
bless $o->[1], ref $o->[1];

print "not " unless ref $o->[1] eq 'Something';
print "ok 11\n";

print "not " unless $o->[0] == $o->[1];		# Addresses
print "ok 12\n";

sub last {12}
