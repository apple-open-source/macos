use strict;
use warnings;
use Test::More;
use HTTP::Proxy::BodyFilter::save;
use File::Temp qw( tempdir );
use File::Spec::Functions;

# a sandbox to play in
my $dir = tempdir( CLEANUP => 1 );

my @errors = (
    [   [ keep_old => 1, timestamp => 1 ] =>
            qr/^Can't timestamp and keep older files at the same time/
    ],
    [ [ status => 200 ] => qr/^status must be an array reference/ ],
    [   [ status => [qw(200 007 )] ] =>
            qr/status must contain only HTTP codes/
    ],
    [ [ filename => 'zlonk' ] => qr/^filename must be a code reference/ ],
);
my @data = (
    'recusandae veritatis illum quos tempor aut quidem',
    'necessitatibus lorem aperiam facere consequuntur incididunt similique'
);
my @d = ( prefix => $dir );    # defaults
my @templates = (

    # args, URL => filename
    [   [@d],
        'http://bam.fr/zok/awk.html' =>
            catfile( $dir, qw(bam.fr zok awk.html) )
    ],
    [   [ @d, multiple => 0 ],
        'http://bam.fr/zok/awk.html' =>
            catfile( $dir, qw(bam.fr zok awk.html) )
    ],
    [   [@d],
        'http://bam.fr/zok/awk.html' =>
            catfile( $dir, qw(bam.fr zok awk.html.1) )
    ],
    [   [ @d, no_host => 1 ],
        'http://bam.fr/zok/awk.html' => catfile( $dir, qw(zok awk.html ) )
    ],
    [   [ @d, no_dirs => 1 ],
        'http://bam.fr/zok/awk.html' => catfile( $dir, qw(bam.fr awk.html) )
    ],
    [   [ @d, no_host => 1, no_dirs => 1 ],
        'http://bam.fr/zok/awk.html' => catfile( $dir, 'awk.html' )
    ],
    [   [ @d, no_dirs => 1 ],
        'http://bam.fr/zok/' => catfile( $dir, qw(bam.fr index.html) )
    ],
    #[ [@d], 'http://bam.fr/zok/' => "$dir/bam.fr/index.html" ],
    [   [ template => "$dir/%p" ],
        'http://bam.fr/pow/zok.html' => catfile( $dir, qw(pow zok.html) )
    ],
    [   [ template => "$dir/%f" ],
        'http://bam.fr/pow/zok.html' => catfile( $dir, 'zok.html' )
    ],
    [   [ template => "$dir/%p" ],
        'http://bam.fr/zam.html?q=pow' => catfile( $dir, 'zam.html' )
    ],
    # Win32 does not accept '?' in file names
    (   [   [ template => "$dir/%P" ],
            'http://bam.fr/zam.html?q=pow' =>
                catfile( $dir, 'zam.html?q=pow' )
        ]
        ) x ( $^O ne 'MSWin32' ),
    [   [ @d, cut_dirs => 2 ],
        'http://bam.fr/a/b/c/d/e.html' =>
            catfile( $dir, qw(bam.fr c d e.html) )
    ],
    [   [ @d, cut_dirs => 2, no_host => 1 ],
        'http://bam.fr/a/b/c/d/e.html' => catfile( $dir, qw(c d e.html) )
    ],
    [   [ @d, cut_dirs => 5, no_host => 1 ],
        'http://bam.fr/a/b/c/d/e.html' => catfile( $dir, 'e.html' )
    ],

    # won't save
    [ [ @d, keep_old => 1 ], 'http://bam.fr/zok/awk.html' => undef ],
);
my @responses = (
    [   [@d],
        'http://bam.fr/a.html' => 200,
        catfile( $dir, qw(bam.fr a.html) )
    ],
    [ [@d], 'http://bam.fr/b.html' => 404, undef ],
    [   [ @d, status => [ 200, 404 ] ],
        'http://bam.fr/c.html' => 404,
        catfile( $dir, qw(bam.fr c.html) )
    ],
);

plan tests => 2 * @errors    # error checking
    + 1                      # simple test
    + 7 * 2                  # filename tests: 2 that save
    + 5 * 2                  # filename tests: 2 that don't
    + 2 * @templates         # all template tests
    + 2 * @responses         # all responses tests
    ;

# some variables
my $proxy = HTTP::Proxy->new( port => 0 );
my ( $filter, $data, $file, $buffer );

# test the save filter
# 1) errors in new
for my $t (@errors) {
    my ( $args, $regex ) = @$t;
    ok( !eval { HTTP::Proxy::BodyFilter::save->new(@$args); 1; },
        "new( @$args ) fails" );
    like( $@, $regex, "Error matches $regex" );
}

# 2) code for filenames
$filter = HTTP::Proxy::BodyFilter::save->new( filename => sub {$file} );
$filter->proxy($proxy);

# simple check
ok( !$filter->will_modify, 'Filter does not modify content' );

# loop on four requests
# two that save, and two that won't
for my $name ( qw( zlonk.pod kayo.html ), undef, '' ) {
    $file = $name ? catfile( $dir, $name ) : $name;

    my $req = HTTP::Request->new( GET => 'http://www.example.com/' );
    ok( my $ok = eval {
            $filter->begin($req);
            1;
        },
        'Initialized filter without error'
    );
    diag $@ if !$ok;

    if ($file) {
        is( $filter->{_hpbf_save_filename}, $file, "Got filename ($file)" );
    }
    else {
        ok( !$filter->{_hpbf_save_filename}, 'No filename' );
    }

    my $filter_fh;
    if ($name) {
        ok( $filter->{_hpbf_save_fh}->opened, 'Filehandle opened' );
        $filter_fh = $filter->{_hpbf_save_fh};
    }
    else {
        ok( !exists $filter->{_hpbf_save_fh}, 'No filehandle' );
    }

    # add some data
    $buffer = '';
    ok( eval {
            $filter->filter( \$data[0], $req, '', \$buffer );
            $filter->filter( \$data[1], $req, '', undef );
            $filter->end();
            1;
        },
        'Filtered data without error'
    );
    diag $@ if $@;

    # file closed now
    ok( !defined $filter->{_hpbf_save_fh}, 'No filehandle' );
    if ($filter_fh) {
        ok( !$filter_fh->opened, 'Filehandle closed' );

        # check the data
        open my $fh, $file or diag "Can't open $file: $!";
        is( join( '', <$fh> ), join( '', @data ), 'All data saved' );
        close $fh;
    }

}

# 3) the multiple templating cases
for my $t (@templates) {
    my ( $args, $url, $filename ) = @$t;
    my $filter = HTTP::Proxy::BodyFilter::save->new(@$args);
    $filter->proxy($proxy);
    my $req = HTTP::Request->new( GET => $url );

    # filter initialisation
    ok( my $ok = eval {
            $filter->begin($req);
            1;
        },
        'Initialized filter without error'
    );
    diag $@ if !$ok;
    my $mesg = defined $filename ? "$url => $filename" : "Won't save $url";
    is( $filter->{_hpbf_save_filename}, $filename, $mesg );
}

# 4) some cases that depend on the response
for my $t (@responses) {
    my ( $args, $url, $status, $filename ) = @$t;
    my $filter = HTTP::Proxy::BodyFilter::save->new(@$args);
    $filter->proxy($proxy);
    my $res = HTTP::Response->new($status);
    $res->request( HTTP::Request->new( GET => $url ) );

    ok( my $ok = eval {
            $filter->begin($res);
            1;
        },
        'Initialized filter without error'
    );
    diag $@ if !$ok;
    if ($filename) {
        is( $filter->{_hpbf_save_filename},
            $filename, "$url ($status) => $filename" );
    }
    else {
        ok( !$filter->{_hpbf_save_filename},
            "$url ($status) => No filename" );
    }
}

