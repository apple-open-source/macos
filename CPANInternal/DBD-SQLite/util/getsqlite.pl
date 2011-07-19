#!/usr/bin/perl

use strict;
use FindBin;
use File::Spec::Functions ':ALL';
use LWP::Simple qw(getstore);
use ExtUtils::Command;

chdir(catdir($FindBin::Bin, updir())) or die "Failed to change to the dist root";

my $version = shift || die "Usage: getsqlite.pl <version>\n";

print("downloading http://www.sqlite.org/sqlite-amalgamation-$version.tar.gz\n");
my $rv = getstore(
	"http://www.sqlite.org/sqlite-amalgamation-$version.tar.gz", 
	"sqlite.tar.gz",
);
die "Failed to download" if $rv != 200;
print("done\n");

rm_rf('sqlite') || rm_rf("sqlite-$version");
xsystem("tar zxvf sqlite.tar.gz");
chdir("sqlite") || chdir("sqlite-$version") || die "SQLite directory not found";

xsystem("cp sqlite3.c ../");
xsystem("cp sqlite3.h ../");
xsystem("cp sqlite3ext.h ../");

exit(0);

sub xsystem {
    local $, = ", ";
    print("@_\n");
    my $ret = system(@_);
    if ($ret != 0) {
       die "system(@_) failed: $?";
    }
}
