use strict;
use warnings;
use lib qw(t/lib);
use Test::More;
use Log::Dispatch;
use Log::Dispatch::TestUtil qw(cmp_deeply);
use File::Temp qw( tempdir );

my $tempdir = tempdir( CLEANUP => 1 );

{
    my $emerg_log = File::Spec->catdir( $tempdir, 'emerg.log' );

    # Short syntax
    my $dispatch0 = Log::Dispatch->new(
        outputs => [
            [
                'File', name => 'file', min_level => 'emerg',
                filename => $emerg_log
            ],
            [
                '+Log::Dispatch::Screen', name => 'screen',
                min_level => 'debug'
            ]
        ]
    );

    # Short syntax alternate (2.23)
    my $dispatch1 = Log::Dispatch->new(
        outputs => [
            'File' => {
                name => 'file', min_level => 'emerg', filename => $emerg_log
            },
            '+Log::Dispatch::Screen' =>
                { name => 'screen', min_level => 'debug' }
        ]
    );

    # Long syntax
    my $dispatch2 = Log::Dispatch->new;
    $dispatch2->add(
        Log::Dispatch::File->new(
            name      => 'file',
            min_level => 'emerg',
            filename  => $emerg_log
        )
    );
    $dispatch2->add(
        Log::Dispatch::Screen->new( name => 'screen', min_level => 'debug' )
    );

    cmp_deeply( $dispatch0, $dispatch2,
        "created equivalent dispatchers - 0" );
    cmp_deeply( $dispatch1, $dispatch2,
        "created equivalent dispatchers - 1" );
}

{
    eval { Log::Dispatch->new( outputs => ['File'] ) };
    like( $@, qr/expected arrayref/,
        "got error for expected inner arrayref" );
}
{
    eval { Log::Dispatch->new( outputs => 'File' ) };
    like( $@, qr/not one of the allowed types: arrayref/,
        "got error for expected outer arrayref" );
}

done_testing();
