use strict;
use warnings;
use Module::Build;

my $builder = Module::Build->new(
    module_name         => 'HTTP::Proxy',
    license             => 'perl',
    dist_author         => 'Philippe "BooK" Bruhat <book@cpan.org>',
    dist_version_from   => 'lib/HTTP/Proxy.pm',
    requires => {
        'Test::More' => 0,
        'HTTP::Daemon' => 1.25,
        'LWP::UserAgent' => 2,
    },
    add_to_cleanup      => [ 'HTTP-Proxy-*' ],
);

$builder->create_build_script();
