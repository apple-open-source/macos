# A template for Makefile.PL used by Arena Networks.
# - Set the $PACKAGE variable to the name of your module.
# - Set $LAST_API_CHANGE to reflect the last version you changed the API 
#   of your module.
# - Fill in your dependencies in PREREQ_PM
# Alternatively, you can say the hell with this and use h2xs.

use 5.004;

use Config;
use ExtUtils::MakeMaker;

$PACKAGE = 'Test::Simple';
($PACKAGE_FILE = $PACKAGE) =~ s|::|/|g;
$LAST_API_CHANGE    = 0.48;
$LAST_THREAD_CHANGE = 0.48;

eval "require $PACKAGE";

unless ($@) { # Make sure we did find the module.
    if( ${$PACKAGE.'::VERSION'} < $LAST_API_CHANGE ) {
        printf <<"CHANGE_WARN", $LAST_API_CHANGE;

NOTE: There have been API changes between this version and any older
than version %s!  Please see the Changes file for details.

CHANGE_WARN

        sleep 5;
    }
    if( $] >= 5.008001 && $Config{useithreads} &&
        ${$PACKAGE.'::VERSION'} < $LAST_THREAD_CHANGE 
      ) 
    {
        printf <<"THREAD_WARN", $LAST_THREAD_CHANGE;

NOTE: The behavior of Test::More and threads has changed between this
version and any older than version %s!  Please see the Changes file
for details.

THREAD_WARN

        sleep 5;
    }

}

WriteMakefile(
    NAME            => $PACKAGE,
    VERSION_FROM    => "lib/$PACKAGE_FILE.pm", # finds $VERSION
    PREREQ_PM       => { 
                        Test::Harness   => 2.03,
                       },
    # Added to the core somewhere around 5.7.2.
    INSTALLDIRS     => $] >= 5.00702 ? 'perl' : 'site',
    ($ExtUtils::MakeMaker::VERSION <= 6.31 ? (LICENSE         => 'perl') : ()),
);


{
    package MY;

    use File::Basename;
    sub test_via_harness {
        my($self, $orig_perl, $tests) = @_;

        my @perls = ();
        if( $ENV{PERL_TEST_ALL} ) {
            require File::Spec;
            my $tlib = File::Spec->rel2abs('t/lib/');
            push @perls, 
              map "PERL5LIB=$tlib $_",
                qw(
                   perl5.4.0
                   perl5.4.5
                   perl5.5.3
                   perl5.6.0
                   perl5.6.1
                  );

            push @perls,
              qw(
                 perl
                 perl5.8.0
                 perl5.8.0-ithreads
                 bleadperl
                );
        }
        else {
            push @perls, $orig_perl;
        }

        my $out;
        foreach my $perl (@perls) {
            $out .= $self->SUPER::test_via_harness($perl, $tests) . "\n";
        }
        $out =~ s{-I\$\(PERL_\w*LIB\)}{}g;

        return $out;
    }
}


# Older versions of Test::Simple were very naughty about being required and
# exitted with 255.  This overrides that behavior so it builds from CPAN.
END {
    exit(0);
}
