# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

my $fibi;
my $biny;
my $binl;
my $b1;

BEGIN {
    chdir 't' if -d 't';
    use lib '../lib';
    $| = 1;
    my $arg = $ENV{HEAPTESTARG};
    my $types;
    $b1 = 50;
    # env var $HEAPTESTARG can change the test set
    # It can contain chars i y l to select fibonaccI binarY or binomiaL.
    # It can contain a number to control the (number of items heaped)/4
    # default is iyl50 (test all three, 200 numbers on heap).
    # All comments below use the 50/200 default, other sizes are
    # for debug purposes.
    if( defined $arg ) {
	$fibi = $biny = $binl = 0;
	++$fibi  if $arg =~ /i/;
	++$biny  if $arg =~ /y/;
	++$binl  if $arg =~ /l/;
	$b1 = $1 if $arg =~ /([\d]+)/;
    } else {
	$fibi = 1;
	$biny = 1;
	$binl = 1;
    }
    print "1..", ($b1*2*8+4)*($fibi+$biny+$binl)+1, "\n";
}
END {print "not ok 1\n" unless $loaded;}
use Heap;
$loaded = 1;
print "ok 1\n";

my $b2 = $b1*2;
my $b3 = $b1*3;
my $b4 = $b1*4;

my $b0p1 = 1;
my $b1p1 = $b1+1;
my $b2p1 = $b2+1;
my $b3p1 = $b3+1;

use Heap::Fibonacci;
use Heap::Binomial;
use Heap::Binary;

use Heap::Elem::Num( NumElem );

my $count = 1;

sub testaheap {
    my $heap = shift;
    my @elems = map { NumElem($_) } 1..($b4);
    unshift @elems, undef;	# index them 1..200, not 0..199

    # add block4, block3, block2, block1 to mix the order a bit
    foreach( ($b3p1)..($b4),
	     ($b2p1)..($b3),
	     ($b1p1)..($b2),
	     ($b0p1)..($b1) ) {
	$heap->add( $elems[$_] );
    }

    sub testit {
	print( ($_[0] ? "ok " : "not ok "), $_[1], "\n" );
    }

    # test 2..801
    # We should find 1..100 in order on the heap, each element
    # should have its heap value defined while it is still in
    # the heap, and then undef after it is removed.
    # Meanwhile, after removing element i (in 1..100) we then
    # remove element i+100 out of order using delete, to test
    # that the heap doesn't get corrupted.
    # (i.e. 1, 101, 2, 102, ..., 100, 200)
    foreach my $index ( 1..$b2 ) {
	my $el;
	$el = $heap->top;
	testit( $index == $el->val, ++$count );
	testit( defined($el->heap), ++$count );
	$el = $heap->extract_top;
	testit( $index == $el->val, ++$count );
	testit( ! defined($el->heap), ++$count );
	$el = $elems[$index+$b2];
	testit( $index+$b2 == $el->val, ++$count );
	testit( defined($el->heap), ++$count );
	$heap->delete( $el );
	testit( $index+$b2 == $el->val, ++$count );
	testit( ! defined($el->heap), ++$count );
    }

    # test 802..805 - heap should be empty, and return undef
    testit( ! defined($heap->top), ++$count );
    testit( ! defined($heap->extract_top), ++$count );
    testit( ! defined($heap->top), ++$count );
    testit( ! defined($heap->extract_top), ++$count );
}

$fibi && testaheap( Heap::Fibonacci->new );
$binl && testaheap( Heap::Binomial->new );
$biny && testaheap( Heap::Binary->new );
