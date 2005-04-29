#!perl -w

print "1..8\n";

use URI;

$u = URI->new("<mms://66.250.188.13/KFOG_FM>");

#print "$u\n";
print "not " unless $u eq "mms://66.250.188.13/KFOG_FM";
print "ok 1\n";

print "not " unless $u->port == 1755;
print "ok 2\n";

# play with port
$old = $u->port(8755);
print "not " unless $old == 1755 && $u eq "mms://66.250.188.13:8755/KFOG_FM";
print "ok 3\n";

$u->port(1755);
print "not " unless $u eq "mms://66.250.188.13:1755/KFOG_FM";
print "ok 4\n";

$u->port("");
print "not " unless $u eq "mms://66.250.188.13:/KFOG_FM" && $u->port == 1755;
print "ok 5\n";

$u->port(undef);
print "not " unless $u eq "mms://66.250.188.13/KFOG_FM";
print "ok 6\n";

print "not " unless $u->host eq "66.250.188.13";
print "ok 7\n";

print "not " unless $u->path eq "/KFOG_FM";
print "ok 8\n";
