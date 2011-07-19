use 5.006;
use strict;
use lib 'inc';
eval "require Pod::WikiDoc";
my $class = $@ ? "Module::Build" : "Module::Build::WikiDoc";
eval "require $class";

$class->new( 
    module_name         => 'Sub::Uplevel',
    dist_author         => 'David A. Golden <dagolden@cpan.org>',
    license             => 'perl',
    create_readme       => 1,
    create_makefile_pl  => 'traditional',
    requires        => {
        'perl' => 5.006,
    },
    build_requires => {
        'Carp' => 0, 
        'Test::More' => 0.47, 
    },
    meta_add            => { 
        no_index => { 
            package => [ 'DB' ], 
            directory => [ qw{
                examples
                inc
            }],
        },
    },
    meta_merge          => {
        resources => {
            repository => 'http://github.com/dagolden/sub-uplevel',
        },
    },
)->create_build_script;

