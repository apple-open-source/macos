package dbixcsl_common_tests;

use strict;
use warnings;

use Test::More;
use Test::Exception;
use DBIx::Class::Schema::Loader;
use Class::Unload;
use File::Path 'rmtree';
use DBI;
use Digest::MD5;
use File::Find 'find';
use Class::Unload ();
use DBIx::Class::Schema::Loader::Utils qw/dumper_squashed slurp_file/;
use List::MoreUtils 'apply';
use DBIx::Class::Schema::Loader::Optional::Dependencies ();
use Try::Tiny;
use File::Spec::Functions 'catfile';
use File::Basename 'basename';
use namespace::clean;

use dbixcsl_test_dir '$tdir';

use constant DUMP_DIR => "$tdir/common_dump";

rmtree DUMP_DIR;

use constant RESCAN_WARNINGS => qr/(?i:loader_test|LoaderTest)\d+s? has no primary key|^Dumping manual schema|^Schema dump completed|collides with an inherited method|invalidates \d+ active statement|^Bad table or view/;

# skip schema-qualified tables in the Pg tests
use constant SOURCE_DDL => qr/CREATE (?:TABLE|VIEW) (?!"dbicsl[.-]test")/i;

use constant SCHEMA_CLASS => 'DBIXCSL_Test::Schema';

use constant RESULT_NAMESPACE => [ 'MyResult', 'MyResultTwo' ];

use constant RESULTSET_NAMESPACE => [ 'MyResultSet', 'MyResultSetTwo' ];

sub new {
    my $class = shift;

    my $self;

    if( ref($_[0]) eq 'HASH') {
       my $args = shift;
       $self = { (%$args) };
    }
    else {
       $self = { @_ };
    }

    # Only MySQL uses this
    $self->{innodb} ||= '';

    # DB2 and Firebird don't support 'field type NULL'
    $self->{null} = 'NULL' unless defined $self->{null};

    $self->{verbose} = $ENV{TEST_VERBOSE} || 0;

    # Optional extra tables and tests
    $self->{extra} ||= {};

    $self->{basic_date_datatype} ||= 'DATE';

    # Not all DBS do SQL-standard CURRENT_TIMESTAMP
    $self->{default_function} ||= "current_timestamp";
    $self->{default_function_def} ||= "timestamp default $self->{default_function}";

    $self = bless $self, $class;

    $self->{preserve_case_tests_table_names} = [qw/LoaderTest40 LoaderTest41/];

    if (lc($self->{vendor}) eq 'mysql' && $^O =~ /^(?:MSWin32|cygwin)\z/) {
        $self->{preserve_case_tests_table_names} = [qw/Loader_Test40 Loader_Test41/];
    }

    $self->setup_data_type_tests;

    return $self;
}

sub skip_tests {
    my ($self, $why) = @_;

    plan skip_all => $why;
}

sub _monikerize {
    my $name = shift;
    return 'LoaderTest2X' if $name =~ /^loader_test2$/i;
    return undef;
}

sub run_tests {
    my $self = shift;

    my @connect_info;

    if ($self->{dsn}) {
        push @connect_info, [ @{$self}{qw/dsn user password connect_info_opts/ } ];
    }
    else {
        foreach my $info (@{ $self->{connect_info} || [] }) {
            push @connect_info, [ @{$info}{qw/dsn user password connect_info_opts/ } ];
        }
    }

    if ($ENV{SCHEMA_LOADER_TESTS_EXTRA_ONLY}) {
        $self->run_only_extra_tests(\@connect_info);
        return;
    }

    my $extra_count = $self->{extra}{count} || 0;

    my $col_accessor_map_tests = 5;
    my $num_rescans = 6;
    $num_rescans++ if $self->{vendor} eq 'mssql';
    $num_rescans++ if $self->{vendor} eq 'Firebird';

    plan tests => @connect_info *
        (221 + $num_rescans * $col_accessor_map_tests + $extra_count + ($self->{data_type_tests}{test_count} || 0));

    foreach my $info_idx (0..$#connect_info) {
        my $info = $connect_info[$info_idx];

        @{$self}{qw/dsn user password connect_info_opts/} = @$info;

        $self->create();

        my $schema_class = $self->setup_schema($info);
        $self->test_schema($schema_class);

        rmtree DUMP_DIR
            unless $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP} && $info_idx == $#connect_info;
    }
}

sub run_only_extra_tests {
    my ($self, $connect_info) = @_;

    plan tests => @$connect_info * (3 + ($self->{extra}{count} || 0) + ($self->{data_type_tests}{test_count} || 0));

    rmtree DUMP_DIR;

    foreach my $info_idx (0..$#$connect_info) {
        my $info = $connect_info->[$info_idx];

        @{$self}{qw/dsn user password connect_info_opts/} = @$info;

        $self->drop_extra_tables_only;

        my $dbh = $self->dbconnect(1);
        $dbh->do($_) for @{ $self->{pre_create} || [] };
        $dbh->do($_) for @{ $self->{extra}{create} || [] };

        if (not ($self->{vendor} eq 'mssql' && $dbh->{Driver}{Name} eq 'Sybase')) {
            foreach my $ddl (@{ $self->{data_type_tests}{ddl} || []}) {
                if (my $cb = $self->{data_types_ddl_cb}) {
                    $cb->($ddl);
                }
                else {
                    $dbh->do($ddl);
                }
            }
        }

        $self->{_created} = 1;

        my $file_count = grep $_ =~ SOURCE_DDL, @{ $self->{extra}{create} || [] };
        $file_count++; # schema

        if (not ($self->{vendor} eq 'mssql' && $dbh->{Driver}{Name} eq 'Sybase')) {
            $file_count++ for @{ $self->{data_type_tests}{table_names} || [] };
        }

        my $schema_class = $self->setup_schema($info, $file_count);
        my ($monikers, $classes) = $self->monikers_and_classes($schema_class);
        my $conn = $schema_class->clone;

        $self->test_data_types($conn);
        $self->{extra}{run}->($conn, $monikers, $classes, $self) if $self->{extra}{run};

        if (not ($ENV{SCHEMA_LOADER_TESTS_NOCLEANUP} && $info_idx == $#$connect_info)) {
            $self->drop_extra_tables_only;
            rmtree DUMP_DIR;
        }
    }
}

sub drop_extra_tables_only {
    my $self = shift;

    my $dbh = $self->dbconnect(0);

    local $^W = 0; # for ADO

    $dbh->do($_) for @{ $self->{extra}{pre_drop_ddl} || [] };
    $self->drop_table($dbh, $_) for @{ $self->{extra}{drop} || [] };

    if (not ($self->{vendor} eq 'mssql' && $dbh->{Driver}{Name} eq 'Sybase')) {
        foreach my $data_type_table (@{ $self->{data_type_tests}{table_names} || [] }) {
            $self->drop_table($dbh, $data_type_table);
        }
    }
}

# defined in sub create
my (@statements, @statements_reltests, @statements_advanced,
    @statements_advanced_sqlite, @statements_inline_rels,
    @statements_implicit_rels);

sub CONSTRAINT {
    my $self = shift;
return qr/^(?:\S+\.)?(?:(?:$self->{vendor}|extra)[_-]?)?loader[_-]?test[0-9]+(?!.*_)/i;
}

sub setup_schema {
    my ($self, $connect_info, $expected_count) = @_;

    my $debug = ($self->{verbose} > 1) ? 1 : 0;

    if ($ENV{SCHEMA_LOADER_TESTS_USE_MOOSE}) {
        if (not DBIx::Class::Schema::Loader::Optional::Dependencies->req_ok_for('use_moose')) {
            die sprintf ("Missing dependencies for SCHEMA_LOADER_TESTS_USE_MOOSE: %s\n",
                DBIx::Class::Schema::Loader::Optional::Dependencies->req_missing_for('use_moose'));
        }

        $self->{use_moose} = 1;
    }

    my %loader_opts = (
        constraint              => $self->CONSTRAINT,
        result_namespace        => RESULT_NAMESPACE,
        resultset_namespace     => RESULTSET_NAMESPACE,
        schema_base_class       => 'TestSchemaBaseClass',
        schema_components       => [ 'TestSchemaComponent', '+TestSchemaComponentFQN' ],
        additional_classes      => 'TestAdditional',
        additional_base_classes => 'TestAdditionalBase',
        left_base_classes       => [ qw/TestLeftBase/ ],
        components              => [ qw/TestComponent +TestComponentFQN IntrospectableM2M/ ],
        inflect_plural          => { loader_test4_fkid => 'loader_test4zes' },
        inflect_singular        => { fkid => 'fkid_singular' },
        moniker_map             => \&_monikerize,
        custom_column_info      => \&_custom_column_info,
        debug                   => $debug,
        dump_directory          => DUMP_DIR,
        datetime_timezone       => 'Europe/Berlin',
        datetime_locale         => 'de_DE',
        $self->{use_moose} ? (
            use_moose        => 1,
            result_roles     => 'TestRole',
            result_roles_map => { LoaderTest2X => 'TestRoleForMap' },
        ) : (),
        col_collision_map       => { '^(can)\z' => 'caught_collision_%s' },
        rel_collision_map       => { '^(set_primary_key)\z' => 'caught_rel_collision_%s' },
        col_accessor_map        => \&test_col_accessor_map,
        result_components_map   => { LoaderTest2X => 'TestComponentForMap', LoaderTest1 => '+TestComponentForMapFQN' },
        uniq_to_primary         => 1,
        %{ $self->{loader_options} || {} },
    );

    $loader_opts{db_schema} = $self->{db_schema} if $self->{db_schema};

    Class::Unload->unload(SCHEMA_CLASS);

    my $file_count;
    {
        my @loader_warnings;
        local $SIG{__WARN__} = sub { push(@loader_warnings, @_); };
         eval qq{
             package @{[SCHEMA_CLASS]};
             use base qw/DBIx::Class::Schema::Loader/;

             __PACKAGE__->loader_options(\%loader_opts);
             __PACKAGE__->connection(\@\$connect_info);
         };

        ok(!$@, "Loader initialization") or diag $@;

        find sub { return if -d; $file_count++ }, DUMP_DIR;

        my $standard_sources = not defined $expected_count;

        if ($standard_sources) {
            $expected_count = 37;

            if (not ($self->{vendor} eq 'mssql' && $connect_info->[0] =~ /Sybase/)) {
                $expected_count++ for @{ $self->{data_type_tests}{table_names} || [] };
            }

            $expected_count += grep $_ =~ SOURCE_DDL,
                @{ $self->{extra}{create} || [] };

            $expected_count -= grep /CREATE TABLE/i, @statements_inline_rels
                if $self->{skip_rels} || $self->{no_inline_rels};

            $expected_count -= grep /CREATE TABLE/i, @statements_implicit_rels
                if $self->{skip_rels} || $self->{no_implicit_rels};

            $expected_count -= grep /CREATE TABLE/i, ($self->{vendor} =~ /sqlite/ ? @statements_advanced_sqlite : @statements_advanced), @statements_reltests
                if $self->{skip_rels};
        }

        is $file_count, $expected_count, 'correct number of files generated';

        my $warn_count = 2;

        $warn_count++ for grep /^Bad table or view/, @loader_warnings;

        $warn_count++ for grep /renaming \S+ relation/, @loader_warnings;

        $warn_count++ for grep /\b(?!loader_test9)\w+ has no primary key/i, @loader_warnings;

        $warn_count++ for grep /^Column '\w+' in table '\w+' collides with an inherited method\./, @loader_warnings;

        $warn_count++ for grep /^Relationship '\w+' in source '\w+' for columns '[^']+' collides with an inherited method\./, @loader_warnings;

        $warn_count++ for grep { my $w = $_; grep $w =~ $_, @{ $self->{warnings} || [] } } @loader_warnings;

        $warn_count-- for grep { my $w = $_; grep $w =~ $_, @{ $self->{failtrigger_warnings} || [] } } @loader_warnings;

        is scalar(@loader_warnings), $warn_count, 'Correct number of warnings'
            or diag @loader_warnings;
    }

    exit if ($file_count||0) != $expected_count;

    return SCHEMA_CLASS;
}

sub test_schema {
    my $self = shift;
    my $schema_class = shift;

    my $conn = $schema_class->clone;

    ($self->{before_tests_run} || sub {})->($conn);

    my ($monikers, $classes) = $self->monikers_and_classes($schema_class);

    my $moniker1 = $monikers->{loader_test1s};
    my $class1   = $classes->{loader_test1s};
    my $rsobj1   = $conn->resultset($moniker1);
    check_no_duplicate_unique_constraints($class1);

    my $moniker2 = $monikers->{loader_test2};
    my $class2   = $classes->{loader_test2};
    my $rsobj2   = $conn->resultset($moniker2);
    check_no_duplicate_unique_constraints($class2);

    my $moniker23 = $monikers->{LOADER_test23} || $monikers->{loader_test23};
    my $class23   = $classes->{LOADER_test23}  || $classes->{loader_test23};
    my $rsobj23   = $conn->resultset($moniker1);

    my $moniker24 = $monikers->{LoAdEr_test24} || $monikers->{loader_test24};
    my $class24   = $classes->{LoAdEr_test24}  || $classes->{loader_test24};
    my $rsobj24   = $conn->resultset($moniker2);

    my $moniker35 = $monikers->{loader_test35};
    my $class35   = $classes->{loader_test35};
    my $rsobj35   = $conn->resultset($moniker35);

    my $moniker50 = $monikers->{loader_test50};
    my $class50   = $classes->{loader_test50};
    my $rsobj50   = $conn->resultset($moniker50);

    isa_ok( $rsobj1, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj2, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj23, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj24, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj35, "DBIx::Class::ResultSet" );
    isa_ok( $rsobj50, "DBIx::Class::ResultSet" );

    # check result_namespace
    my @schema_dir = split /::/, SCHEMA_CLASS;
    my $result_dir = ref RESULT_NAMESPACE ? ${RESULT_NAMESPACE()}[0] : RESULT_NAMESPACE;

    my $schema_files = [ sort map basename($_), glob catfile(DUMP_DIR, @schema_dir, '*') ];

    is_deeply $schema_files, [ $result_dir ],
        'first entry in result_namespace exists as a directory';

    my $result_file_count =()= glob catfile(DUMP_DIR, @schema_dir, $result_dir, '*.pm');

    ok $result_file_count,
        'Result files dumped to first entry in result_namespace';

    # parse out the resultset_namespace
    my $schema_code = slurp_file $conn->_loader->get_dump_filename(SCHEMA_CLASS);

    my ($schema_resultset_namespace) = $schema_code =~ /\bresultset_namespace => (.*)/;
    $schema_resultset_namespace = eval $schema_resultset_namespace;
    die $@ if $@;

    is_deeply $schema_resultset_namespace, RESULTSET_NAMESPACE,
        'resultset_namespace set correctly on Schema';

    like $schema_code,
qr/\nuse base 'TestSchemaBaseClass';\n\n|\nextends 'TestSchemaBaseClass';\n\n/,
        'schema_base_class works';

    is $conn->testschemabaseclass, 'TestSchemaBaseClass works',
        'schema base class works';

    like $schema_code,
qr/\n__PACKAGE__->load_components\("TestSchemaComponent", "\+TestSchemaComponentFQN"\);\n\n__PACKAGE__->load_namespaces/,
        'schema_components works';

    is $conn->dbix_class_testschemacomponent, 'dbix_class_testschemacomponent works',
        'schema component works';

    is $conn->testschemacomponent_fqn, 'TestSchemaComponentFQN works',
        'fully qualified schema component works';

    my @columns_lt2 = $class2->columns;
    is_deeply( \@columns_lt2, [ qw/id dat dat2 set_primary_key can dbix_class_testcomponent dbix_class_testcomponentmap testcomponent_fqn meta test_role_method test_role_for_map_method crumb_crisp_coating/ ], "Column Ordering" );

    is $class2->column_info('can')->{accessor}, 'caught_collision_can',
        'accessor for column name that conflicts with a UNIVERSAL method renamed based on col_collision_map';

    ok (exists $class2->column_info('set_primary_key')->{accessor}
        && (not defined $class2->column_info('set_primary_key')->{accessor}),
        'accessor for column name that conflicts with a result base class method removed');

    ok (exists $class2->column_info('dbix_class_testcomponent')->{accessor}
        && (not defined $class2->column_info('dbix_class_testcomponent')->{accessor}),
        'accessor for column name that conflicts with a component class method removed');

    ok (exists $class2->column_info('dbix_class_testcomponentmap')->{accessor}
        && (not defined $class2->column_info('dbix_class_testcomponentmap')->{accessor}),
        'accessor for column name that conflicts with a component class method removed');

    ok (exists $class2->column_info('testcomponent_fqn')->{accessor}
        && (not defined $class2->column_info('testcomponent_fqn')->{accessor}),
        'accessor for column name that conflicts with a fully qualified component class method removed');

    if ($self->{use_moose}) {
        ok (exists $class2->column_info('meta')->{accessor}
            && (not defined $class2->column_info('meta')->{accessor}),
            'accessor for column name that conflicts with Moose removed');

        ok (exists $class2->column_info('test_role_for_map_method')->{accessor}
            && (not defined $class2->column_info('test_role_for_map_method')->{accessor}),
            'accessor for column name that conflicts with a Result role removed');

        ok (exists $class2->column_info('test_role_method')->{accessor}
            && (not defined $class2->column_info('test_role_method')->{accessor}),
            'accessor for column name that conflicts with a Result role removed');
    }
    else {
        ok ((not exists $class2->column_info('meta')->{accessor}),
            "not removing 'meta' accessor with use_moose disabled");

        ok ((not exists $class2->column_info('test_role_for_map_method')->{accessor}),
            'no role method conflicts with use_moose disabled');

        ok ((not exists $class2->column_info('test_role_method')->{accessor}),
            'no role method conflicts with use_moose disabled');
    }

    my %uniq1 = $class1->unique_constraints;
    my $uniq1_test = 0;
    foreach my $ucname (keys %uniq1) {
        my $cols_arrayref = $uniq1{$ucname};
        if(@$cols_arrayref == 1 && $cols_arrayref->[0] eq 'dat') {
           $uniq1_test = 1;
           last;
        }
    }
    ok($uniq1_test, "Unique constraint");

    is($moniker1, 'LoaderTest1', 'moniker singularisation');

    my %uniq2 = $class2->unique_constraints;
    my $uniq2_test = 0;
    foreach my $ucname (keys %uniq2) {
        my $cols_arrayref = $uniq2{$ucname};
        if(@$cols_arrayref == 2
           && $cols_arrayref->[0] eq 'dat2'
           && $cols_arrayref->[1] eq 'dat') {
            $uniq2_test = 2;
            last;
        }
    }
    ok($uniq2_test, "Multi-col unique constraint");

    my %uniq3 = $class50->unique_constraints;

    is_deeply $uniq3{primary}, ['id1', 'id2'],
        'unique constraint promoted to primary key with uniq_to_primary';

    is($moniker2, 'LoaderTest2X', "moniker_map testing");

    SKIP: {
        can_ok( $class1, 'test_additional_base' )
            or skip "Pre-requisite test failed", 1;
        is( $class1->test_additional_base, "test_additional_base",
            "Additional Base method" );
    }

    SKIP: {
        can_ok( $class1, 'test_additional_base_override' )
            or skip "Pre-requisite test failed", 1;
        is( $class1->test_additional_base_override,
            "test_left_base_override",
            "Left Base overrides Additional Base method" );
    }

    SKIP: {
        can_ok( $class1, 'test_additional_base_additional' )
            or skip "Pre-requisite test failed", 1;
        is( $class1->test_additional_base_additional, "test_additional",
            "Additional Base can use Additional package method" );
    }

    SKIP: {
        can_ok( $class1, 'dbix_class_testcomponent' )
            or skip "Pre-requisite test failed", 1;
        is( $class1->dbix_class_testcomponent,
            'dbix_class_testcomponent works',
            'Additional Component' );
    }

    is try { $class2->dbix_class_testcomponentmap }, 'dbix_class_testcomponentmap works',
        'component from result_component_map';

    isnt try { $class1->dbix_class_testcomponentmap }, 'dbix_class_testcomponentmap works',
        'component from result_component_map not added to not mapped Result';

    is try { $class1->testcomponent_fqn }, 'TestComponentFQN works',
        'fully qualified component class';

    is try { $class1->testcomponentformap_fqn }, 'TestComponentForMapFQN works',
        'fully qualified component class from result_component_map';

    isnt try { $class2->testcomponentformap_fqn }, 'TestComponentForMapFQN works',
        'fully qualified component class from result_component_map not added to not mapped Result';

    SKIP: {
        skip 'not testing role methods with use_moose disabled', 2
            unless $self->{use_moose};

        is try { $class1->test_role_method }, 'test_role_method works',
            'role from result_roles applied';

        is try { $class2->test_role_for_map_method },
            'test_role_for_map_method works',
            'role from result_roles_map applied';
    }

    SKIP: {
        can_ok( $class1, 'loader_test1_classmeth' )
            or skip "Pre-requisite test failed", 1;
        is( $class1->loader_test1_classmeth, 'all is well', 'Class method' );
    }

    ok( $class1->column_info('id')->{is_auto_increment}, 'is_auto_increment detection' );

    my $obj = try { $rsobj1->find(1) };

    is( try { $obj->id },  1, "Find got the right row" );
    is( try { $obj->dat }, "foo", "Column value" );
    is( $rsobj2->count, 4, "Count" );
    my $saved_id;
    eval {
        my $new_obj1 = $rsobj1->create({ dat => 'newthing' });
        $saved_id = $new_obj1->id;
    };
    ok(!$@, "Inserting new record using a PK::Auto key didn't die") or diag $@;
    ok($saved_id, "Got PK::Auto-generated id");

    my $new_obj1 = $rsobj1->search({ dat => 'newthing' })->single;
    ok($new_obj1, "Found newly inserted PK::Auto record");
    is($new_obj1->id, $saved_id, "Correct PK::Auto-generated id");

    my ($obj2) = $rsobj2->search({ dat => 'bbb' })->single;
    is( $obj2->id, 2 );

    SKIP: {
        skip 'no DEFAULT on Access', 7 if $self->{vendor} eq 'Access';

        is(
            $class35->column_info('a_varchar')->{default_value}, 'foo',
            'constant character default',
        );

        is(
            $class35->column_info('an_int')->{default_value}, 42,
            'constant integer default',
        );

        is(
            $class35->column_info('a_negative_int')->{default_value}, -42,
            'constant negative integer default',
        );

        is(
            sprintf("%.3f", $class35->column_info('a_double')->{default_value}||0), '10.555',
            'constant numeric default',
        );

        is(
            sprintf("%.3f", $class35->column_info('a_negative_double')->{default_value}||0), -10.555,
            'constant negative numeric default',
        );

        my $function_default = $class35->column_info('a_function')->{default_value};

        isa_ok( $function_default, 'SCALAR', 'default_value for function default' );
        is_deeply(
            $function_default, \$self->{default_function},
            'default_value for function default is correct'
        );
    }

    is( $class2->column_info('crumb_crisp_coating')->{accessor},  'trivet',
        'col_accessor_map is being run' );

    is $class1->column_info('dat')->{is_nullable}, 0,
        'is_nullable=0 detection';

    is $class2->column_info('set_primary_key')->{is_nullable}, 1,
        'is_nullable=1 detection';

    SKIP: {
        skip $self->{skip_rels}, 137 if $self->{skip_rels};

        my $moniker3 = $monikers->{loader_test3};
        my $class3   = $classes->{loader_test3};
        my $rsobj3   = $conn->resultset($moniker3);

        my $moniker4 = $monikers->{loader_test4};
        my $class4   = $classes->{loader_test4};
        my $rsobj4   = $conn->resultset($moniker4);

        my $moniker5 = $monikers->{loader_test5};
        my $class5   = $classes->{loader_test5};
        my $rsobj5   = $conn->resultset($moniker5);

        my $moniker6 = $monikers->{loader_test6};
        my $class6   = $classes->{loader_test6};
        my $rsobj6   = $conn->resultset($moniker6);

        my $moniker7 = $monikers->{loader_test7};
        my $class7   = $classes->{loader_test7};
        my $rsobj7   = $conn->resultset($moniker7);

        my $moniker8 = $monikers->{loader_test8};
        my $class8   = $classes->{loader_test8};
        my $rsobj8   = $conn->resultset($moniker8);

        my $moniker9 = $monikers->{loader_test9};
        my $class9   = $classes->{loader_test9};
        my $rsobj9   = $conn->resultset($moniker9);

        my $moniker16 = $monikers->{loader_test16};
        my $class16   = $classes->{loader_test16};
        my $rsobj16   = $conn->resultset($moniker16);

        my $moniker17 = $monikers->{loader_test17};
        my $class17   = $classes->{loader_test17};
        my $rsobj17   = $conn->resultset($moniker17);

        my $moniker18 = $monikers->{loader_test18};
        my $class18   = $classes->{loader_test18};
        my $rsobj18   = $conn->resultset($moniker18);

        my $moniker19 = $monikers->{loader_test19};
        my $class19   = $classes->{loader_test19};
        my $rsobj19   = $conn->resultset($moniker19);

        my $moniker20 = $monikers->{loader_test20};
        my $class20   = $classes->{loader_test20};
        my $rsobj20   = $conn->resultset($moniker20);

        my $moniker21 = $monikers->{loader_test21};
        my $class21   = $classes->{loader_test21};
        my $rsobj21   = $conn->resultset($moniker21);

        my $moniker22 = $monikers->{loader_test22};
        my $class22   = $classes->{loader_test22};
        my $rsobj22   = $conn->resultset($moniker22);

        my $moniker25 = $monikers->{loader_test25};
        my $class25   = $classes->{loader_test25};
        my $rsobj25   = $conn->resultset($moniker25);

        my $moniker26 = $monikers->{loader_test26};
        my $class26   = $classes->{loader_test26};
        my $rsobj26   = $conn->resultset($moniker26);

        my $moniker27 = $monikers->{loader_test27};
        my $class27   = $classes->{loader_test27};
        my $rsobj27   = $conn->resultset($moniker27);

        my $moniker28 = $monikers->{loader_test28};
        my $class28   = $classes->{loader_test28};
        my $rsobj28   = $conn->resultset($moniker28);

        my $moniker29 = $monikers->{loader_test29};
        my $class29   = $classes->{loader_test29};
        my $rsobj29   = $conn->resultset($moniker29);

        my $moniker31 = $monikers->{loader_test31};
        my $class31   = $classes->{loader_test31};
        my $rsobj31   = $conn->resultset($moniker31);

        my $moniker32 = $monikers->{loader_test32};
        my $class32   = $classes->{loader_test32};
        my $rsobj32   = $conn->resultset($moniker32);

        my $moniker33 = $monikers->{loader_test33};
        my $class33   = $classes->{loader_test33};
        my $rsobj33   = $conn->resultset($moniker33);

        my $moniker34 = $monikers->{loader_test34};
        my $class34   = $classes->{loader_test34};
        my $rsobj34   = $conn->resultset($moniker34);

        my $moniker36 = $monikers->{loader_test36};
        my $class36   = $classes->{loader_test36};
        my $rsobj36   = $conn->resultset($moniker36);

        isa_ok( $rsobj3, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj4, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj5, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj6, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj7, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj8, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj9, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj16, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj17, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj18, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj19, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj20, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj21, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj22, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj25, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj26, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj27, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj28, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj29, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj31, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj32, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj33, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj34, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj36, "DBIx::Class::ResultSet" );

        # basic rel test
        my $obj4 = try { $rsobj4->find(123) } || $rsobj4->search({ id => 123 })->single;
        isa_ok( try { $obj4->fkid_singular }, $class3);

        # test renaming rel that conflicts with a class method
        ok ($obj4->has_relationship('belongs_to_rel'), 'relationship name that conflicts with a method renamed');

        isa_ok( try { $obj4->belongs_to_rel }, $class3);

        ok ($obj4->has_relationship('caught_rel_collision_set_primary_key'),
            'relationship name that conflicts with a method renamed based on rel_collision_map');
        isa_ok( try { $obj4->caught_rel_collision_set_primary_key }, $class3);

        ok($class4->column_info('fkid')->{is_foreign_key}, 'Foreign key detected');

        my $obj3 = try { $rsobj3->find(1) } || $rsobj3->search({ id => 1 })->single;
        my $rs_rel4 = try { $obj3->search_related('loader_test4zes') };
        isa_ok( try { $rs_rel4->single }, $class4);

        # check rel naming with prepositions
        ok ($rsobj4->result_source->has_relationship('loader_test5s_to'),
            "rel with preposition 'to' pluralized correctly");

        ok ($rsobj4->result_source->has_relationship('loader_test5s_from'),
            "rel with preposition 'from' pluralized correctly");

        # check default relationship attributes
        is try { $rsobj3->result_source->relationship_info('loader_test4zes')->{attrs}{cascade_delete} }, 0,
            'cascade_delete => 0 on has_many by default';

        is try { $rsobj3->result_source->relationship_info('loader_test4zes')->{attrs}{cascade_copy} }, 0,
            'cascade_copy => 0 on has_many by default';

        ok ((not try { exists $rsobj3->result_source->relationship_info('loader_test4zes')->{attrs}{on_delete} }),
            'has_many does not have on_delete');

        ok ((not try { exists $rsobj3->result_source->relationship_info('loader_test4zes')->{attrs}{on_update} }),
            'has_many does not have on_update');

        ok ((not try { exists $rsobj3->result_source->relationship_info('loader_test4zes')->{attrs}{is_deferrable} }),
            'has_many does not have is_deferrable');

        my $default_on_clause = $self->{default_on_clause} || 'CASCADE';

        my $default_on_delete_clause = $self->{default_on_delete_clause} || $default_on_clause;

        is try { $rsobj4->result_source->relationship_info('fkid_singular')->{attrs}{on_delete} },
            $default_on_delete_clause,
            "on_delete is $default_on_delete_clause on belongs_to by default";

        my $default_on_update_clause = $self->{default_on_update_clause} || $default_on_clause;

        is try { $rsobj4->result_source->relationship_info('fkid_singular')->{attrs}{on_update} },
            $default_on_update_clause,
            "on_update is $default_on_update_clause on belongs_to by default";

        my $default_is_deferrable = $self->{default_is_deferrable};

        $default_is_deferrable = 1
            if not defined $default_is_deferrable;

        is try { $rsobj4->result_source->relationship_info('fkid_singular')->{attrs}{is_deferrable} },
            $default_is_deferrable,
            "is_deferrable => $default_is_deferrable on belongs_to by default";

        ok ((not try { exists $rsobj4->result_source->relationship_info('fkid_singular')->{attrs}{cascade_delete} }),
            'belongs_to does not have cascade_delete');

        ok ((not try { exists $rsobj4->result_source->relationship_info('fkid_singular')->{attrs}{cascade_copy} }),
            'belongs_to does not have cascade_copy');

        is try { $rsobj27->result_source->relationship_info('loader_test28')->{attrs}{cascade_delete} }, 0,
            'cascade_delete => 0 on might_have by default';

        is try { $rsobj27->result_source->relationship_info('loader_test28')->{attrs}{cascade_copy} }, 0,
            'cascade_copy => 0 on might_have by default';

        ok ((not try { exists $rsobj27->result_source->relationship_info('loader_test28')->{attrs}{on_delete} }),
            'might_have does not have on_delete');

        ok ((not try { exists $rsobj27->result_source->relationship_info('loader_test28')->{attrs}{on_update} }),
            'might_have does not have on_update');

        ok ((not try { exists $rsobj27->result_source->relationship_info('loader_test28')->{attrs}{is_deferrable} }),
            'might_have does not have is_deferrable');

        # find on multi-col pk
        if ($conn->loader->preserve_case) {
            my $obj5 = $rsobj5->find({id1 => 1, iD2 => 1});
            is $obj5->i_d2, 1, 'Find on multi-col PK';
        }
        else {
	    my $obj5 = $rsobj5->find({id1 => 1, id2 => 1});
            is $obj5->id2, 1, 'Find on multi-col PK';
        }

        # mulit-col fk def
        my $obj6 = try { $rsobj6->find(1) } || $rsobj6->search({ id => 1 })->single;
        isa_ok( try { $obj6->loader_test2 }, $class2);
        isa_ok( try { $obj6->loader_test5 }, $class5);

        ok($class6->column_info('loader_test2_id')->{is_foreign_key}, 'Foreign key detected');
        ok($class6->column_info('id')->{is_foreign_key}, 'Foreign key detected');

	my $id2_info = try { $class6->column_info('id2') } ||
			$class6->column_info('Id2');
        ok($id2_info->{is_foreign_key}, 'Foreign key detected');

        unlike slurp_file $conn->_loader->get_dump_filename($class6),
qr/\n__PACKAGE__->(?:belongs_to|has_many|might_have|has_one|many_to_many)\(
    \s+ "(\w+?)"
    .*?
   \n__PACKAGE__->(?:belongs_to|has_many|might_have|has_one|many_to_many)\(
    \s+ "\1"/xs,
'did not create two relationships with the same name';

        unlike slurp_file $conn->_loader->get_dump_filename($class8),
qr/\n__PACKAGE__->(?:belongs_to|has_many|might_have|has_one|many_to_many)\(
    \s+ "(\w+?)"
    .*?
   \n__PACKAGE__->(?:belongs_to|has_many|might_have|has_one|many_to_many)\(
    \s+ "\1"/xs,
'did not create two relationships with the same name';

        # check naming of ambiguous relationships
        my $rel_info = $class6->relationship_info('lovely_loader_test7') || {};

        ok (($class6->has_relationship('lovely_loader_test7')
            && $rel_info->{cond}{'foreign.lovely_loader_test6'} eq 'self.id'
            && $rel_info->{class} eq $class7
            && $rel_info->{attrs}{accessor} eq 'single'),
            'ambiguous relationship named correctly');

        $rel_info = $class8->relationship_info('active_loader_test16') || {};

        ok (($class8->has_relationship('active_loader_test16')
            && $rel_info->{cond}{'foreign.loader_test8_id'} eq 'self.id'
            && $rel_info->{class} eq $class16
            && $rel_info->{attrs}{accessor} eq 'single'),
            'ambiguous relationship named correctly');

        # fk that references a non-pk key (UNIQUE)
        my $obj8 = try { $rsobj8->find(1) } || $rsobj8->search({ id => 1 })->single;
        isa_ok( try { $obj8->loader_test7 }, $class7);

        ok($class8->column_info('loader_test7')->{is_foreign_key}, 'Foreign key detected');

        # test double-fk 17 ->-> 16
        my $obj17 = try { $rsobj17->find(33) } || $rsobj17->search({ id => 33 })->single;

        my $rs_rel16_one = try { $obj17->loader16_one };
        isa_ok($rs_rel16_one, $class16);
        is(try { $rs_rel16_one->dat }, 'y16', "Multiple FKs to same table");

        ok($class17->column_info('loader16_one')->{is_foreign_key}, 'Foreign key detected');

        my $rs_rel16_two = try { $obj17->loader16_two };
        isa_ok($rs_rel16_two, $class16);
        is(try { $rs_rel16_two->dat }, 'z16', "Multiple FKs to same table");

        ok($class17->column_info('loader16_two')->{is_foreign_key}, 'Foreign key detected');

        my $obj16 = try { $rsobj16->find(2) } || $rsobj16->search({ id => 2 })->single;
        my $rs_rel17 = try { $obj16->search_related('loader_test17_loader16_ones') };
        isa_ok(try { $rs_rel17->single }, $class17);
        is(try { $rs_rel17->single->id }, 3, "search_related with multiple FKs from same table");

        # XXX test m:m 18 <- 20 -> 19
        ok($class20->column_info('parent')->{is_foreign_key}, 'Foreign key detected');
        ok($class20->column_info('child')->{is_foreign_key}, 'Foreign key detected');

        # XXX test double-fk m:m 21 <- 22 -> 21
        ok($class22->column_info('parent')->{is_foreign_key}, 'Foreign key detected');
        ok($class22->column_info('child')->{is_foreign_key}, 'Foreign key detected');

        # test many_to_many detection 18 -> 20 -> 19 and 19 -> 20 -> 18
        my $m2m;

        ok($m2m = (try { $class18->_m2m_metadata->{children} }), 'many_to_many created');

        is $m2m->{relation}, 'loader_test20s', 'm2m near rel';
        is $m2m->{foreign_relation}, 'child', 'm2m far rel';

        ok($m2m = (try { $class19->_m2m_metadata->{parents} }), 'many_to_many created');

        is $m2m->{relation}, 'loader_test20s', 'm2m near rel';
        is $m2m->{foreign_relation}, 'parent', 'm2m far rel';

        # test double multi-col fk 26 -> 25
        my $obj26 = try { $rsobj26->find(33) } || $rsobj26->search({ id => 33 })->single;

        my $rs_rel25_one = try { $obj26->loader_test25_id_rel1 };
        isa_ok($rs_rel25_one, $class25);
        is(try { $rs_rel25_one->dat }, 'x25', "Multiple multi-col FKs to same table");

        ok($class26->column_info('id')->{is_foreign_key}, 'Foreign key detected');
        ok($class26->column_info('rel1')->{is_foreign_key}, 'Foreign key detected');
        ok($class26->column_info('rel2')->{is_foreign_key}, 'Foreign key detected');

        my $rs_rel25_two = try { $obj26->loader_test25_id_rel2 };
        isa_ok($rs_rel25_two, $class25);
        is(try { $rs_rel25_two->dat }, 'y25', "Multiple multi-col FKs to same table");

        my $obj25 = try { $rsobj25->find(3,42) } || $rsobj25->search({ id1 => 3, id2 => 42 })->single;
        my $rs_rel26 = try { $obj25->search_related('loader_test26_id_rel1s') };
        isa_ok(try { $rs_rel26->single }, $class26);
        is(try { $rs_rel26->single->id }, 3, "search_related with multiple multi-col FKs from same table");

        # test one-to-one rels
        my $obj27 = try { $rsobj27->find(1) } || $rsobj27->search({ id => 1 })->single;
        my $obj28 = try { $obj27->loader_test28 };
        isa_ok($obj28, $class28);
        is(try { $obj28->get_column('id') }, 1, "One-to-one relationship with PRIMARY FK");

        ok($class28->column_info('id')->{is_foreign_key}, 'Foreign key detected');

        my $obj29 = try { $obj27->loader_test29 };
        isa_ok($obj29, $class29);
        is(try { $obj29->id }, 1, "One-to-one relationship with UNIQUE FK");

        ok($class29->column_info('fk')->{is_foreign_key}, 'Foreign key detected');

        $obj27 = try { $rsobj27->find(2) } || $rsobj27->search({ id => 2 })->single;
        is(try { $obj27->loader_test28 }, undef, "Undef for missing one-to-one row");
        is(try { $obj27->loader_test29 }, undef, "Undef for missing one-to-one row");

        # test outer join for nullable referring columns:
        is $class32->column_info('rel2')->{is_nullable}, 1,
          'is_nullable detection';

        ok($class32->column_info('rel1')->{is_foreign_key}, 'Foreign key detected');
        ok($class32->column_info('rel2')->{is_foreign_key}, 'Foreign key detected');

        my $obj32 = try { $rsobj32->find(1, { prefetch => [qw/rel1 rel2/] }) }
            || try { $rsobj32->search({ id => 1 }, { prefetch => [qw/rel1 rel2/] })->single }
            || $rsobj32->search({ id => 1 })->single;

        my $obj34 = eval { $rsobj34->find(1, { prefetch => [qw/loader_test33_id_rel1 loader_test33_id_rel2/] }) }
            || eval { $rsobj34->search({ id => 1 }, { prefetch => [qw/loader_test33_id_rel1 loader_test33_id_rel2/] })->single }
            || $rsobj34->search({ id => 1 })->single;
        diag $@ if $@;

        isa_ok($obj32,$class32);
        isa_ok($obj34,$class34);

        ok($class34->column_info('id')->{is_foreign_key}, 'Foreign key detected');
        ok($class34->column_info('rel1')->{is_foreign_key}, 'Foreign key detected');
        ok($class34->column_info('rel2')->{is_foreign_key}, 'Foreign key detected');

        my $rs_rel31_one = try { $obj32->rel1 };
        my $rs_rel31_two = try { $obj32->rel2 };
        isa_ok($rs_rel31_one, $class31);
        is($rs_rel31_two, undef);

        my $rs_rel33_one = try { $obj34->loader_test33_id_rel1 };
        my $rs_rel33_two = try { $obj34->loader_test33_id_rel2 };

        isa_ok($rs_rel33_one, $class33);
        isa_ok($rs_rel33_two, $class33);

        # from Chisel's tests...
        my $moniker10 = $monikers->{loader_test10};
        my $class10   = $classes->{loader_test10};
        my $rsobj10   = $conn->resultset($moniker10);

        my $moniker11 = $monikers->{loader_test11};
        my $class11   = $classes->{loader_test11};
        my $rsobj11   = $conn->resultset($moniker11);

        isa_ok( $rsobj10, "DBIx::Class::ResultSet" );
        isa_ok( $rsobj11, "DBIx::Class::ResultSet" );

        ok($class10->column_info('loader_test11')->{is_foreign_key}, 'Foreign key detected');
        ok($class11->column_info('loader_test10')->{is_foreign_key}, 'Foreign key detected');

        my $obj10 = $rsobj10->create({ subject => 'xyzzy' });

        $obj10->update();
        ok( defined $obj10, 'Create row' );

        my $obj11 = $rsobj11->create({ loader_test10 => (try { $obj10->id() } || $obj10->id10) });
        $obj11->update();
        ok( defined $obj11, 'Create related row' );

        eval {
            my $obj10_2 = $obj11->loader_test10;
            $obj10_2->update({ loader_test11 => $obj11->id11 });
        };
        diag $@ if $@;
        ok(!$@, "Setting up circular relationship");

        SKIP: {
            skip 'Previous eval block failed', 3 if $@;

            my $results = $rsobj10->search({ subject => 'xyzzy' });
            is( $results->count(), 1, 'No duplicate row created' );

            my $obj10_3 = $results->single();
            isa_ok( $obj10_3, $class10 );
            is( $obj10_3->loader_test11()->id(), $obj11->id(),
                'Circular rel leads back to same row' );
        }

        SKIP: {
            skip 'This vendor cannot do inline relationship definitions', 9
                if $self->{no_inline_rels};

            my $moniker12 = $monikers->{loader_test12};
            my $class12   = $classes->{loader_test12};
            my $rsobj12   = $conn->resultset($moniker12);

            my $moniker13 = $monikers->{loader_test13};
            my $class13   = $classes->{loader_test13};
            my $rsobj13   = $conn->resultset($moniker13);

            isa_ok( $rsobj12, "DBIx::Class::ResultSet" );
            isa_ok( $rsobj13, "DBIx::Class::ResultSet" );

            ok($class13->column_info('id')->{is_foreign_key}, 'Foreign key detected');
            ok($class13->column_info('loader_test12')->{is_foreign_key}, 'Foreign key detected');
            ok($class13->column_info('dat')->{is_foreign_key}, 'Foreign key detected');

            my $obj13 = try { $rsobj13->find(1) } || $rsobj13->search({ id => 1 })->single;
            isa_ok( $obj13->id, $class12 );
            isa_ok( $obj13->loader_test12, $class12);
            isa_ok( $obj13->dat, $class12);

            my $obj12 = try { $rsobj12->find(1) } || $rsobj12->search({ id => 1 })->single;
            isa_ok( try { $obj12->loader_test13 }, $class13 );
        }

        # relname is preserved when another fk is added
        {
            local $SIG{__WARN__} = sub { warn @_ unless $_[0] =~ /invalidates \d+ active statement/ };
            $conn->storage->disconnect; # for mssql and access
        }

        isa_ok try { $rsobj3->find(1)->loader_test4zes }, 'DBIx::Class::ResultSet';

        $conn->storage->disconnect; # for access

        if (lc($self->{vendor}) !~ /^(?:sybase|mysql)\z/) {
            $conn->storage->dbh->do('ALTER TABLE loader_test4 ADD fkid2 INTEGER REFERENCES loader_test3 (id)');
        }
        else {
            $conn->storage->dbh->do(<<"EOF");
            ALTER TABLE loader_test4 ADD fkid2 INTEGER $self->{null}
EOF
            $conn->storage->dbh->do(<<"EOF");
            ALTER TABLE loader_test4 ADD CONSTRAINT loader_test4_to_3_fk FOREIGN KEY (fkid2) REFERENCES loader_test3 (id)
EOF
        }

        $conn->storage->disconnect; # for firebird

        $self->rescan_without_warnings($conn);

        isa_ok try { $rsobj3->find(1)->loader_test4zes }, 'DBIx::Class::ResultSet',
            'relationship name preserved when another foreign key is added in remote table';

        SKIP: {
            skip 'This vendor cannot do out-of-line implicit rel defs', 4
                if $self->{no_implicit_rels};
            my $moniker14 = $monikers->{loader_test14};
            my $class14   = $classes->{loader_test14};
            my $rsobj14   = $conn->resultset($moniker14);

            my $moniker15 = $monikers->{loader_test15};
            my $class15   = $classes->{loader_test15};
            my $rsobj15   = $conn->resultset($moniker15);

            isa_ok( $rsobj14, "DBIx::Class::ResultSet" );
            isa_ok( $rsobj15, "DBIx::Class::ResultSet" );

            ok($class15->column_info('loader_test14')->{is_foreign_key}, 'Foreign key detected');

            my $obj15 = try { $rsobj15->find(1) } || $rsobj15->search({ id => 1 })->single;
            isa_ok( $obj15->loader_test14, $class14 );
        }
    }

    # test custom_column_info and datetime_timezone/datetime_locale
    {
        my $class35 = $classes->{loader_test35};
        my $class36 = $classes->{loader_test36};

        ok($class35->column_info('an_int')->{is_numeric}, 'custom_column_info');

        is($class36->column_info('a_date')->{locale},'de_DE','datetime_locale');
        is($class36->column_info('a_date')->{timezone},'Europe/Berlin','datetime_timezone');

        ok($class36->column_info('b_char_as_data')->{inflate_datetime},'custom_column_info');
        is($class36->column_info('b_char_as_data')->{locale},'de_DE','datetime_locale');
        is($class36->column_info('b_char_as_data')->{timezone},'Europe/Berlin','datetime_timezone');

        ok($class36->column_info('c_char_as_data')->{inflate_date},'custom_column_info');
        is($class36->column_info('c_char_as_data')->{locale},'de_DE','datetime_locale');
        is($class36->column_info('c_char_as_data')->{timezone},'Europe/Berlin','datetime_timezone');
    }

    # rescan and norewrite test
    {
        my @statements_rescan = (
            qq{
                CREATE TABLE loader_test30 (
                    id INTEGER NOT NULL PRIMARY KEY,
                    loader_test2 INTEGER NOT NULL,
                    FOREIGN KEY (loader_test2) REFERENCES loader_test2 (id)
                ) $self->{innodb}
            },
            q{ INSERT INTO loader_test30 (id,loader_test2) VALUES(123, 1) },
            q{ INSERT INTO loader_test30 (id,loader_test2) VALUES(321, 2) },
        );

        # get md5
        my $digest  = Digest::MD5->new;

        my $find_cb = sub {
            return if -d;
            return if /^(?:LoaderTest30|LoaderTest1|LoaderTest2X)\.pm\z/;

            open my $fh, '<', $_ or die "Could not open $_ for reading: $!";
            binmode $fh;
            $digest->addfile($fh);
        };

        find $find_cb, DUMP_DIR;

#        system "rm -rf /tmp/before_rescan /tmp/after_rescan";
#        system "mkdir /tmp/before_rescan";
#        system "mkdir /tmp/after_rescan";
#        system "cp -a @{[DUMP_DIR]} /tmp/before_rescan";

        my $before_digest = $digest->b64digest;

        $conn->storage->disconnect; # needed for Firebird and Informix
        my $dbh = $self->dbconnect(1);
        $dbh->do($_) for @statements_rescan;
        $dbh->disconnect;

        sleep 1;

        my @new = $self->rescan_without_warnings($conn);

        is_deeply(\@new, [ qw/LoaderTest30/ ], "Rescan");

#        system "cp -a @{[DUMP_DIR]} /tmp/after_rescan";

        $digest = Digest::MD5->new;
        find $find_cb, DUMP_DIR;
        my $after_digest = $digest->b64digest;

        is $before_digest, $after_digest,
            'dumped files are not rewritten when there is no modification';

        my $rsobj30   = $conn->resultset('LoaderTest30');
        isa_ok($rsobj30, 'DBIx::Class::ResultSet');

        SKIP: {
            skip 'no rels', 2 if $self->{skip_rels};

            my $obj30 = try { $rsobj30->find(123) } || $rsobj30->search({ id => 123 })->single;
            isa_ok( $obj30->loader_test2, $class2);

            ok($rsobj30->result_source->column_info('loader_test2')->{is_foreign_key},
               'Foreign key detected');
        }

        $conn->storage->disconnect; # for Firebird
        $self->drop_table($conn->storage->dbh, 'loader_test30');

        @new = $self->rescan_without_warnings($conn);

        is_deeply(\@new, [], 'no new tables on rescan');

        throws_ok { $conn->resultset('LoaderTest30') }
            qr/Can't find source/,
            'source unregistered for dropped table after rescan';
    }

    $self->test_data_types($conn);

    $self->test_preserve_case($conn);

    # run extra tests
    $self->{extra}{run}->($conn, $monikers, $classes, $self) if $self->{extra}{run};

    ## Create a dump from an existing $dbh in a transaction

TODO: {
    local $TODO = 'dumping in a txn is experimental and Pg-only right now'
        unless $self->{vendor} eq 'Pg';

    ok eval {
        my %opts = (
          naming         => 'current',
          constraint     => $self->CONSTRAINT,
          dump_directory => DUMP_DIR,
          debug          => ($ENV{SCHEMA_LOADER_TESTS_DEBUG}||0)
        );

        my $guard = $conn->txn_scope_guard;

        my $warn_handler = $SIG{__WARN__} || sub { warn @_ };
        local $SIG{__WARN__} = sub {
            $warn_handler->(@_)
                unless $_[0] =~ RESCAN_WARNINGS
                    || $_[0] =~ /commit ineffective with AutoCommit enabled/; # FIXME
        };

        my $schema_from = DBIx::Class::Schema::Loader::make_schema_at(
            "TestSchemaFromAnother", \%opts, [ sub { $conn->storage->dbh } ]
        );

        $guard->commit;

        1;
    }, 'Making a schema from another schema inside a transaction worked';

    diag $@ if $@ && (not $TODO);
}

    $self->drop_tables unless $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP};

    $conn->storage->disconnect;
}

sub test_data_types {
    my ($self, $conn) = @_;

    SKIP: {
        if (my $test_count = $self->{data_type_tests}{test_count}) {
            if ($self->{vendor} eq 'mssql' && $conn->storage->dbh->{Driver}{Name} eq 'Sybase') {
                skip 'DBD::Sybase does not work with the data_type tests on latest SQL Server', $test_count;
            }

            my $data_type_tests = $self->{data_type_tests};

            foreach my $moniker (@{ $data_type_tests->{table_monikers} }) {
                my $columns = $data_type_tests->{columns}{$moniker};

                my $rsrc = $conn->resultset($moniker)->result_source;

                while (my ($col_name, $expected_info) = each %$columns) {
                    my %info = %{ $rsrc->column_info($col_name) };
                    delete @info{qw/is_nullable timezone locale sequence/};

                    my $text_col_def = dumper_squashed \%info;

                    my $text_expected_info = dumper_squashed $expected_info;

                    is_deeply \%info, $expected_info,
                        "test column $col_name has definition: $text_col_def expecting: $text_expected_info";
                }
            }
        }
    }
}

sub test_preserve_case {
    my ($self, $conn) = @_;

    my ($oqt, $cqt) = $self->get_oqt_cqt(always => 1); # open quote, close quote

    my $dbh = $conn->storage->dbh;

    my ($table40_name, $table41_name) = @{ $self->{preserve_case_tests_table_names} };

    $dbh->do($_) for (
qq|
    CREATE TABLE ${oqt}${table40_name}${cqt} (
        ${oqt}Id${cqt} INTEGER NOT NULL PRIMARY KEY,
        ${oqt}Foo3Bar${cqt} VARCHAR(100) NOT NULL
    ) $self->{innodb}
|,
qq|
    CREATE TABLE ${oqt}${table41_name}${cqt} (
        ${oqt}Id${cqt} INTEGER NOT NULL PRIMARY KEY,
        ${oqt}LoaderTest40Id${cqt} INTEGER,
        FOREIGN KEY (${oqt}LoaderTest40Id${cqt}) REFERENCES ${oqt}${table40_name}${cqt} (${oqt}Id${cqt})
    ) $self->{innodb}
|,
qq| INSERT INTO ${oqt}${table40_name}${cqt} VALUES (1, 'foo') |,
qq| INSERT INTO ${oqt}${table41_name}${cqt} VALUES (1, 1) |,
    );
    $conn->storage->disconnect;

    my $orig_preserve_case = $conn->loader->preserve_case;

    $conn->loader->preserve_case(1);
    $conn->loader->_setup;
    $self->rescan_without_warnings($conn);

    if (not $self->{skip_rels}) {
        ok my $row = try { $conn->resultset('LoaderTest41')->find(1) },
            'row in mixed-case table';
        ok my $related_row = try { $row->loader_test40 },
            'rel in mixed-case table';
        is try { $related_row->foo3_bar }, 'foo',
            'accessor for mixed-case column name in mixed case table';
    }
    else {
        SKIP: { skip 'not testing mixed-case rels with skip_rels', 2 }

        is try { $conn->resultset('LoaderTest40')->find(1)->foo3_bar }, 'foo',
            'accessor for mixed-case column name in mixed case table';
    }

    # Further tests may expect preserve_case to be unset, so reset it to the
    # original value and rescan again.

    $conn->loader->preserve_case($orig_preserve_case);
    $conn->loader->_setup;
    $self->rescan_without_warnings($conn);
}

sub monikers_and_classes {
    my ($self, $schema_class) = @_;
    my ($monikers, $classes);

    foreach my $source_name ($schema_class->sources) {
        my $table_name = $schema_class->loader->moniker_to_table->{$source_name};

        my $result_class = $schema_class->source($source_name)->result_class;

        $monikers->{$table_name} = $source_name;
        $classes->{$table_name} = $result_class;

        # some DBs (Firebird, Oracle) uppercase everything
        $monikers->{lc $table_name} = $source_name;
        $classes->{lc $table_name} = $result_class;
    }

    return ($monikers, $classes);
}

sub check_no_duplicate_unique_constraints {
    my ($class) = @_;

    # unique_constraints() automatically includes the PK, if any
    my %uc_cols;
    ++$uc_cols{ join ", ", @$_ }
        for values %{ { $class->unique_constraints } };
    my $dup_uc = grep { $_ > 1 } values %uc_cols;

    is($dup_uc, 0, "duplicate unique constraints ($class)")
        or diag "uc_cols: @{[ %uc_cols ]}";
}

sub dbconnect {
    my ($self, $complain) = @_;

    require DBIx::Class::Storage::DBI;
    my $storage = DBIx::Class::Storage::DBI->new;

    $complain = defined $complain ? $complain : 1;

    $storage->connect_info([
        @{ $self }{qw/dsn user password/},
        {
            unsafe => 1,
            RaiseError => $complain,
            ShowErrorStatement => $complain,
            PrintError => 0,
            %{ $self->{connect_info_opts} || {} },
        },
    ]);

    my $dbh = $storage->dbh;
    die "Failed to connect to database: $@" if !$dbh;

    $self->{storage} = $storage; # storage DESTROY disconnects

    return $dbh;
}

sub get_oqt_cqt {
    my $self = shift;
    my %opts = @_;

    if ((not $opts{always}) && $self->{preserve_case_mode_is_exclusive}) {
        return ('', '');
    }

    # XXX should get quote_char from the storage of an initialized loader.
    my ($oqt, $cqt); # open quote, close quote
    if (ref $self->{quote_char}) {
        ($oqt, $cqt) = @{ $self->{quote_char} };
    }
    else {
        $oqt = $cqt = $self->{quote_char} || '';
    }

    return ($oqt, $cqt);
}

sub create {
    my $self = shift;

    $self->{_created} = 1;

    $self->drop_tables;

    my $make_auto_inc = $self->{auto_inc_cb} || sub { return () };
    @statements = (
        qq{
            CREATE TABLE loader_test1s (
                id $self->{auto_inc_pk},
                dat VARCHAR(32) NOT NULL UNIQUE
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test1s id/),

        q{ INSERT INTO loader_test1s (dat) VALUES('foo') },
        q{ INSERT INTO loader_test1s (dat) VALUES('bar') },
        q{ INSERT INTO loader_test1s (dat) VALUES('baz') },

        # also test method collision
        # crumb_crisp_coating is for col_accessor_map tests
        qq{
            CREATE TABLE loader_test2 (
                id $self->{auto_inc_pk},
                dat VARCHAR(32) NOT NULL,
                dat2 VARCHAR(32) NOT NULL,
                set_primary_key INTEGER $self->{null},
                can INTEGER $self->{null},
                dbix_class_testcomponent INTEGER $self->{null},
                dbix_class_testcomponentmap INTEGER $self->{null},
                testcomponent_fqn INTEGER $self->{null},
                meta INTEGER $self->{null},
                test_role_method INTEGER $self->{null},
                test_role_for_map_method INTEGER $self->{null},
                crumb_crisp_coating VARCHAR(32) $self->{null},
                UNIQUE (dat2, dat)
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test2 id/),

        q{ INSERT INTO loader_test2 (dat, dat2) VALUES('aaa', 'zzz') },
        q{ INSERT INTO loader_test2 (dat, dat2) VALUES('bbb', 'yyy') },
        q{ INSERT INTO loader_test2 (dat, dat2) VALUES('ccc', 'xxx') },
        q{ INSERT INTO loader_test2 (dat, dat2) VALUES('ddd', 'www') },

        qq{
            CREATE TABLE LOADER_test23 (
                ID INTEGER NOT NULL PRIMARY KEY,
                DAT VARCHAR(32) NOT NULL UNIQUE
            ) $self->{innodb}
        },

        qq{
            CREATE TABLE LoAdEr_test24 (
                iD INTEGER NOT NULL PRIMARY KEY,
                DaT VARCHAR(32) NOT NULL UNIQUE
            ) $self->{innodb}
        },

# Access does not support DEFAULT
        $self->{vendor} ne 'Access' ? qq{
            CREATE TABLE loader_test35 (
                id INTEGER NOT NULL PRIMARY KEY,
                a_varchar VARCHAR(100) DEFAULT 'foo',
                an_int INTEGER DEFAULT 42,
                a_negative_int INTEGER DEFAULT -42,
                a_double DOUBLE PRECISION DEFAULT 10.555,
                a_negative_double DOUBLE PRECISION DEFAULT -10.555,
                a_function $self->{default_function_def}
            ) $self->{innodb}
        } : qq{
            CREATE TABLE loader_test35 (
                id INTEGER NOT NULL PRIMARY KEY,
                a_varchar VARCHAR(100),
                an_int INTEGER,
                a_negative_int INTEGER,
                a_double DOUBLE,
                a_negative_double DOUBLE,
                a_function DATETIME
            )
        },

        qq{
            CREATE TABLE loader_test36 (
                id INTEGER NOT NULL PRIMARY KEY,
                a_date $self->{basic_date_datatype},
                b_char_as_data VARCHAR(100),
                c_char_as_data VARCHAR(100)
            ) $self->{innodb}
        },
        # DB2 does not allow nullable uniq components, SQLAnywhere automatically
        # converts nullable uniq components to NOT NULL
        qq{
            CREATE TABLE loader_test50 (
                id INTEGER NOT NULL UNIQUE,
                id1 INTEGER NOT NULL,
                id2 INTEGER NOT NULL,
                @{[ $self->{vendor} !~ /^(?:DB2|SQLAnywhere)\z/i ? "
                    id3 INTEGER $self->{null},
                    id4 INTEGER NOT NULL,
                    UNIQUE (id3, id4),
                " : '' ]}
                    UNIQUE (id1, id2)
            ) $self->{innodb}
        },
    );

    # some DBs require mixed case identifiers to be quoted
    my ($oqt, $cqt) = $self->get_oqt_cqt;

    @statements_reltests = (
        qq{
            CREATE TABLE loader_test3 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat VARCHAR(32)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test3 (id,dat) VALUES(1,'aaa') },
        q{ INSERT INTO loader_test3 (id,dat) VALUES(2,'bbb') },
        q{ INSERT INTO loader_test3 (id,dat) VALUES(3,'ccc') },
        q{ INSERT INTO loader_test3 (id,dat) VALUES(4,'ddd') },

        qq{
            CREATE TABLE loader_test4 (
                id INTEGER NOT NULL PRIMARY KEY,
                fkid INTEGER NOT NULL,
                dat VARCHAR(32),
                belongs_to INTEGER $self->{null},
                set_primary_key INTEGER $self->{null},
                FOREIGN KEY( fkid ) REFERENCES loader_test3 (id),
                FOREIGN KEY( belongs_to ) REFERENCES loader_test3 (id),
                FOREIGN KEY( set_primary_key ) REFERENCES loader_test3 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test4 (id,fkid,dat,belongs_to,set_primary_key) VALUES(123,1,'aaa',1,1) },
        q{ INSERT INTO loader_test4 (id,fkid,dat,belongs_to,set_primary_key) VALUES(124,2,'bbb',2,2) },
        q{ INSERT INTO loader_test4 (id,fkid,dat,belongs_to,set_primary_key) VALUES(125,3,'ccc',3,3) },
        q{ INSERT INTO loader_test4 (id,fkid,dat,belongs_to,set_primary_key) VALUES(126,4,'ddd',4,4) },

        qq|
            CREATE TABLE loader_test5 (
                id1 INTEGER NOT NULL,
                ${oqt}iD2${cqt} INTEGER NOT NULL,
                dat VARCHAR(8),
                from_id INTEGER $self->{null},
                to_id INTEGER $self->{null},
                PRIMARY KEY (id1,${oqt}iD2${cqt}),
                FOREIGN KEY (from_id) REFERENCES loader_test4 (id),
                FOREIGN KEY (to_id) REFERENCES loader_test4 (id)
            ) $self->{innodb}
        |,

        qq| INSERT INTO loader_test5 (id1,${oqt}iD2${cqt},dat) VALUES (1,1,'aaa') |,

        qq|
            CREATE TABLE loader_test6 (
                id INTEGER NOT NULL PRIMARY KEY,
                ${oqt}Id2${cqt} INTEGER,
                loader_test2_id INTEGER,
                dat VARCHAR(8),
                FOREIGN KEY (loader_test2_id)  REFERENCES loader_test2 (id),
                FOREIGN KEY(id,${oqt}Id2${cqt}) REFERENCES loader_test5 (id1,${oqt}iD2${cqt})
            ) $self->{innodb}
        |,

        (qq| INSERT INTO loader_test6 (id, ${oqt}Id2${cqt},loader_test2_id,dat) | .
         q{ VALUES (1, 1,1,'aaa') }),

        # here we are testing adjective detection

        qq{
            CREATE TABLE loader_test7 (
                id INTEGER NOT NULL PRIMARY KEY,
                id2 VARCHAR(8) NOT NULL UNIQUE,
                dat VARCHAR(8),
                lovely_loader_test6 INTEGER NOT NULL UNIQUE,
                FOREIGN KEY (lovely_loader_test6) REFERENCES loader_test6 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test7 (id,id2,dat,lovely_loader_test6) VALUES (1,'aaa','bbb',1) },

        # for some DBs we need a named FK to drop later
        ($self->{vendor} =~ /^(mssql|sybase|access|mysql)\z/i ? (
            (q{ ALTER TABLE loader_test6 ADD } .
             qq{ loader_test7_id INTEGER $self->{null} }),
            (q{ ALTER TABLE loader_test6 ADD CONSTRAINT loader_test6_to_7_fk } .
             q{ FOREIGN KEY (loader_test7_id) } .
             q{ REFERENCES loader_test7 (id) })
        ) : (
            (q{ ALTER TABLE loader_test6 ADD } .
             qq{ loader_test7_id INTEGER $self->{null} REFERENCES loader_test7 (id) }),
        )),

        qq{
            CREATE TABLE loader_test8 (
                id INTEGER NOT NULL PRIMARY KEY,
                loader_test7 VARCHAR(8) NOT NULL,
                dat VARCHAR(8),
                FOREIGN KEY (loader_test7) REFERENCES loader_test7 (id2)
            ) $self->{innodb}
        },

        (q{ INSERT INTO loader_test8 (id,loader_test7,dat) VALUES (1,'aaa','bbb') }),
        (q{ INSERT INTO loader_test8 (id,loader_test7,dat) VALUES (2,'aaa','bbb') }),
        (q{ INSERT INTO loader_test8 (id,loader_test7,dat) VALUES (3,'aaa','bbb') }),

        qq{
            CREATE TABLE loader_test9 (
                loader_test9 VARCHAR(8) NOT NULL
            ) $self->{innodb}
        },

        qq{
            CREATE TABLE loader_test16 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat  VARCHAR(8),
                loader_test8_id INTEGER NOT NULL UNIQUE,
                FOREIGN KEY (loader_test8_id) REFERENCES loader_test8 (id)
            ) $self->{innodb}
        },

        qq{ INSERT INTO loader_test16 (id,dat,loader_test8_id) VALUES (2,'x16',1) },
        qq{ INSERT INTO loader_test16 (id,dat,loader_test8_id) VALUES (4,'y16',2) },
        qq{ INSERT INTO loader_test16 (id,dat,loader_test8_id) VALUES (6,'z16',3) },

        # for some DBs we need a named FK to drop later
        ($self->{vendor} =~ /^(mssql|sybase|access|mysql)\z/i ? (
            (q{ ALTER TABLE loader_test8 ADD } .
             qq{ loader_test16_id INTEGER $self->{null} }),
            (q{ ALTER TABLE loader_test8 ADD CONSTRAINT loader_test8_to_16_fk } .
             q{ FOREIGN KEY (loader_test16_id) } .
             q{ REFERENCES loader_test16 (id) })
        ) : (
            (q{ ALTER TABLE loader_test8 ADD } .
             qq{ loader_test16_id INTEGER $self->{null} REFERENCES loader_test16 (id) }),
        )),

        qq{
            CREATE TABLE loader_test17 (
                id INTEGER NOT NULL PRIMARY KEY,
                loader16_one INTEGER,
                loader16_two INTEGER,
                FOREIGN KEY (loader16_one) REFERENCES loader_test16 (id),
                FOREIGN KEY (loader16_two) REFERENCES loader_test16 (id)
            ) $self->{innodb}
        },

        qq{ INSERT INTO loader_test17 (id, loader16_one, loader16_two) VALUES (3, 2, 4) },
        qq{ INSERT INTO loader_test17 (id, loader16_one, loader16_two) VALUES (33, 4, 6) },

        qq{
            CREATE TABLE loader_test18 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat  VARCHAR(8)
            ) $self->{innodb}
        },

        qq{ INSERT INTO loader_test18 (id,dat) VALUES (1,'x18') },
        qq{ INSERT INTO loader_test18 (id,dat) VALUES (2,'y18') },
        qq{ INSERT INTO loader_test18 (id,dat) VALUES (3,'z18') },

        qq{
            CREATE TABLE loader_test19 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat  VARCHAR(8)
            ) $self->{innodb}
        },

        qq{ INSERT INTO loader_test19 (id,dat) VALUES (4,'x19') },
        qq{ INSERT INTO loader_test19 (id,dat) VALUES (5,'y19') },
        qq{ INSERT INTO loader_test19 (id,dat) VALUES (6,'z19') },

        qq{
            CREATE TABLE loader_test20 (
                parent INTEGER NOT NULL,
                child INTEGER NOT NULL,
                PRIMARY KEY (parent, child),
                FOREIGN KEY (parent) REFERENCES loader_test18 (id),
                FOREIGN KEY (child) REFERENCES loader_test19 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test20 (parent, child) VALUES (1,4) },
        q{ INSERT INTO loader_test20 (parent, child) VALUES (2,5) },
        q{ INSERT INTO loader_test20 (parent, child) VALUES (3,6) },

        qq{
            CREATE TABLE loader_test21 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat  VARCHAR(8)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test21 (id,dat) VALUES (7,'a21')},
        q{ INSERT INTO loader_test21 (id,dat) VALUES (11,'b21')},
        q{ INSERT INTO loader_test21 (id,dat) VALUES (13,'c21')},
        q{ INSERT INTO loader_test21 (id,dat) VALUES (17,'d21')},

        qq{
            CREATE TABLE loader_test22 (
                parent INTEGER NOT NULL,
                child INTEGER NOT NULL,
                PRIMARY KEY (parent, child),
                FOREIGN KEY (parent) REFERENCES loader_test21 (id),
                FOREIGN KEY (child) REFERENCES loader_test21 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test22 (parent, child) VALUES (7,11)},
        q{ INSERT INTO loader_test22 (parent, child) VALUES (11,13)},
        q{ INSERT INTO loader_test22 (parent, child) VALUES (13,17)},

	qq{
            CREATE TABLE loader_test25 (
                id1 INTEGER NOT NULL,
                id2 INTEGER NOT NULL,
                dat VARCHAR(8),
                PRIMARY KEY (id1,id2)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test25 (id1,id2,dat) VALUES (33,5,'x25') },
        q{ INSERT INTO loader_test25 (id1,id2,dat) VALUES (33,7,'y25') },
        q{ INSERT INTO loader_test25 (id1,id2,dat) VALUES (3,42,'z25') },

        qq{
            CREATE TABLE loader_test26 (
               id INTEGER NOT NULL PRIMARY KEY,
               rel1 INTEGER NOT NULL,
               rel2 INTEGER NOT NULL,
               FOREIGN KEY (id, rel1) REFERENCES loader_test25 (id1, id2),
               FOREIGN KEY (id, rel2) REFERENCES loader_test25 (id1, id2)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test26 (id,rel1,rel2) VALUES (33,5,7) },
        q{ INSERT INTO loader_test26 (id,rel1,rel2) VALUES (3,42,42) },

        qq{
            CREATE TABLE loader_test27 (
                id INTEGER NOT NULL PRIMARY KEY
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test27 (id) VALUES (1) },
        q{ INSERT INTO loader_test27 (id) VALUES (2) },

        qq{
            CREATE TABLE loader_test28 (
                id INTEGER NOT NULL PRIMARY KEY,
                FOREIGN KEY (id) REFERENCES loader_test27 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test28 (id) VALUES (1) },

        qq{
            CREATE TABLE loader_test29 (
                id INTEGER NOT NULL PRIMARY KEY,
                fk INTEGER NOT NULL UNIQUE,
                FOREIGN KEY (fk) REFERENCES loader_test27 (id)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test29 (id,fk) VALUES (1,1) },

        qq{
          CREATE TABLE loader_test31 (
            id INTEGER NOT NULL PRIMARY KEY
          ) $self->{innodb}
        },
        q{ INSERT INTO loader_test31 (id) VALUES (1) },

        qq{
          CREATE TABLE loader_test32 (
            id INTEGER NOT NULL PRIMARY KEY,
            rel1 INTEGER NOT NULL,
            rel2 INTEGER $self->{null},
            FOREIGN KEY (rel1) REFERENCES loader_test31(id),
            FOREIGN KEY (rel2) REFERENCES loader_test31(id)
          ) $self->{innodb}
        },
        q{ INSERT INTO loader_test32 (id,rel1) VALUES (1,1) },

        qq{
          CREATE TABLE loader_test33 (
            id1 INTEGER NOT NULL,
            id2 INTEGER NOT NULL,
            PRIMARY KEY (id1,id2)
          ) $self->{innodb}
        },
        q{ INSERT INTO loader_test33 (id1,id2) VALUES (1,2) },

        qq{
          CREATE TABLE loader_test34 (
            id INTEGER NOT NULL PRIMARY KEY,
            rel1 INTEGER NOT NULL,
            rel2 INTEGER $self->{null},
            FOREIGN KEY (id,rel1) REFERENCES loader_test33(id1,id2),
            FOREIGN KEY (id,rel2) REFERENCES loader_test33(id1,id2)
          ) $self->{innodb}
        },
        q{ INSERT INTO loader_test34 (id,rel1,rel2) VALUES (1,2,2) },
    );

    @statements_advanced = (
        qq{
            CREATE TABLE loader_test10 (
                id10 $self->{auto_inc_pk},
                subject VARCHAR(8),
                loader_test11 INTEGER $self->{null}
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test10 id10/),

# Access does not support DEFAULT.
        qq{
            CREATE TABLE loader_test11 (
                id11 $self->{auto_inc_pk},
                a_message VARCHAR(8) @{[ $self->{vendor} ne 'Access' ? "DEFAULT 'foo'" : '' ]},
                loader_test10 INTEGER $self->{null},
                FOREIGN KEY (loader_test10) REFERENCES loader_test10 (id10)
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test11 id11/),

        (lc($self->{vendor}) ne 'informix' ?
            (q{ ALTER TABLE loader_test10 ADD CONSTRAINT loader_test11_fk } .
             q{ FOREIGN KEY (loader_test11) } .
             q{ REFERENCES loader_test11 (id11) })
        :
            (q{ ALTER TABLE loader_test10 ADD CONSTRAINT } .
             q{ FOREIGN KEY (loader_test11) } .
             q{ REFERENCES loader_test11 (id11) } .
             q{ CONSTRAINT loader_test11_fk })
        ),
    );

    @statements_advanced_sqlite = (
        qq{
            CREATE TABLE loader_test10 (
                id10 $self->{auto_inc_pk},
                subject VARCHAR(8)
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test10 id10/),

        qq{
            CREATE TABLE loader_test11 (
                id11 $self->{auto_inc_pk},
                a_message VARCHAR(8) DEFAULT 'foo',
                loader_test10 INTEGER $self->{null},
                FOREIGN KEY (loader_test10) REFERENCES loader_test10 (id10)
            ) $self->{innodb}
        },
        $make_auto_inc->(qw/loader_test11 id11/),

        (q{ ALTER TABLE loader_test10 ADD COLUMN } .
         q{ loader_test11 INTEGER REFERENCES loader_test11 (id11) }),
    );

    @statements_inline_rels = (
        qq{
            CREATE TABLE loader_test12 (
                id INTEGER NOT NULL PRIMARY KEY,
                id2 VARCHAR(8) NOT NULL UNIQUE,
                dat VARCHAR(8) NOT NULL UNIQUE
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test12 (id,id2,dat) VALUES (1,'aaa','bbb') },

        qq{
            CREATE TABLE loader_test13 (
                id INTEGER NOT NULL PRIMARY KEY REFERENCES loader_test12,
                loader_test12 VARCHAR(8) NOT NULL REFERENCES loader_test12 (id2),
                dat VARCHAR(8) REFERENCES loader_test12 (dat)
            ) $self->{innodb}
        },

        (q{ INSERT INTO loader_test13 (id,loader_test12,dat) } .
         q{ VALUES (1,'aaa','bbb') }),
    );


    @statements_implicit_rels = (
        qq{
            CREATE TABLE loader_test14 (
                id INTEGER NOT NULL PRIMARY KEY,
                dat VARCHAR(8)
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test14 (id,dat) VALUES (123,'aaa') },

        qq{
            CREATE TABLE loader_test15 (
                id INTEGER NOT NULL PRIMARY KEY,
                loader_test14 INTEGER NOT NULL,
                FOREIGN KEY (loader_test14) REFERENCES loader_test14
            ) $self->{innodb}
        },

        q{ INSERT INTO loader_test15 (id,loader_test14) VALUES (1,123) },
    );

    $self->drop_tables;

    my $dbh = $self->dbconnect(1);

    $dbh->do($_) for @{ $self->{pre_create} || [] };

    $dbh->do($_) foreach (@statements);

    if (not ($self->{vendor} eq 'mssql' && $dbh->{Driver}{Name} eq 'Sybase')) {
        foreach my $ddl (@{ $self->{data_type_tests}{ddl} || [] }) {
            if (my $cb = $self->{data_types_ddl_cb}) {
                $cb->($ddl);
            }
            else {
                $dbh->do($ddl);
            }
        }
    }

    unless ($self->{skip_rels}) {
        # hack for now, since DB2 doesn't like inline comments, and we need
        # to test one for mysql, which works on everyone else...
        # this all needs to be refactored anyways.

        for my $stmt (@statements_reltests) {
            try {
                $dbh->do($stmt);
            }
            catch {
                die "Error executing '$stmt': $_\n";
            };
        }
        if($self->{vendor} =~ /sqlite/i) {
            $dbh->do($_) for (@statements_advanced_sqlite);
        }
        else {
            $dbh->do($_) for (@statements_advanced);
        }
        unless($self->{no_inline_rels}) {
            $dbh->do($_) for (@statements_inline_rels);
        }
        unless($self->{no_implicit_rels}) {
            $dbh->do($_) for (@statements_implicit_rels);
        }
    }

    $dbh->do($_) for @{ $self->{extra}->{create} || [] };
    $dbh->disconnect();
}

sub drop_tables {
    my $self = shift;

    my @tables = qw/
        loader_test1
        loader_test1s
        loader_test2
        LOADER_test23
        LoAdEr_test24
        loader_test35
        loader_test36
        loader_test50
    /;

    my @tables_auto_inc = (
        [ qw/loader_test1s id/ ],
        [ qw/loader_test2 id/ ],
    );

    my @tables_reltests = qw/
        loader_test4
        loader_test3
        loader_test6
        loader_test5
        loader_test8
        loader_test7
        loader_test9
        loader_test17
        loader_test16
        loader_test20
        loader_test19
        loader_test18
        loader_test22
        loader_test21
        loader_test26
        loader_test25
        loader_test28
        loader_test29
        loader_test27
        loader_test32
        loader_test31
        loader_test34
        loader_test33
    /;

    my @tables_advanced = qw/
        loader_test11
        loader_test10
    /;

    my @tables_advanced_auto_inc = (
        [ qw/loader_test10 id10/ ],
        [ qw/loader_test11 id11/ ],
    );

    my @tables_inline_rels = qw/
        loader_test13
        loader_test12
    /;

    my @tables_implicit_rels = qw/
        loader_test15
        loader_test14
    /;

    my @tables_rescan = qw/ loader_test30 /;

    my @tables_preserve_case_tests = @{ $self->{preserve_case_tests_table_names} };

    my %drop_columns = (
        loader_test6  => 'loader_test7_id',
        loader_test7  => 'lovely_loader_test6',
        loader_test8  => 'loader_test16_id',
        loader_test16 => 'loader_test8_id',
    );

    my %drop_constraints = (
        loader_test10 => 'loader_test11_fk',
        loader_test6  => 'loader_test6_to_7_fk',
        loader_test8  => 'loader_test8_to_16_fk',
    );

    # For some reason some tests do this twice (I guess dependency issues?)
    # do it twice for all drops
    for (1,2) {
        local $^W = 0; # for ADO

        my $dbh = $self->dbconnect(0);

        $dbh->do($_) for @{ $self->{extra}{pre_drop_ddl} || [] };

        $self->drop_table($dbh, $_) for @{ $self->{extra}{drop} || [] };

        my $drop_auto_inc = $self->{auto_inc_drop_cb} || sub {};

        unless ($self->{skip_rels}) {
            # drop the circular rel columns if possible, this
            # doesn't work on all DBs
            foreach my $table (keys %drop_columns) {
                $dbh->do("ALTER TABLE $table DROP $drop_columns{$table}");
                $dbh->do("ALTER TABLE $table DROP COLUMN $drop_columns{$table}");
            }

            foreach my $table (keys %drop_constraints) {
                # for MSSQL
                $dbh->do("ALTER TABLE $table DROP $drop_constraints{$table}");
                # for Sybase and Access
                $dbh->do("ALTER TABLE $table DROP CONSTRAINT $drop_constraints{$table}");
                # for MySQL
                $dbh->do("ALTER TABLE $table DROP FOREIGN KEY $drop_constraints{$table}");
            }

            $self->drop_table($dbh, $_) for (@tables_reltests);
            $self->drop_table($dbh, $_) for (@tables_reltests);

            $dbh->do($_) for map { $drop_auto_inc->(@$_) } @tables_advanced_auto_inc;

            $self->drop_table($dbh, $_) for (@tables_advanced);

            unless($self->{no_inline_rels}) {
                $self->drop_table($dbh, $_) for (@tables_inline_rels);
            }
            unless($self->{no_implicit_rels}) {
                $self->drop_table($dbh, $_) for (@tables_implicit_rels);
            }
        }
        $dbh->do($_) for map { $drop_auto_inc->(@$_) } @tables_auto_inc;
        $self->drop_table($dbh, $_) for (@tables, @tables_rescan);

        if (not ($self->{vendor} eq 'mssql' && $dbh->{Driver}{Name} eq 'Sybase')) {
            foreach my $data_type_table (@{ $self->{data_type_tests}{table_names} || [] }) {
                $self->drop_table($dbh, $data_type_table);
            }
        }

        $self->drop_table($dbh, $_) for @tables_preserve_case_tests;

        $dbh->disconnect;
    }
}

sub drop_table {
    my ($self, $dbh, $table) = @_;

    local $^W = 0; # for ADO

    try { $dbh->do("DROP TABLE $table CASCADE CONSTRAINTS") }; # oracle
    try { $dbh->do("DROP TABLE $table CASCADE") }; # postgres and ?
    try { $dbh->do("DROP TABLE $table") };

    # if table name is case sensitive
    my ($oqt, $cqt) = $self->get_oqt_cqt(always => 1);

    try { $dbh->do("DROP TABLE ${oqt}${table}${cqt}") };
}

sub _custom_column_info {
    my ( $table_name, $column_name, $column_info ) = @_;

    $table_name = lc ( $table_name );
    $column_name = lc ( $column_name );

    if ( $table_name eq 'loader_test35'
        and $column_name eq 'an_int'
    ){
        return { is_numeric => 1 }
    }
    # Set inflate_datetime or  inflate_date to check
    #   datetime_timezone and datetime_locale
    if ( $table_name eq 'loader_test36' ){
        return { inflate_datetime => 1 } if
            ( $column_name eq 'b_char_as_data' );
        return { inflate_date => 1 } if
            ( $column_name eq 'c_char_as_data' );
    }

    return;
}

my %DATA_TYPE_MULTI_TABLE_OVERRIDES = (
    oracle => qr/\blong\b/i,
    mssql  => qr/\b(?:timestamp|rowversion)\b/i,
    informix => qr/\b(?:bigserial|serial8)\b/i,
);

sub setup_data_type_tests {
    my $self = shift;

    return unless my $types = $self->{data_types};

    my $tests = $self->{data_type_tests} = {};

    # split types into tables based on overrides
    my (@types, @split_off_types, @first_table_types);
    {
        my $split_off_re = $DATA_TYPE_MULTI_TABLE_OVERRIDES{lc($self->{vendor})} || qr/(?!)/;

        @types = keys %$types;
        @split_off_types   = grep  /$split_off_re/, @types;
        @first_table_types = grep !/$split_off_re/, @types;
    }

    @types = +{ map +($_, $types->{$_}), @first_table_types },
        map +{ $_, $types->{$_} }, @split_off_types;

    my $test_count = 0;
    my $table_num  = 10000;

    foreach my $types (@types) {
        my $table_name    = "loader_test$table_num";
        push @{ $tests->{table_names} }, $table_name;

        my $table_moniker = "LoaderTest$table_num";
        push @{ $tests->{table_monikers} }, $table_moniker;

        $table_num++;

        my $cols = $tests->{columns}{$table_moniker} = {};

        my $ddl = "CREATE TABLE $table_name (\n    id INTEGER NOT NULL PRIMARY KEY,\n";

        my %seen_col_names;

        while (my ($col_def, $expected_info) = each %$types) {
            (my $type_alias = $col_def) =~ s/\( (.+) \)(?=(?:[^()]* '(?:[^']* (?:''|\\')* [^']*)* [^\\']' [^()]*)*\z)//xg;

            my $size = $1;
            $size = '' unless defined $size;
            $size = '' unless $size =~ /^[\d, ]+\z/;
            $size =~ s/\s+//g;
            my @size = split /,/, $size;

            # some DBs don't like very long column names
            if ($self->{vendor} =~ /^(?:Firebird|SQLAnywhere|Oracle|DB2)\z/i) {
                my ($col_def, $default) = $type_alias =~ /^(.*)(default.*)?\z/i;

                $type_alias = substr $col_def, 0, 15;

                $type_alias .= '_with_dflt' if $default;
            }

            $type_alias =~ s/\s/_/g;
            $type_alias =~ s/\W//g;

            my $col_name = 'col_' . $type_alias;

            if (@size) {
                my $size_name = join '_', apply { s/\W//g } @size;

                $col_name .= "_sz_$size_name";
            }

            # XXX would be better to check loader->preserve_case
            $col_name = lc $col_name;

            $col_name .= '_' . $seen_col_names{$col_name} if $seen_col_names{$col_name}++;

            $ddl .= "    $col_name $col_def,\n";

            $cols->{$col_name} = $expected_info;

            $test_count++;
        }

        $ddl =~ s/,\n\z/\n)/;

        push @{ $tests->{ddl} }, $ddl;
    }

    $tests->{test_count} = $test_count;

    return $test_count;
}

sub rescan_without_warnings {
    my ($self, $conn) = @_;

    local $SIG{__WARN__} = sub { warn @_ unless $_[0] =~ RESCAN_WARNINGS };
    return $conn->rescan;
}

sub test_col_accessor_map {
    my ( $column_name, $default_name, $context ) = @_;
    if( lc($column_name) eq 'crumb_crisp_coating' ) {

        is( $default_name, 'crumb_crisp_coating', 'col_accessor_map was passed the default name' );
        ok( $context->{$_}, "col_accessor_map func was passed the $_" )
            for qw( table_name table_class table_moniker schema_class );

        return 'trivet';
    } else {
        return $default_name;
    }
}

sub DESTROY {
    my $self = shift;
    unless ($ENV{SCHEMA_LOADER_TESTS_NOCLEANUP}) {
      $self->drop_tables if $self->{_created};
      rmtree DUMP_DIR
    }
}

1;
# vim:et sts=4 sw=4 tw=0:
