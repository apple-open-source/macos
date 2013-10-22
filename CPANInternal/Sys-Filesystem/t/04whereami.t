use Test::More tests => 3;
use Sys::Filesystem;
use Cwd qw(abs_path);
use Config;

my $RealTest = abs_path(__FILE__);
my $RealPerl = abs_path( $Config{perlpath} );
if ( $^O ne 'VMS' )
{
    $RealPerl .= $Config{_exe}
      unless $RealPerl =~ m/$Config{_exe}$/i;
}
$RealTest = ucfirst($RealTest) if ( $^O =~ m/Win32/ );

my $sfs;
eval { $sfs = Sys::Filesystem->new(); };
plan( skip_all => "Cannot initialize Sys::Filesystem" ) if ($@);
ok( ref($sfs) eq 'Sys::Filesystem', 'Create new Sys::Filesystem object' );

my ( $binmount, $mymount );

my @mounted_filesystems = sort { length($b) <=> length($a) } $sfs->filesystems( mounted => 1 );
SKIP:
{
    unless (@mounted_filesystems)
    {
        if ( $sfs->supported() )
        {
            diag("Unexpected empty list of mounted filesystems");
        }
        skip( 'Badly poor supported OS or no file systems found.', 0 );
    }
    foreach my $fs (@mounted_filesystems)
    {
        diag("Checking '$fs' being mountpoint for '$RealPerl' or '$RealTest' ...")
          if ( $^O eq 'MSWin32' or $^O eq 'cygwin' );
        if ( !defined($binmount) && ( 0 == index( $RealPerl, $fs ) ) )
        {
            $binmount = $fs;
        }

        if ( !defined($mymount) && ( 0 == index( $RealTest, $fs ) ) )
        {
            $mymount = $fs;
        }
    }
  TODO:
    {
        local $TODO = "Known fail for MSWin32, cygwin & Co. - let's make it not so important ...";
        ok( $mymount, sprintf( q{Found mountpoint for test file '%s' at '%s'}, $RealTest, $mymount || '<n/a>' ) );
        ok( $binmount,
            sprintf( q{Found mountpoint for perl executable '%s' at '%s'}, $RealPerl, $binmount || '<n/a>' ) );
    }
}
