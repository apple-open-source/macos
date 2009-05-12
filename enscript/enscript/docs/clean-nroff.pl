#!/usr/local/bin/perl -w

sub skip_lines {
    ($count) = @_;

    while ($count > 0) {
	if (!<>) {
	    last;
	}
	$count--;
   }
    if ($count > 0) {
	return 0;
    }
    return 1;
}

sub print_lines {
    ($count) = @_;

    while ($count > 0)  {
	$line = <>;
	if (!$line) {
	    last;
	}
	print $line;
	$count--;
    }

    if ($count > 0) {
	return 0;
    }
    return 1;
}

# Header.
print_lines(7);

# Process file.
while (1) {
    if (!print_lines(52) || !skip_lines(14)) {
	last;
    }
}
