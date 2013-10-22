# 00-basic.t

use Test::More tests => 12;

BEGIN {
    use_ok( 'Crypt::SSLeay' );
    use_ok( 'Crypt::SSLeay::CTX' );
    use_ok( 'Crypt::SSLeay::Conn' );
    use_ok( 'Crypt::SSLeay::Err' );
    use_ok( 'Crypt::SSLeay::MainContext', 'main_ctx' );
    use_ok( 'Crypt::SSLeay::X509' );
    use_ok( 'Net::SSL' );
}

SKIP: {
    skip( 'Test::Pod not installed on this system', 2 )
        unless do {
            eval "use Test::Pod";
            $@ ? 0 : 1;
        };

    pod_file_ok( 'SSLeay.pm' );
    pod_file_ok( 'lib/Net/SSL.pm' );
}

SKIP: {
    skip( 'Test::Pod::Coverage not installed on this system', 2 )
        unless do {
            eval "use Test::Pod::Coverage";
            $@ ? 0 : 1;
        };
    pod_coverage_ok( 'Crypt::SSLeay', 'Crypt-SSLeay POD coverage is go!' );
    pod_coverage_ok( 'Net::SSL', 'Net::SSL POD coverage is go!' );
}

my $ctx = main_ctx();
is(ref($ctx), 'Crypt::SSLeay::CTX', 'we have a context');
