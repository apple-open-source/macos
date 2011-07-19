#!/usr/bin/perl -I.

# try to honor possible tempdirs
$tmp = "file_$$";

$short = <<END;
small
file
END

$long = <<END;
This is a much longer bit of contents
to store in a file.
END

print "1..7\n";

use File::Slurp;

&write_file($tmp, $long);
if (&read_file($tmp) eq $long) {print "ok 1\n";} else {print "not ok 1\n";}

@x = &read_file($tmp);
@y = grep( $_ ne '', split(/(.*?\n)/, $long));
while (@x && @y) {
	last unless $x[0] eq $y[0];
	shift @x;
	shift @y;
}
if (@x == @y && (@x ? $x[0] eq $y[0] : 1)) { print "ok 2\n";} else {print "not ok 2\n"}

&append_file($tmp, $short);
if (&read_file($tmp) eq "$long$short") {print "ok 3\n";} else {print "not ok 3\n";}

$iold = (stat($tmp))[1];
&overwrite_file($tmp, $short);
$inew = (stat($tmp))[1];

if (&read_file($tmp) eq $short) {print "ok 4\n";} else {print "not ok 4\n";}

if ($inew == $iold) {print "ok 5\n";} else {print "not ok 5\n";}

unlink($tmp);

&overwrite_file($tmp, $long);
if (&read_file($tmp) eq $long) {print "ok 6\n";} else {print "not ok 6\n";}

unlink($tmp);

&append_file($tmp, $short);
if (&read_file($tmp) eq $short) {print "ok 7\n";} else {print "not ok 7\n";}

unlink($tmp);


