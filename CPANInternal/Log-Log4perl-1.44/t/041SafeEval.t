########################################################################
# Test Suite for Log::Log4perl::Config (Safe compartment functionality)
# James FitzGibbon, 2003 (james.fitzgibbon@target.com)
# Mike Schilli, 2003 (log4perl@perlmeister.com)
########################################################################

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use Test;
BEGIN { plan tests => 23 };

use Log::Log4perl;

ok(1); # If we made it this far, we're ok.

my $example_log = "example" . (stat($0))[9] . ".log";
unlink($example_log);

Log::Log4perl::Config->vars_shared_with_safe_compartment(
  main => [ '$0' ],
);

# test that unrestricted code works properly
Log::Log4perl::Config::allow_code(1);
my $config = <<'END';
    log4perl.logger = INFO, Main
    log4perl.appender.Main = Log::Log4perl::Appender::File
    log4perl.appender.Main.filename = sub { "example" . (stat($0))[9] . ".log" }
    log4perl.appender.Main.layout = Log::Log4perl::Layout::SimpleLayout
END
eval { Log::Log4perl->init( \$config ) };
my $failed = $@ ? 1 : 0;
ok($failed, 0, 'config file with code initializes successfully');

# test that disallowing code works properly
Log::Log4perl::Config->allow_code(0);
eval { Log::Log4perl->init( \$config ) };
$failed = $@ ? 1 : 0;
ok($failed, 1, 'config file with code fails if ALLOW_CODE_IN_CONFIG_FILE is false');

# test that providing an explicit mask causes illegal code to fail
Log::Log4perl::Config->allow_code(1);
Log::Log4perl::Config->allowed_code_ops(':default');
eval { Log::Log4perl->init( \$config ) };
$failed = $@ ? 1 : 0;
ok($failed, 1, 'config file with code fails if ALLOW_CODE_IN_CONFIG_FILE is true and an explicit mask is set');

# test that providing an restrictive convenience mask causes illegal code to fail
Log::Log4perl::Config::allow_code('restrictive');
undef @Log::Log4perl::ALLOWED_CODE_OPS_IN_CONFIG_FILE;
eval { Log::Log4perl->init( \$config ) };
$failed = $@ ? 1 : 0;
ok($failed, 1, 'config file with code fails if ALLOW_CODE_IN_CONFIG_FILE is true and a restrictive convenience mask is set');

# test that providing an restrictive convenience mask causes illegal code to fail
Log::Log4perl::Config->allow_code('safe');
undef @Log::Log4perl::ALLOWED_CODE_OPS_IN_CONFIG_FILE;
eval { Log::Log4perl->init( \$config ) };
$failed = $@ ? 1 : 0;
ok($failed, 0, 'config file with code succeeds if ALLOW_CODE_IN_CONFIG_FILE is true and a safe convenience mask is set');

##################################################
# Test allowed_code_ops_convenience_map accessors
###################################################

# get entire map as hashref
my $map = Log::Log4perl::Config->allowed_code_ops_convenience_map();
ok(ref $map, 'HASH', 'entire map is returned as a hashref');
my $numkeys = keys %{ $map };

# get entire map as hash
my %map = Log::Log4perl::Config->allowed_code_ops_convenience_map();
ok(keys %map, $numkeys, 'entire map returned as hash has same number of keys as hashref');

# replace entire map
Log::Log4perl::Config->allowed_code_ops_convenience_map( {} );
ok(keys %{ Log::Log4perl::Config->allowed_code_ops_convenience_map() }, 0,
    'can replace entire map with an empty one');
Log::Log4perl::Config->allowed_code_ops_convenience_map( \%map );
ok(keys %{ Log::Log4perl::Config->allowed_code_ops_convenience_map() }, $numkeys,
    'can replace entire map with an populated one');

# Add a new name/mask to the map
Log::Log4perl::Config->allowed_code_ops_convenience_map( foo => [ ':default' ] );
ok( keys %{ Log::Log4perl::Config->allowed_code_ops_convenience_map() },
    $numkeys + 1, 'can add a new name/mask to the map');

# get the mask we just added back
my $mask = Log::Log4perl::Config->allowed_code_ops_convenience_map( 'foo' );
ok( $mask->[0], ':default', 'can retrieve a single mask');

###################################################
# Test vars_shared_with_safe_compartment accessors
###################################################

# get entire varlist as hashref
$map = Log::Log4perl::Config->vars_shared_with_safe_compartment();
ok(ref $map, 'HASH', 'entire map is returned as a hashref');
$numkeys = keys %{ $map };

# get entire map as hash
%map = Log::Log4perl::Config->vars_shared_with_safe_compartment();
ok(keys %map, $numkeys, 'entire map returned as hash has same number of keys as hashref');

# replace entire map
Log::Log4perl::Config->vars_shared_with_safe_compartment( {} );
ok(keys %{ Log::Log4perl::Config->vars_shared_with_safe_compartment() }, 0,
    'can replace entire map with an empty one');
Log::Log4perl::Config->vars_shared_with_safe_compartment( \%map );
ok(keys %{ Log::Log4perl::Config->vars_shared_with_safe_compartment() }, $numkeys,
    'can replace entire map with an populated one');

# Add a new name/mask to the map
$Foo::foo = 1;
@Foo::bar = ( 1, 2, 3 );
push @Foo::bar, $Foo::foo; # Some nonsense to avoid 'used only once' warning
Log::Log4perl::Config->vars_shared_with_safe_compartment( Foo => [ '$foo', '@bar' ] );
ok( keys %{ Log::Log4perl::Config->vars_shared_with_safe_compartment() },
    $numkeys + 1, 'can add a new name/mask to the map');

# get the varlist we just added back
my $varlist = Log::Log4perl::Config->vars_shared_with_safe_compartment( 'Foo' );
ok( $varlist->[0], '$foo', 'can retrieve a single varlist');
ok( $varlist->[1], '@bar', 'can retrieve a single varlist');


############################################
# Now the some tests with restricted cspecs
############################################

# Global cspec with illegal code
$config = <<'END';
    log4perl.logger = INFO, Main
    #'U' a global user-defined cspec
    log4j.PatternLayout.cspec.U = sub { unlink 'quackquack'; }
    log4perl.appender.Main = Log::Log4perl::Appender::Screen
    log4perl.appender.Main.layout = Log::Log4perl::Layout::SimpleLayout
END
Log::Log4perl::Config::allow_code('restrictive');
undef @Log::Log4perl::ALLOWED_CODE_OPS_IN_CONFIG_FILE;
eval { Log::Log4perl->init( \$config ) };
$failed = $@ ? 1 : 0;
ok($failed, 1, 
   'global cspec with harmful code rejected on restrictive setting');

# Global cspec with legal code
$config = <<'END';
    log4perl.logger = INFO, Main
    #'U' a global user-defined cspec
    log4j.PatternLayout.cspec.U = sub { 1; }
    log4perl.appender.Main = Log::Log4perl::Appender::Screen
    log4perl.appender.Main.layout = Log::Log4perl::Layout::SimpleLayout
END
Log::Log4perl::Config->allow_code('restrictive');
undef @Log::Log4perl::ALLOWED_CODE_OPS_IN_CONFIG_FILE;
eval { Log::Log4perl->init( \$config ) };
$failed = $@ ? 1 : 0;
ok($failed, 0, 'global cspec with legal code allowed on restrictive setting');

# Local cspec with illegal code
$config = <<'END';
    log4perl.logger = INFO, Main
    log4perl.appender.Main = Log::Log4perl::Appender::Screen
    log4perl.appender.Main.layout = Log::Log4perl::Layout::PatternLayout
    log4perl.appender.Main.layout.cspec.K = sub { symlink("a", "b"); }
END
Log::Log4perl::Config::allow_code('restrictive');
undef @Log::Log4perl::ALLOWED_CODE_OPS_IN_CONFIG_FILE;
eval { Log::Log4perl->init( \$config ) };
$failed = $@ ? 1 : 0;
ok($failed, 1, 'local cspec with harmful code rejected on restrictive setting');

# Global cspec with legal code
$config = <<'END';
    log4perl.logger = INFO, Main
    log4perl.appender.Main = Log::Log4perl::Appender::Screen
    log4perl.appender.Main.layout = Log::Log4perl::Layout::PatternLayout
    log4perl.appender.Main.layout.cspec.K = sub { return sprintf "%1x", $$}
END
Log::Log4perl::Config::allow_code('restrictive');
undef @Log::Log4perl::ALLOWED_CODE_OPS_IN_CONFIG_FILE;
eval { Log::Log4perl->init( \$config ) };
$failed = $@ ? 1 : 0;
ok($failed, 0, 'local cspec with legal code allowed on restrictive setting');

unlink($example_log);
