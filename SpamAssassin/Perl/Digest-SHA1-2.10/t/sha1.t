print "1..13\n";

use Digest::SHA1 qw(sha1 sha1_hex sha1_base64 sha1_transform);

print "not " unless Digest::SHA1->new->add("abc")->hexdigest eq "a9993e364706816aba3e25717850c26c9cd0d89d";
print "ok 1\n";

print "not " unless sha1("abc") eq pack("H*", "a9993e364706816aba3e25717850c26c9cd0d89d");
print "ok 2\n";

print "not " unless sha1_hex("abc") eq "a9993e364706816aba3e25717850c26c9cd0d89d";
print "ok 3\n";

print "not " unless sha1_base64("abc") eq "qZk+NkcGgWq6PiVxeFDCbJzQ2J0";
print "ok 4\n";

# Test file checking from too...
open(FILE, ">stest$$.txt") || die;
binmode(FILE);
for (1..512) {
    print FILE "This is line $_\n";
}
close(FILE);

open(FILE, "stest$$.txt") || die;
$digest = Digest::SHA1->new->addfile(*FILE)->b64digest;
print "$digest\nnot " unless $digest eq "1ZuIK/sQeBwqh+dIACqpnoRQUE4";
print "ok 5\n";
close(FILE);

unlink("stest$$.txt");


print "not " unless sha1_transform(pack('H*', 'dc71a8092d4b1b7b98101d58698d9d1cc48225bb')) 
	eq pack('H*', '2e4c75ad39160f52614d122e6c7ec80446f68567');
print "ok 6\n";

print "not " unless sha1_transform(pack('H*', '0abe1db666612acdf95d2f86d60c65210b78ab23')) 
	eq pack('H*', '7c1c2aabca822912f3016299b160035787477b48');
print "ok 7\n";

print "not " unless sha1_transform(pack('H*', '86da486230e353e0ec5e9220876c687892c0266c')) 
	eq pack('H*', '1da304aec652c21d4f54642434705c91aeaf9abe');
print "ok 8\n";

$digest = Digest::SHA1->new;
print "not " unless $digest->hexdigest eq "da39a3ee5e6b4b0d3255bfef95601890afd80709";
print "ok 9\n";

print "not " unless $digest->clone->hexdigest eq "da39a3ee5e6b4b0d3255bfef95601890afd80709";
print "ok 10\n";

$digest->add("abc");
print "not " unless $digest->clone->hexdigest eq "a9993e364706816aba3e25717850c26c9cd0d89d";
print "ok 11\n";

$digest->add("d");
print "not " unless $digest->hexdigest eq "81fe8bfe87576c3ecb22426f8e57847382917acf";
print "ok 12\n";

print "not " unless $digest->hexdigest eq "da39a3ee5e6b4b0d3255bfef95601890afd80709";
print "ok 13\n";
