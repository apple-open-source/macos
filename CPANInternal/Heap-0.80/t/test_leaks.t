# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

BEGIN {
    chdir 't' if -d 't';
    use lib '../lib';
    $| = 1;
    print "1..7\n"; 
}
use Heap;
use Heap::Elem::NumRev;

my @test_seq =
	(
	    [ test_empty => ],
	    [ add    => 1, 100 ],
	    [ test   => 100 ],
	    [ remove => 50, 100, 51 ],
	    [ test   => 50 ],
	    [ remove => 50, 50, 1 ],
	    [ test_empty => ],
	    [ repeat => 0, 2 ],
	    [ mem_test => ],
	    [ repeat => 1, 50 ],
	    [ last => ],
	);
my $test_index = 0;
my @repeat_count = ( 0, 0, 0, 0 );

my $heap = new Heap::Fibonacci;
my $test_num = 0;
my $still_testing = 1;
my $not;

while (1) {
    my $step = $test_seq[$test_index++];
    my $op = $step->[0];
    my $scratch;
    $not = 'not ';
    if( $op eq 'test_empty' ) {
	defined($heap->top) or $not = '';
    } elsif( $op eq 'test' ) {
	defined($scratch = $heap->top) and $scratch->val == $step->[1] and $not = '';
    } elsif( $op eq 'add' ) {
	my( $base, $limit, $incr ) = (@$step)[1..3];
	defined $incr or $incr = 1;
	while(1) {
	    my $elem = new Heap::Elem::NumRev($base);
	    $heap->add( $elem );
	    last if $base == $limit;
	    $base += $incr;
	}
	$not = 'skip';
    } elsif( $op eq 'remove' ) {
	my( $count, $base, $limit, $incr ) = (@$step)[1..4];
	defined $incr or $incr = -1;
	$not = '';
	while($count--) {
	    my $elem = $heap->extract_top;
	    defined($elem) && $elem->val == $base
		or $not = 'not ';
	    $base += $incr;
	}
	$not = 'not '
	    if $base != $limit + $incr;
    } elsif( $op eq 'repeat' ) {
	my( $index, $limit ) = (@$step)[1..2];
	if( $still_testing ) {
	    $still_testing = 0;
	}
	if( ++$repeat_count[$index] == $limit ) {
	    $repeat_count[$index] = 0;
	} else {
	    $test_index = 0;
	}
	$not = '';
    } elsif( $op eq 'mem_test' ) {
	$not = '';
	print `ps -lp$$`;
    } elsif( $op eq 'last' ) {
	$not = '';
	last;
    }
    if( $not ne 'skip' ) {
	if( $still_testing ) {
	    ++$test_num;
	    print $not, "ok $test_num\n";
	} else {
	    last if $not;
	}
    }
}

++$test_num;
print $not, "ok $test_num\n";
