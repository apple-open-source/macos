print "1..10\n";

$str = "abcd";

#$IO::String::DEBUG++;

use IO::String;
$io = IO::String->new($str);

sub all_pos
{
   my($io, $expect) = @_;
   $io->getpos == $expect &&
   $io->pos    == $expect &&
   $io->tell   == $expect &&
   $io->seek(0, 1) == $expect &&
   $io->sysseek(0, 1) == $expect &&
   $] >= 5.006 ? ( tell($io) == $expect &&
      	           seek($io, 0, 1) == $expect &&
                   sysseek($io, 0, 1) == $expect
                 )
               : 1;
}

print "not " unless all_pos($io, 0);
print "ok 1\n";

$io->setpos(2);
print "not " unless all_pos($io, 2);
print "ok 2\n";

$io->setpos(10);  # XXX should it be defined in terms of seek??
print "not " unless all_pos($io, 4);
print "ok 3\n";

$io->seek(10, 0);
print "not " unless all_pos($io, 10);
print "ok 4\n";

$io->print("זרו");
print "not " unless all_pos($io, 13);
print "ok 5\n";

$io->seek(-4, 2);
print "not " unless all_pos($io, 9);
print "ok 6\n";

print "not " unless $io->read($buf, 20) == 4 && $buf eq "\0זרו";
print "ok 7\n";

print "not " unless $io->seek(-10,1) && all_pos($io, 3);
print "ok 8\n";

$io->seek(0,0);
print "not " unless all_pos($io, 0);
print "ok 9\n";

if ($] >= 5.006) {
   seek($io, 1, 0);
   print "not " unless all_pos($io, 1);
}
print "ok 10\n";

