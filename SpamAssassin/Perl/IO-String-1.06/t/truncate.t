print "1..6\n";

use IO::String;

$io = IO::String->new($str);

$io->truncate(10);
print "not " unless length($str) == 10;
print "ok 1\n";

print "not " unless $io->getpos == 0;
print "ok 2\n";

$io->setpos(8);
$io->truncate(2);
print "not " unless length($str) == 2 && $io->getpos == 2;
print "ok 3\n";

undef($io);
$str = "";

$io = IO::String->new($str);
$io->pad("+");

$io->truncate(5);

$n = read($io, $buf, 20);
print "not " unless $n == 5 && $buf eq "+++++" && $buf eq $str;
print "ok 4\n";

print "not " unless read($io, $buf, 20) == 0;
print "ok 5\n";

$io->truncate(0);
print "not " unless $str eq "";
print "ok 6\n";


