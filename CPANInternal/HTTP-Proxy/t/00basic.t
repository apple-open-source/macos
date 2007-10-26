use vars qw( @modules );

BEGIN {
    use File::Find;
    use vars qw( @modules );

    find( sub { push @modules, $File::Find::name if /\.pm$/ }, 'blib/lib' );
}

use Test::More tests => scalar @modules;

use_ok($_) for sort map { s!/!::!g; s/\.pm$//; s/^blib::lib:://; $_ } @modules;

