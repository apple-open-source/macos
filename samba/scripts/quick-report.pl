#! /usr/bin/perl

# $Id$ 

# Take a logfile that resulted from running "run-samba.sh QUICK" and filter it
# down to something that's useable as a basic reporting format.

use strict;

my %results;
my $debug = 0;

sub test_cmp
{
    $results{$b}{status} cmp $results{$a}{status}
}

sub search_for_test
{
    my $name;

    while (my $line = <>) {
	chomp $line;
	if (($name) = ($line =~ m/Running smbtorture test ([\w-]+)/)) {
	    print "found test $name\n" if ($debug);
	    return $name;
	}
    }

    return
}

sub process_test
{
    my $lines = [];
    my $status = 'unknown';

    while (my $line = <>) {
	my ($num, $fail, $err, $skip);
	chomp $line;

	push @{$lines}, $line;
	($num, $fail, $err, $skip) =
	    ($line =~ /Tests: (\d+), Failures: (\d+), Errors: (\d+), Skipped: (\d+)/);

	if (defined($num) and 
	    defined($fail) and
	    defined($err) and
	    defined($skip)) {

	    if (($skip + $err + $fail) == 0) {
		$status = 'passed';
	    } elsif ($fail != 0) {
		$status = 'failed';
	    } elsif ($err != 0) {
		$status = 'error';
	    } elsif ($skip != 0) {
		$status = 'skipped';
	    }

	    last;
	}
    }

    return ($status, $lines);
}

while (1) {
    my ($testname, $lines, $status);

    $testname = search_for_test();
    last unless $testname;

    ($status, $lines) = process_test();

    $results{$testname}{status} = $status;
    $results{$testname}{lines} = $lines;
}

my %summary;

# first pass - print summary
foreach my $test (sort (keys %results)) {
    printf("%-20.20s %s\n", $test, $results{$test}{status});
    if (!defined($summary{$results{$test}{status}})) {
	$summary{$results{$test}{status}} = 1;
    } else {
	$summary{$results{$test}{status}}++;
    }
}

my $str = join ', ', (map { $_ . ": " . $summary{$_} }  (keys %summary));
print("\n\n$str\n\n");

# second pass - print failure info
foreach my $test (sort (keys %results)) {
    if ($results{$test}{status} ne 'passed') {
	if (scalar(@{$results{$test}{lines}})) {
	    print "    +----\n";
	    foreach my $line (@{$results{$test}{lines}}) {
		print "    |$line\n";
	    }
	    print "    +----\n";
	} else {
	    print "    no output\n";
	}

    }
}

1;
