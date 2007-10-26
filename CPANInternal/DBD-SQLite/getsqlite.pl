use strict;
use LWP::Simple qw(getstore);
use ExtUtils::Command;

my $version = shift || die "Usage: getsqlite.pl <version>\n";

print("downloading http://www.sqlite.org/sqlite-$version.tar.gz\n");
if (getstore(
	"http://www.sqlite.org/sqlite-$version.tar.gz", 
	"sqlite.tar.gz") != 200) {
   die "Failed to download";
}
print("done\n");

rm_rf('sqlite');
xsystem("tar zxvf sqlite.tar.gz");
chdir("sqlite") || chdir("sqlite-$version") || die "SQLite directory not found";
xsystem("sh configure --enable-utf8");
xsystem("make parse.c sqlite3.h opcodes.h opcodes.c");
eval { xsystem("make keywordhash.h") };

my %skip = map { $_ => 1 } map { chomp; $_ } <DATA>;
warn("Skip: $_\n") for keys %skip;

foreach (<*.[ch]>, `find src -name \\*.[ch]`) {
    chomp;
    next if $skip{$_};
    next if /\btest.*\.c$/;
    xsystem("cp $_ ../");
}

exit(0);

sub xsystem {
    local $, = ", ";
    print("@_\n");
    my $ret = system(@_);
    if ($ret != 0) {
       die "system(@_) failed: $?";
    }
}

__DATA__
lempar.c
src/threadtest.c
src/tclsqlite.c
src/shell.c
src/lemon.c
src/md5.c
src/encode.c
src/experimental.c
