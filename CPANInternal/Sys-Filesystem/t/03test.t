use Test::More;
use Sys::Filesystem;

my $fs;
eval { $fs = Sys::Filesystem->new(); };
plan( skip_all => "Cannot initialize Sys::Filesystem" ) if ($@);
ok( ref($fs) eq 'Sys::Filesystem', 'Create new Sys::Filesystem object' );

my @mounted_filesystems = $fs->mounted_filesystems();
my @mounted_filesystems2 = $fs->filesystems( mounted => 1 );
ok( "@mounted_filesystems" eq "@mounted_filesystems2", 'Compare mounted methods' );

my @unmounted_filesystems = $fs->unmounted_filesystems();
my @special_filesystems   = $fs->special_filesystems();

my @regular_filesystems = $fs->regular_filesystems();
my @filesystems         = $fs->filesystems();

SKIP:
{
    unless (@filesystems)
    {
        skip( 'Badly poor supported OS or no file systems found.', 0 );
    }
    else
    {
        ok( @regular_filesystems, 'Get list of regular filesystems' );
        ok( @filesystems,         'Get list of all filesystems' );

        for my $filesystem (@filesystems)
        {
            my $mounted = $fs->mounted($filesystem) || 0;
            my $unmounted = !$mounted;
            ok( $mounted == grep( /^$filesystem$/, @mounted_filesystems ), 'Mounted' );
            ok( $unmounted == grep( /^$filesystem$/, @unmounted_filesystems ), 'Unmounted' );

            my $special = $fs->special($filesystem) || 0;
            my $regular = !$special;
            ok( $special == grep( /^$filesystem$/, @special_filesystems ), 'Special' );
            ok( $regular == grep( /^$filesystem$/, @regular_filesystems ), 'Regular' );

            my ( $device, $options, $format, $volume, $label );
            ok( $device = $fs->device($filesystem), "Get device for $filesystem" );
            ok( defined( $options = $fs->options($filesystem) ), "Get options for $filesystem: $options" );
            ok( $format = $fs->format($filesystem), "Get format for $filesystem" );
            ok( $volume = $fs->volume($filesystem) || 1, "Get volume type for $filesystem" );
            ok( $label  = $fs->label($filesystem)  || 1, "Get label for $filesystem" );
            diag(join( ' - ', $filesystem, $mounted, $special, $device, $options, $format, $volume || '', $label || '' )
                );
        }

        my $device = $fs->device( $filesystems[0] );
        ok( my $foo_filesystem = Sys::Filesystem::filesystems( device => $device ),
            "Get filesystem attached to $device" );
    }
}

done_testing();
