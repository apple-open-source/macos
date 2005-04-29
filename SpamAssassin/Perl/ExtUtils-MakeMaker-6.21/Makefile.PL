#!/usr/bin/perl -w

# A template for Makefile.PL.
# - Set the $PACKAGE variable to the name of your module.
# - Set $LAST_API_CHANGE to reflect the last version you changed the API 
#   of your module.
# - Fill in your dependencies in PREREQ_PM
# Alternatively, you can say the hell with this and use h2xs.

use 5.00503;

use lib qw(lib);    # build ourself with ourself
use ExtUtils::MakeMaker;
use File::Spec;

BEGIN {
    die "You have File::Spec version $File::Spec::VERSION\n".
      "ExtUtils::MakeMaker requires File::Spec >= 0.8 to build at all.\n"
        if $File::Spec::VERSION < 0.8;
}

$PACKAGE = 'ExtUtils::MakeMaker';
my $version = ${$PACKAGE.'::VERSION'};
$version =~ s/_//;  # for X.Y_Z alpha releases
($PACKAGE_FILE = $PACKAGE) =~ s|::|/|g;
$LAST_API_CHANGE = 5.50;

my $Is_VMS = $^O eq 'VMS';


eval "require $PACKAGE";

unless ($@) { # Make sure we did find the module.
    print <<"CHANGE_WARN" if $version < $LAST_API_CHANGE;

NOTE: There have been API changes between this version and any older
than version $LAST_API_CHANGE!  Please read the Changes file if you
are upgrading from a version older than $LAST_API_CHANGE.

CHANGE_WARN
}

# Test::Harnesses prior to 2.00 shoved all of @INC onto the command line
# when a test had -T.  This made it too long.  So we need a Test::Harness
# > 2.00 on VMS for t/testlib.t
my %prereq = ( );
$prereq{'Test::Harness'} = 2.00 if $^O eq 'VMS';


my $MM = WriteMakefile(
    NAME            => $PACKAGE,
    VERSION_FROM    => "lib/$PACKAGE_FILE.pm", # finds $VERSION
    PREREQ_PM       => { %prereq,

                         # splitpath(), rel2abs()
                         File::Spec         => 0.8,

                         # manifypods needs Pod::Man
                         Pod::Man   => 0,

                         File::Basename     => 0,
                         DirHandle          => 0,
                       },
    INSTALLDIRS     => 'perl',
    EXE_FILES       => [qw(bin/instmodsh)],

    ABSTRACT_FROM   => "lib/$PACKAGE_FILE.pm",
    AUTHOR          => 'Michael G Schwern <schwern@pobox.com>',
);

if( !$Is_VMS && $MM->{PERL} =~ /\S\s+\S/ ) {
    require Test::Harness;
    my $th_version = defined $Test::Harness::VERSION ? $Test::Harness::VERSION
                                                     : 0;
    print <<SPACE_WARN if $th_version < 2.27;

NOTE: Your Perl looks like it contains a space in the path name.
MakeMaker is now OK with that but your version of Test::Harness is not
which means 'make test' will likely puke.

You will have to install this new version of MakeMaker, then upgrade
Test::Harness from CPAN, then run the MakeMaker tests.

SPACE_WARN

}

{
    package MY;

    my($lib, $tlib);
    BEGIN { 
        $lib  = File::Spec->canonpath('lib/');
        $tlib = File::Spec->rel2abs('t/lib/');
    }

    # Make sure PERLRUN uses the MakeMaker about to be installed
    # and not the currently installed one.
    sub init_PERL {
        my($self) = shift;
        $self->SUPER::init_PERL;
        $self->{PERLRUN} .= qq{ "-I$lib"};
        $self->{FULLPERLRUN} .= qq{ "-I$lib"};
    }

    use Config;
    sub test_via_harness {
        my($self, $orig_perl, $tests) = @_;

        my @perls = ($orig_perl);
        if( $ENV{PERL_TEST_ALL} ) {
            push @perls, 
              map "PERL5LIB=$lib$Config{path_sep}$tlib /usr/local/bin/$_",
                'perl5.5.3';

            push @perls, map "PERL5LIB=$lib /usr/local/bin/$_", 
              qw(perl5.6.0
                 perl5.6.1
                 perl
                 perl5.8.0
                 perl5.8.0-ithreads
                 bleadperl
                );
        }

        my $out;
        foreach my $perl (@perls) {
            $out .= $self->SUPER::test_via_harness($perl, $tests);
        }

        return $out;
    }
}
