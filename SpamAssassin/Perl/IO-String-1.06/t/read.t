print "1..13\n";

$str = <<EOT;
This is an example
of a paragraph

and a single line.

EOT

use IO::String 0.01;
$io = IO::String->new($str);

@lines = <$io>;
print "not " unless @lines == 5 && $lines[1] eq "of a paragraph\n" && $. == 5;
print "ok 1\n";

use vars qw(@tmp);

print "not " if defined($io->getline)  ||
                (@tmp = $io->getlines) ||
                defined(<$io>)         ||
                defined($io->getc)     ||
                read($io, $buf, 100)   != 0 ||
	        $io->getpos != length($str);
print "ok 2\n";


{
    local $/;  # slurp mode
    $io->setpos(0);
    @lines = $io->getlines;
    print "not " unless @lines == 1 && $lines[0] eq $str;
    print "ok 3\n";

    $io->setpos(index($str, "and"));
    $line = <$io>;
    print "not " unless $line eq "and a single line.\n\n";
    print "ok 4\n";
}

{
    local $/ = "";  # paragraph mode
    $io->setpos(0);
    @lines = <$io>;
    print "not " unless @lines == 2 && $lines[1] eq "and a single line.\n\n";
    print "ok 5\n";
}

{
    local $/ = "is";
    $io->setpos(0);
    @lines = ();
    my $no = $io->input_line_number;
    my $err;
    while (<$io>) {
	push(@lines, $_);
	$err++ if $. != ++$no;
    }

    print "not " if $err;
    print "ok 6\n";

    print "not " unless @lines == 3 && join("-", @lines) eq
                                       "This- is- an example\n" .
                                       "of a paragraph\n\n" .
                                       "and a single line.\n\n";
    print "ok 7\n";
}


# Test read

$io->setpos(0);

print "not " unless read($io, $buf, 3) == 3 && $buf eq "Thi";
print "ok 8\n";

print "not " unless sysread($io, $buf, 3, 2) == 3 && $buf eq "Ths i";
print "ok 9\n";

$io->seek(-4, 2);

print "not " if $io->eof;
print "ok 10\n";

print "not " unless read($io, $buf, 20) == 4 && $buf eq "e.\n\n";
print "ok 11\n";

print "not " unless read($io, $buf, 20) == 0 && $buf eq "";
print "ok 12\n";

print "not " unless $io->eof;
print "ok 13\n";


