use strict;
use warnings;
use Test::More tests => 10;
use DBIx::Class::Schema::Loader 'make_schema_at';
use lib 't/lib';
use make_dbictest_db;

make_schema_at(
    'DBICTest::Schema::_test_schema_base',
    {
	naming => 'current',
	schema_base_class => 'TestSchemaBaseClass',
        schema_components => ['TestSchemaComponent'],
    },
    [ $make_dbictest_db::dsn ],
);

is $TestSchemaBaseClass::test_ok, 1,
    'connected using schema_base_class';

is $DBIx::Class::TestSchemaComponent::test_component_ok, 1,
    'connected using schema_components';

# try an explicit dynamic schema

{
    package DBICTest::Schema::_test_schema_base_dynamic;
    use base 'DBIx::Class::Schema::Loader';
    our $ran_connection = 0;
    __PACKAGE__->loader_options({
        naming => 'current',
        schema_base_class => 'TestSchemaBaseClass',
        schema_components => ['TestSchemaComponent'],
    });
    # check that connection doesn't cause an infinite loop
    sub connection { my $self = shift; $ran_connection++; return $self->next::method(@_) }
}

$TestSchemaBaseClass::test_ok = 0;
$DBIx::Class::TestSchemaComponent::test_component_ok = 0;

ok(my $schema =
    DBICTest::Schema::_test_schema_base_dynamic->connect($make_dbictest_db::dsn),
    'connected dynamic schema');

is $DBICTest::Schema::_test_schema_base_dynamic::ran_connection, 1,
    'schema class connection method ran only once';

is $TestSchemaBaseClass::test_ok, 1,
    'connected using schema_base_class in dynamic schema';

is $DBIx::Class::TestSchemaComponent::test_component_ok, 1,
    'connected using schema_components in dynamic schema';

# connect a second time

$TestSchemaBaseClass::test_ok = 0;
$DBIx::Class::TestSchemaComponent::test_component_ok = 0;
$DBICTest::Schema::_test_schema_base_dynamic::ran_connection = 0;

ok($schema =
    DBICTest::Schema::_test_schema_base_dynamic->connect($make_dbictest_db::dsn),
    'connected dynamic schema a second time');

is $DBICTest::Schema::_test_schema_base_dynamic::ran_connection, 1,
'schema class connection method ran only once when connecting a second time';

is $TestSchemaBaseClass::test_ok, 1,
    'connected using schema_base_class in dynamic schema a second time';

is $DBIx::Class::TestSchemaComponent::test_component_ok, 1,
    'connected using schema_components in dynamic schema a second time';
