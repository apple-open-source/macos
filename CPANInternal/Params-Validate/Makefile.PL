# The perl/C checking voodoo is stolen from Graham Barr's
# Scalar-List-Utils distribution.
use 5.006;

use strict;
use warnings;

use ExtUtils::MakeMaker;
use Config qw(%Config);
use File::Spec;

my $no_xs;
my $force_xs;
for (@ARGV)
{
    /^--pm/ and $no_xs = 1;
    /^--xs/ and $no_xs = 0;
}

if ($no_xs)
{
    write_makefile();
    exit;
}

unless (defined $no_xs)
{
    check_for_compiler()
        or no_cc();

    if ( -d '.svn' )
    {
        local *DIR;
        opendir DIR, "t" or die "Cannot read t: $!";

        foreach my $file ( grep { /^\d.+\.t$/ } readdir DIR )
        {
            next if $file =~ /^99/;

            my $real_file = File::Spec->catfile( 't', $file );

            local *F;
            open F, "<$real_file" or die "Cannot read $real_file: $!";

            my $shbang = <F>;
            my $test = do { local $/; <F> };

            close F;

            $test = "$shbang\nBEGIN { \$ENV{PV_TEST_PERL} = 1 }\n\n$test";

            my $new_file = File::Spec->catfile( 't', "zz_$file" );
            open F, ">$new_file" or die "Cannot write $new_file: $!";

            print F $test;

            close F;
        }
    }
}

write_makefile();

sub write_makefile
{
    my %prereq =
        ( 'Test::More' => 0,
          'Scalar::Util' => 0,
        );
    $prereq{'Attribute::Handlers'} = 0 if $] >= 5.006;

    my $zz = join ' ', glob File::Spec->catfile( 't', 'zz_*.t' );

    my $ccflags = -d '.svn' ? '-Wall' : '';

    WriteMakefile( VERSION_FROM  => "lib/Params/Validate.pm",
                   NAME          => "Params::Validate",
                   PREREQ_PM     => \%prereq,
                   CONFIGURE     => \&init,
                   CCFLAGS       => $ccflags,
                   clean         => { FILES => "test.c test.o $zz" },
                   ABSTRACT_FROM => 'lib/Params/Validate.pm',
                   AUTHOR        => 'Dave Rolsky, <autarch@urth.org>',
                   LICENSE       => 'perl',
                 );
}

sub init
{
    my $hash = $_[1];

    if ($no_xs)
    {
        @{ $hash }{ 'XS', 'C' } = ( {}, [] );
    }

    $hash;
}

sub no_cc
{
    $no_xs = 1;
    print <<'EOF';

 I cannot determine if you have a C compiler
 so I will install a perl-only implementation

 You can force installation of the XS version with

    perl Makefile.PL --xs

EOF

    write_makefile();
    exit;
}

sub check_for_compiler
{
    print "Testing if you have a C compiler\n";

    eval { require ExtUtils::CBuilder };
    if ($@)
    {
        return _check_for_compiler_manually();
    }
    else
    {
        return _check_for_compiler_with_cbuilder();
    }
}

sub _check_for_compiler_with_cbuilder
{
    my $cb = ExtUtils::CBuilder->new( quiet => 1 );

    return $cb->have_compiler;
}

sub _check_for_compiler_manually
{
    unless ( open F, ">test.c" )
    {
        warn "Cannot write test.c, skipping test compilation and installing pure Perl version.\n";
        return 0;
    }

    print F <<'EOF';
int main() { return 0; }
EOF

    close F or return 0;

    my $cc = $Config{cc};
    if ( $cc =~ /cl(\.exe)?$/ )
    {
        # stupid stupid MSVC compiler hack tacken from version.pm's
        # Makefile.PL
        $cc .= ' -c'; # prevent it from calling the linker
    }

    system( "$cc -o test$Config{obj_ext} test.c" ) and return 0;

    unlink $_
        for grep { -f } 'test.c', "test$Config{obj_ext}";

    return 1;
}
