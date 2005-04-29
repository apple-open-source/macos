use ExtUtils::MakeMaker;
use strict;
use Getopt::Std;
my $opt = {};
getopt( 'n', $opt );

my $have_zlib = eval { require IO::Zlib; 1 };

unless( $] >= 5.005_03 ) {
    die qq[Archive::Tar requires perl version 5.005_03 or higher to run\n];
}

if( !$have_zlib and !( exists $opt->{n} ) ) {
    warn    qq[You do not have IO::Zlib installed. This means you can not read ].
            qq[or write compressed archive!\n] .
            qq[Note: you can disable this warning by invoking Makefile.PL with ] .
            qq['-n'\n];
}

WriteMakefile (
    NAME            => 'Archive::Tar',
    VERSION_FROM    => 'lib/Archive/Tar.pm', # finds $VERSION
    dist            => { COMPRESS => 'gzip -9f', SUFFIX => 'gz' },
    PREREQ_PM       => {'Test::More' => 0, 
                        'File::Spec' => 0.82,
                        'Test::Harness' => 2.26, # there's a bug in older versions
                    },
    AUTHOR          => 'Jos Boumans <kane[at]cpan.org>',
	ABSTRACT        => 'Manipulates TAR archives'
);             
