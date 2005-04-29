print "1..2\n";

use Digest::SHA1 ();

$sha1 = Digest::SHA1->new;

eval {
   use vars qw(*FOO);
   $sha1->addfile(*FOO);
};
print "not " unless $@ =~ /^Bad filehandle: FOO at/;
print "ok 1\n";

open(BAR, "no-existing-file.$$");
eval {
    $sha1->addfile(*BAR);
};
print "not " unless $@ =~ /^No filehandle passed at/;
print "ok 2\n";
