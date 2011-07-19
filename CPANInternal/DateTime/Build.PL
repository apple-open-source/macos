use 5.006;

use strict;
use warnings;

use Module::Build;

my $force_pp = grep { $_ eq '--pp' } @ARGV;

my @pp = _make_pp_tests();

my $builder = Module::Build->new(
    module_name => 'DateTime',
    license     => 'perl',
    c_source    => 'c',
    requires    => {
        'DateTime::Locale'   => '0.41',
        'DateTime::TimeZone' => '0.59',
        'Params::Validate'   => '0.76',
        'Scalar::Util'       => '0',
        'Time::Local'        => '1.04',
    },
    build_requires => {
        'Pod::Man'        => '1.14',
        'Test::Exception' => '0',
        'Test::More'      => '0.34',
    },
    add_to_cleanup => \@pp,
    create_readme  => 1,
    sign           => 1,
    meta_merge     => {
        resources => {
            homepage   => 'http://datetime.perl.org/',
            bugtracker => 'http://rt.cpan.org/NoAuth/Bugs.html?Dist=DateTime',
            repository =>
                'https://perl-date-time.svn.sourceforge.net/svnroot/perl-date-time',
        },
    },
);

if ($force_pp) {
    $builder->build_elements(
        [ grep { $_ ne 'xs' } @{ $builder->build_elements() } ] );
}

$builder->create_build_script();

sub _make_pp_tests {
    return unless -d '.svn';

    require File::Slurp;
    File::Slurp->import(qw( read_file write_file ));

    opendir my $dh, 't'
        or die "Cannot read t: $!";

    my @pp;
    foreach my $file ( grep {/^\d.+\.t$/} readdir $dh ) {
        next if $file eq '26dt_leapsecond_pm.t';
        next if $file eq '39no-so.t';
        next if $file !~ /^\d/;

        my $test = read_file("t/$file");
        $test
            = "#!/usr/bin/perl -w\n\nBEGIN { \$ENV{PERL_DATETIME_PP} = 1 }\n\n$test";

        my $new_file = "t/pp_$file";
        write_file( $new_file, $test );

        push @pp, $new_file;
    }

    return @pp;
}
