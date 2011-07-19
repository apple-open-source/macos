use Test::More;

eval "use Pod::Coverage 0.19";
plan skip_all => 'Pod::Coverage 0.19 required' if $@;
eval "use Test::Pod::Coverage 1.04";
plan skip_all => 'Test::Pod::Coverage 1.04 required' if $@;

plan skip_all => 'set TEST_POD to enable this test'
  unless ( $ENV{TEST_POD} || -e 'MANIFEST.SKIP' );

my @modules = sort { $a cmp $b } ( Test::Pod::Coverage::all_modules() );
plan tests => scalar(@modules);

# Since this is about checking documentation, a little documentation
# of what this is doing might be in order...
# The exceptions structure below is a hash keyed by the module
# name.  The value for each is a hash, which contains one or more
# (although currently more than one makes no sense) of the following
# things:-
#   skip   => a true value means this module is not checked
#   ignore => array ref containing list of methods which
#             do not need to be documented.
my $exceptions = {
    'SQL::Abstract' => {
        ignore => [
            qw/belch
              puke/
        ]
    },
    'SQL::Abstract::Test' => { skip => 1 },
};

foreach my $module (@modules) {
  SKIP:
    {
        skip "$module - No user visible methods",
          1
          if ( $exceptions->{$module}{skip} );

        # build parms up from ignore list
        my $parms = {};
        $parms->{trustme} =
          [ map { qr/^$_$/ } @{ $exceptions->{$module}{ignore} } ]
          if exists( $exceptions->{$module}{ignore} );

        # run the test with the potentially modified parm set
        pod_coverage_ok( $module, $parms, "$module POD coverage" );
    }
}
