# test for loading additional methods from file-defined packages
# by Mark Hedges (  hedges   -at|   scriptdolphin.com )

use strict;
use Test::More tests => 7 * 5;
use Test::Exception;

use lib 't/lib';

use make_dbictest_db;

use DBIx::Class::Schema::Loader;

$ENV{SCHEMA_LOADER_BACKCOMPAT} = 1;

# In the first test run, then, Foo should be a DBICTestMethods::Namespaces::Schema::Result::Foo

run_test_sequence(
    testname        => "naming => 'current'",
    schema_class    => 'DBICTestMethods::Namespaces::Schema',
    foo_class       => 'DBICTestMethods::Namespaces::Schema::Result::Foo',
    schema_opts     => {
        naming => 'current',
    },
);

# In the second test run with use_namespaces => 0 (backcompat), Foo should be a DBICTestMethods::Backcompat::Schema

run_test_sequence(
    testname        => "naming => 'current', use_namespaces => 0",
    schema_class    => 'DBICTestMethods::Backcompat::Schema',
    foo_class       => 'DBICTestMethods::Backcompat::Schema::Foo',
    schema_opts     => {
        naming              => 'current',
        use_namespaces      => 0,
    },
);

# In the third test, with use_namespaces => 1, Foo gets the explicit Result class again

run_test_sequence(
    testname        => "naming => 'current', use_namespaces => 1",
    schema_class    => 'DBICTestMethods::Namespaces::Schema',
    foo_class        => 'DBICTestMethods::Namespaces::Schema::Result::Foo',
    schema_opts     => {
        naming              => 'current',
        use_namespaces      => 1,
    },
);

# try it in full backcompat 0.04006 mode with no schema options

run_test_sequence(
    testname        => "no naming or namespaces options (0.04006 mode)",
    schema_class    => 'DBICTestMethods::Backcompat::Schema',
    foo_class        => 'DBICTestMethods::Backcompat::Schema::Foo',
    schema_opts     => {
    },
);

# try it in backcompat mode (no naming option) but with use_namespaces => 1

run_test_sequence(
    testname        => "no naming, but with use_namespaces options (0.04006 mode)",
    schema_class    => 'DBICTestMethods::Namespaces::Schema',
    foo_class        => 'DBICTestMethods::Namespaces::Schema::Result::Foo',
    schema_opts     => {
        use_namespaces      => 1,
    },
);

sub run_test_sequence {
    my %p = @_;
    die "specify a $_ test param" for grep !$p{$_}, 
        qw( testname schema_opts schema_class foo_class );

    my $schema; 
    lives_ok { $schema = make_schema_with(%p) } "($p{testname}) get schema";

    SKIP: {
        skip 'no point in checking if schema could not be connected', 6 unless defined $schema;

        # well, if that worked, try to get a ResultSet
        my $foo_rs;
        lives_ok {
            $foo_rs = $schema->resultset('Foo')->search();
        } "($p{testname}) get a ResultSet for Foo";
    
        # get a foo
        my $foo;
        lives_ok {
            $foo = $foo_rs->first();
        } "($p{testname}) get the first foo";
    
        ok(defined $foo, "($p{testname}) \$foo is defined");
    
        SKIP: {
            skip 'foo is not defined', 3 unless defined $foo;
    
            isa_ok $foo, $p{foo_class};
    
            # call the file-defined method
            my $biz;
            lives_ok {
                $biz = $foo->biz();
            } "($p{testname}) call the file-defined Foo->biz method";
    
            SKIP: {
                skip 'no point in checking value if method was not found', 1 unless defined $biz;
        
                ok(
                    $biz eq 'foo bar biz baz boz noz schnozz', 
                    "($p{testname}) biz() method returns correct string"
                );
            }
        }
    }
}
    
sub make_schema_with {
    my %p = @_;
    return DBIx::Class::Schema::Loader::make_schema_at(
        $p{schema_class},
        $p{schema_opts},
        [ $make_dbictest_db::dsn ],
    );
}
