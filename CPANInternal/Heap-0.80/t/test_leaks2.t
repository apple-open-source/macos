#!/usr/bin/env perl

BEGIN {
    chdir 't' if -d 't';
    use lib '../lib';
    $| = 1;
    print "1..13\n";
}

END {print "not ok 1\n" unless $loaded;}
$loaded = 1;
print "ok 1\n";

use Heap::Fibonacci;
use Heap::Elem::Num( NumElem );

my $heapsize;
my $extractsize;
my $test = 1;

my $allocated;

sub Heap::Elem::Num::DESTROY {
    --$allocated;
}

for (
	$extractsize = 5;
	$extractsize < 20000;
	$extractsize = $heapsize) {
    $heapsize = $extractsize*5;
    $allocated = 0;

    my $heap = Heap::Fibonacci->new;

    for (1..$heapsize) {
	my $val = int(rand(1000));
	my $heapElem = NumElem( $val );
	$heap->add($heapElem);
	++$allocated;
    }

    print( (($allocated == $heapsize) ? "" : "not "),
	    "ok ",
	    ++$test,
	    "\n" );

    for (1..$extractsize){ 
	my $elem = $heap->extract_top;
    }
	
    undef $heap;

    print( (($allocated == 0) ? "" : "not "),
	    "ok ",
	    ++$test,
	    "\n" );

}
