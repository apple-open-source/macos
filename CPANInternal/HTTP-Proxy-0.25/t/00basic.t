use vars qw( @modules );

BEGIN {
    use Config;
    use File::Find;
    use vars qw( @modules );

    find( sub { push @modules, $File::Find::name if /\.pm$/ }, 'blib/lib' );
}

use Test::More tests => scalar @modules;

for ( sort map { s!/!::!g; s/\.pm$//; s/^blib::lib:://; $_ } @modules ) {
SKIP:
    {
        skip "$^X is not a threaded Perl", 1
            if /Thread/ && !$Config{usethreads};
        use_ok($_);
    }
}

