use ExtUtils::MakeMaker;
use strict;
use Getopt::Std;
my $opt = {};
getopts( 'n', $opt );

my $have_zlib = eval { require IO::Zlib; 1 };

unless( $] >= 5.005_03 ) {
    die qq[Archive::Tar requires perl version 5.005_03 or higher to run\n];
}

if( !$have_zlib and !$opt->{n} ) {
    warn    qq[You do not have IO::Zlib installed. This means you can ].
            qq[not read or write compressed archive!\n] .
            qq[Note: you can disable this warning (and the prerequisite) ].
            qq[by invoking Makefile.PL with '-n'\n];
}

my $prereqs = {
    'Test::More'    => 0, 
    'File::Spec'    => 0.82,
    'Test::Harness' => 2.26,    # bug in older versions
    'IO::Zlib'      => 1.01,    # the most recent, not modified since 2001
    'IO::String'    => 0,       # for better 'return stringified archive'
};

### ok, you didn't want IO::Zlib ###
delete $prereqs->{'IO::Zlib'} if $opt->{n};


WriteMakefile (
    NAME            => 'Archive::Tar',
    VERSION_FROM    => 'lib/Archive/Tar.pm', # finds $VERSION
    dist            => { COMPRESS => 'gzip -9f', SUFFIX => 'gz' },
    EXE_FILES       => ['bin/ptar'],
    PREREQ_PM       => $prereqs,
    AUTHOR          => 'Jos Boumans <kane[at]cpan.org>',
	ABSTRACT        => 'Manipulates TAR archives'
);             
