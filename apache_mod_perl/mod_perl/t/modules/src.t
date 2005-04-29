use ExtUtils::testlib;

use Apache::testold;
use Apache::src;

my $i = 0;

skip_test if (WIN32 and !Apache::src->mmn_eq);

print "1..6\n";

my $src = Apache::src->new;

test ++$i, $src;

print "dir=", $src->dir, "\n";
test ++$i, -d $src->dir;

print "main=", $src->main, "\n";
test ++$i, -e join("/", $src->main, "httpd.h");

my $mmn = $src->module_magic_number;
print "module_magic_number = $mmn\n";
test ++$i, $mmn;

my $v = $src->httpd_version;
print "httpd_version = $v\n";
test ++$i, $v;

print $src->inc, "\n";

for (split /\s+/, $src->inc) {
    s/^-I//;
    -d $_ or die "can't stat $_ $!\n";
}

test ++$i, 1;
