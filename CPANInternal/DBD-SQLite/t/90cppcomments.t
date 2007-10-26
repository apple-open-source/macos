use Test;
use DBI;
use Fatal qw(open);
my @c_files = <*.c>, <*.xs>;
plan tests => scalar(@c_files);

FILE:
foreach my $file (@c_files) {
    open(F, $file);
    my $line = 0;
    while (<F>) {
        $line++;
        if (/^(.*)\/\//) {
            my $m = $1;
            if ($m !~ /\*/ && $m !~ /http:$/) { # skip the // in c++ comment in parse.c
                ok(0, 1, "C++ comment in $file line $line");
                next FILE;
            }
        }
    }
    ok(1,1,"$file has no C++ comments");
    close(F);
}
