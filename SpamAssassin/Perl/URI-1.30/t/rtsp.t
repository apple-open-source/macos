#!perl -w

print "1..9\n";

use URI;

$u = URI->new("<rtsp://media.perl.com/fôo.smi/>");

#print "$u\n";
print "not " unless $u eq "rtsp://media.perl.com/f%F4o.smi/";
print "ok 1\n";

print "not " unless $u->port == 554;
print "ok 2\n";

# play with port
$old = $u->port(8554);
print "not " unless $old == 554 && $u eq "rtsp://media.perl.com:8554/f%F4o.smi/";
print "ok 3\n";

$u->port(554);
print "not " unless $u eq "rtsp://media.perl.com:554/f%F4o.smi/";
print "ok 4\n";

$u->port("");
print "not " unless $u eq "rtsp://media.perl.com:/f%F4o.smi/" && $u->port == 554;
print "ok 5\n";

$u->port(undef);
print "not " unless $u eq "rtsp://media.perl.com/f%F4o.smi/";
print "ok 6\n";

print "not " unless $u->host eq "media.perl.com";
print "ok 7\n";

print "not " unless $u->path eq "/f%F4o.smi/";
print "ok 8\n";

$u->scheme("rtspu");
print "not " unless $u->scheme eq "rtspu";
print "ok 9\n";

