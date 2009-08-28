#!/usr/bin/perl

use lib "..";
use HeaderDoc::PerfEngine;

my $global_perf = HeaderDoc::PerfEngine->new;

print STDERR "Testing Perf engine.\n";
foo();
baz();
incomplete();

$global_perf->printstats();

sub foo()
{
	my $i = 0;
	while ($i< 100) {
		$global_perf->checkpoint(1);
		open(OUTPUT, ">/tmp/bar10000");
		print OUTPUT "bar\n";
		close(OUTPUT);
		$i++;
		$global_perf->checkpoint(0);
	}
}

sub baz()
{
	my $i = 0;
	$global_perf->checkpoint(1);


	while ($i< 100) {
		foo();
		$i++;
	}


	$global_perf->checkpoint(0);
}

sub incomplete()
{
    $global_perf->checkpoint(1);
}

