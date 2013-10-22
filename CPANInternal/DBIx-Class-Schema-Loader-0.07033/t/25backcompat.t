use strict;
use warnings;
use Test::More;
use Test::Exception;
use File::Path qw/rmtree make_path/;
use Class::Unload;
use File::Temp qw/tempfile tempdir/;
use IO::File;
use DBIx::Class::Schema::Loader ();
use DBIx::Class::Schema::Loader::Utils 'slurp_file';
use Lingua::EN::Inflect::Number ();
use lib qw(t/lib);
use make_dbictest_db_with_unique;
use dbixcsl_test_dir qw/$tdir/;

my $DUMP_DIR = "$tdir/common_dump";
rmtree $DUMP_DIR;
my $SCHEMA_CLASS = 'DBIXCSL_Test::Schema';

my $RESULT_COUNT = 7;

sub class_content_like;

# test dynamic schema in 0.04006 mode
{
    my $res = run_loader();
    my $warning = $res->{warnings}[0];

    like $warning, qr/dynamic schema/i,
        'dynamic schema in backcompat mode detected';
    like $warning, qr/run in 0\.04006 mode/i,
        'dynamic schema in 0.04006 mode warning';
    like $warning, qr/DBIx::Class::Schema::Loader::Manual::UpgradingFromV4/,
        'warning refers to upgrading doc';
    
    run_v4_tests($res);
}

# setting naming accessor on dynamic schema should disable warning (even when
# we're setting it to 'v4' .)
{
    my $res = run_loader(naming => 'v4');
    is_deeply $res->{warnings}, [], 'no warnings with naming attribute set';
    run_v4_tests($res);
}

# test upgraded dynamic schema
{
    my $res = run_loader(naming => 'current');
    is_deeply $res->{warnings}, [], 'no warnings with naming attribute set';
    run_v7_tests($res);
}

# test upgraded dynamic schema with external content loaded
{
    my $temp_dir = setup_load_external({
        Quuxs => 'Bazs',
        Bar   => 'Foos',
    });

    my $res = run_loader(naming => 'current', use_namespaces => 0);
    my $schema = $res->{schema};

    is scalar @{ $res->{warnings} }, 1,
'correct nummber of warnings for upgraded dynamic schema with external ' .
'content for unsingularized Result.';

    my $warning = $res->{warnings}[0];
    like $warning, qr/Detected external content/i,
        'detected external content warning';

    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'hlagh' }
'external custom content for unsingularized Result was loaded by upgraded ' .
'dynamic Schema';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel,
        $res->{classes}{bazs} }
        'unsingularized class names in external content are translated';

    lives_and { is $schema->resultset('Bar')->find(1)->a_method, 'hlagh' }
'external content from unchanged Result class';

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel,
        $res->{classes}{foos} }
'unsingularized class names in external content from unchanged Result class ' .
'names are translated';

    run_v7_tests($res);
}

# test upgraded dynamic schema with use_namespaces with external content loaded
{
    my $temp_dir = setup_load_external({
        Quuxs => 'Bazs',
        Bar   => 'Foos',
    });

    my $res = run_loader(naming => 'current', use_namespaces => 1);
    my $schema = $res->{schema};

    is scalar @{ $res->{warnings} }, 2,
'correct nummber of warnings for upgraded dynamic schema with external ' .
'content for unsingularized Result with use_namespaces.';

    my $warning = $res->{warnings}[0];
    like $warning, qr/Detected external content/i,
        'detected external content warning';

    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'hlagh' }
'external custom content for unsingularized Result was loaded by upgraded ' .
'dynamic Schema';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel,
        $res->{classes}{bazs} }
        'unsingularized class names in external content are translated';

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel,
        $res->{classes}{foos} }
'unsingularized class names in external content from unchanged Result class ' .
'names are translated';

    run_v7_tests($res);
}

# test upgraded static schema with external content loaded
{
    clean_dumpdir();

    my $temp_dir = setup_load_external({
        Quuxs => 'Bazs',
        Bar   => 'Foos',
    });

    write_v4_schema_pm();

    my $res = run_loader(static => 1, naming => 'current');
    my $schema = $res->{schema};

    run_v7_tests($res);

    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'hlagh' }
'external custom content for unsingularized Result was loaded by upgraded ' .
'static Schema';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel,
        $res->{classes}{bazs} }
        'unsingularized class names in external content are translated';

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel,
        $res->{classes}{foos} }
'unsingularized class names in external content from unchanged Result class ' .
'names are translated in static schema';

    class_content_like $schema, $res->{classes}{quuxs}, qr/package ${SCHEMA_CLASS}::Quux;/,
'package line translated correctly from external custom content in static dump';

    class_content_like $schema, $res->{classes}{quuxs}, qr/sub a_method { 'hlagh' }/,
'external custom content loaded into static dump correctly';
}

# test running against v4 schema without upgrade, twice, then upgrade
{
    clean_dumpdir();
    write_v4_schema_pm();
    my $res = run_loader(static => 1);
    my $warning = $res->{warnings}[1];

    like $warning, qr/static schema/i,
        'static schema in backcompat mode detected';
    like $warning, qr/0.04006/,
        'correct version detected';
    like $warning, qr/DBIx::Class::Schema::Loader::Manual::UpgradingFromV4/,
        'refers to upgrading doc';

    is scalar @{ $res->{warnings} }, 4,
        'correct number of warnings for static schema in backcompat mode';

    run_v4_tests($res);

    add_custom_content($res->{schema}, {
        Quuxs => 'Bazs'
    });

    # Rerun the loader in backcompat mode to make sure it's still in backcompat
    # mode.
    $res = run_loader(static => 1);
    run_v4_tests($res);

    # now upgrade the schema
    $res = run_loader(
        static => 1,
        naming => 'current',
        use_namespaces => 1
    );
    my $schema = $res->{schema};

    like $res->{warnings}[0], qr/Dumping manual schema/i,
        'correct warnings on upgrading static schema (with "naming" set)';

    like $res->{warnings}[1], qr/dump completed/i,
        'correct warnings on upgrading static schema (with "naming" set)';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on upgrading static schema (with "naming" set)'
        or diag @{ $res->{warnings} };

    run_v7_tests($res);

    is result_count('Result'), $RESULT_COUNT,
        'un-singularized results were replaced during upgrade';

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->b_method, 'dongs' }
        'custom content was carried over from un-singularized Result';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel,
        $res->{classes}{bazs} }
        'unsingularized class names in custom content are translated';

    class_content_like $schema, $res->{classes}{quuxs}, qr/sub b_method { 'dongs' }/,
'custom content from unsingularized Result loaded into static dump correctly';
}

# test running against v4 schema without upgrade, then upgrade with
# use_namespaces not explicitly set
{
    clean_dumpdir();
    write_v4_schema_pm();
    my $res = run_loader(static => 1);
    my $warning = $res->{warnings}[1];

    like $warning, qr/static schema/i,
        'static schema in backcompat mode detected';
    like $warning, qr/0.04006/,
        'correct version detected';
    like $warning, qr/DBIx::Class::Schema::Loader::Manual::UpgradingFromV4/,
        'refers to upgrading doc';

    is scalar @{ $res->{warnings} }, 4,
        'correct number of warnings for static schema in backcompat mode';

    run_v4_tests($res);

    add_custom_content($res->{schema}, {
        Quuxs => 'Bazs'
    });

    # now upgrade the schema
    $res = run_loader(
        static => 1,
        naming => 'current'
    );
    my $schema = $res->{schema};

    like $res->{warnings}[0], qr/load_classes/i,
'correct warnings on upgrading static schema (with "naming" set and ' .
'use_namespaces not set)';

    like $res->{warnings}[1], qr/Dumping manual schema/i,
'correct warnings on upgrading static schema (with "naming" set and ' .
'use_namespaces not set)';

    like $res->{warnings}[2], qr/dump completed/i,
'correct warnings on upgrading static schema (with "naming" set and ' .
'use_namespaces not set)';

    is scalar @{ $res->{warnings} }, 3,
'correct number of warnings on upgrading static schema (with "naming" set)'
        or diag @{ $res->{warnings} };

    run_v7_tests($res);

    is result_count(), $RESULT_COUNT,
        'un-singularized results were replaced during upgrade';

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->b_method, 'dongs' }
        'custom content was carried over from un-singularized Result';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel,
        $res->{classes}{bazs} }
        'unsingularized class names in custom content are translated';

    class_content_like $schema, $res->{classes}{quuxs}, qr/sub b_method { 'dongs' }/,
'custom content from unsingularized Result loaded into static dump correctly';
}

# test running against v4 schema with load_namespaces, upgrade to current but
# downgrade to load_classes, with external content
{
    clean_dumpdir();

    my $temp_dir = setup_load_external({
        Quuxs => 'Bazs',
        Bar   => 'Foos',
    }, { result_namespace => 'Result' });

    write_v4_schema_pm(use_namespaces => 1);

    my $res = run_loader(static => 1);
    my $warning = $res->{warnings}[0];

    like $warning, qr/static schema/i,
        'static schema in backcompat mode detected';
    like $warning, qr/0.04006/,
        'correct version detected';
    like $warning, qr/DBIx::Class::Schema::Loader::Manual::UpgradingFromV4/,
        'refers to upgrading doc';

    is scalar @{ $res->{warnings} }, 3,
        'correct number of warnings for static schema in backcompat mode';

    run_v4_tests($res);

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Result::Quuxs',
        'use_namespaces in backcompat mode';

    add_custom_content($res->{schema}, {
        Quuxs => 'Bazs',
    }, {
        result_namespace => 'Result',
        rel_name_map => { QuuxBaz => 'bazrel2' },
    });

    # now upgrade the schema to current but downgrade to load_classes
    $res = run_loader(
        static => 1,
        naming => 'current',
        use_namespaces => 0,
    );
    my $schema = $res->{schema};

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on upgrading static schema (with "naming" set and ' .
'use_namespaces => 0)';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on upgrading static schema (with "naming" set and ' .
'use_namespaces => 0)';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on upgrading static schema (with "naming" set)'
        or diag @{ $res->{warnings} };

    run_v7_tests($res);

    is result_count(), $RESULT_COUNT,
'un-singularized results were replaced during upgrade and Result dir removed';

    ok ((not -d result_dir('Result')),
        'Result dir was removed for load_classes downgrade');

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Quux',
        'load_classes in upgraded mode';

    # check that custom and external content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->b_method, 'dongs' }
        'custom content was carried over from un-singularized Result';

    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'hlagh' }
        'external content was carried over from un-singularized Result';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel2,
        $res->{classes}{bazs} }
        'unsingularized class names in custom content are translated';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel,
        $res->{classes}{bazs} }
        'unsingularized class names in external content are translated';

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel,
        $res->{classes}{foos} }
'unsingularized class names in external content from unchanged Result class ' .
'names are translated in static schema';

    class_content_like $schema, $res->{classes}{quuxs}, qr/sub a_method { 'hlagh' }/,
'external content from unsingularized Result loaded into static dump correctly';

    class_content_like $schema, $res->{classes}{quuxs}, qr/sub b_method { 'dongs' }/,
'custom   content from unsingularized Result loaded into static dump correctly';
}

# test a regular schema with use_namespaces => 0 upgraded to
# use_namespaces => 1
{
    my $res = run_loader(
        clean_dumpdir => 1,
        static => 1,
        use_namespaces => 0,
        naming => 'current',
    );

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on dumping static schema with use_namespaces => 0';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on dumping static schema with use_namespaces => 0';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on dumping static schema with use_namespaces => 0'
        or diag @{ $res->{warnings} };

    run_v7_tests($res);

    my $schema   = $res->{schema};
    add_custom_content($res->{schema}, {
        Quux => 'Baz'
    });

    # test that with no use_namespaces option, there is a warning and
    # load_classes is preserved
    $res = run_loader(static => 1, naming => 'current');

    like $res->{warnings}[0], qr/load_classes/i,
'correct warnings on re-dumping static schema with load_classes';

    like $res->{warnings}[1], qr/Dumping manual schema/i,
'correct warnings on re-dumping static schema with load_classes';

    like $res->{warnings}[2], qr/dump completed/i,
'correct warnings on re-dumping static schema with load_classes';

    is scalar @{ $res->{warnings} }, 3,
'correct number of warnings on re-dumping static schema with load_classes'
        or diag @{ $res->{warnings} };

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Quux',
        'load_classes preserved on re-dump';

    run_v7_tests($res);

    # now upgrade the schema to use_namespaces
    $res = run_loader(
        static => 1,
        use_namespaces => 1,
        naming => 'current',
    );
    $schema = $res->{schema};

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on upgrading to use_namespaces';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on upgrading to use_namespaces';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on upgrading to use_namespaces'
        or diag @{ $res->{warnings} };

    run_v7_tests($res);

    my @schema_files = schema_files();

    is 1, (scalar @schema_files),
        "schema dir contains only 1 entry";

    like $schema_files[0], qr{/Result\z},
        "schema dir contains only a Result/ directory";

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->b_method, 'dongs' }
        'custom content was carried over during use_namespaces upgrade';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel,
        $res->{classes}{bazs} }
        'un-namespaced class names in custom content are translated';

    class_content_like $schema, $res->{classes}{quuxs}, qr/sub b_method { 'dongs' }/,
'custom content from un-namespaced Result loaded into static dump correctly';
}

# test a regular schema with default use_namespaces => 1, redump, and downgrade
# to load_classes
{
    my $res = run_loader(clean_dumpdir => 1, static => 1, naming => 'current');

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on dumping static schema';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on dumping static schema';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on dumping static schema'
        or diag @{ $res->{warnings} };

    run_v7_tests($res);

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Result::Quux',
        'defaults to use_namespaces on regular dump';

    add_custom_content($res->{schema}, { Quux => 'Baz' }, { result_namespace => 'Result' });

    # test that with no use_namespaces option, use_namespaces is preserved
    $res = run_loader(static => 1, naming => 'current');

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on re-dumping static schema';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on re-dumping static schema';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on re-dumping static schema'
        or diag @{ $res->{warnings} };

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Result::Quux',
        'use_namespaces preserved on re-dump';

    run_v7_tests($res);

    # now downgrade the schema to load_classes
    $res = run_loader(
        static => 1,
        use_namespaces => 0,
        naming => 'current',
    );
    my $schema = $res->{schema};

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on downgrading to load_classes';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on downgrading to load_classes';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on downgrading to load_classes'
        or diag @{ $res->{warnings} };

    run_v7_tests($res);

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Quux',
        'load_classes downgrade correct';

    is result_count(), $RESULT_COUNT,
'correct number of Results after upgrade and Result dir removed';

    ok ((not -d result_dir('Result')),
        'Result dir was removed for load_classes downgrade');

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->b_method, 'dongs' }
        'custom content was carried over during load_classes downgrade';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel,
        $res->{classes}{bazs} }
'namespaced class names in custom content are translated during load_classes '.
'downgrade';

    class_content_like $schema, $res->{classes}{quuxs}, qr/sub b_method { 'dongs' }/,
'custom content from namespaced Result loaded into static dump correctly '.
'during load_classes downgrade';
}

# test a regular schema with use_namespaces => 1 and a custom result_namespace
# downgraded to load_classes
{
    my $res = run_loader(
        clean_dumpdir => 1,
        static => 1,
        result_namespace => 'MyResult',
        naming => 'current',
    );

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on dumping static schema';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on dumping static schema';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on dumping static schema'
        or diag @{ $res->{warnings} };

    run_v7_tests($res);

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::MyResult::Quux',
        'defaults to use_namespaces and uses custom result_namespace';

    add_custom_content($res->{schema}, { Quux => 'Baz' }, { result_namespace => 'MyResult' });

    # test that with no use_namespaces option, use_namespaces is preserved, and
    # the custom result_namespace is preserved
    $res = run_loader(static => 1, naming => 'current');

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on re-dumping static schema';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on re-dumping static schema';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on re-dumping static schema'
        or diag @{ $res->{warnings} };

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::MyResult::Quux',
        'use_namespaces and custom result_namespace preserved on re-dump';

    run_v7_tests($res);

    # now downgrade the schema to load_classes
    $res = run_loader(
        static => 1,
        use_namespaces => 0,
        naming => 'current',
    );
    my $schema = $res->{schema};

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on downgrading to load_classes';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on downgrading to load_classes';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on downgrading to load_classes'
        or diag @{ $res->{warnings} };

    run_v7_tests($res);

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Quux',
        'load_classes downgrade correct';

    is result_count(), $RESULT_COUNT,
'correct number of Results after upgrade and Result dir removed';

    ok ((not -d result_dir('MyResult')),
        'Result dir was removed for load_classes downgrade');

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->b_method, 'dongs' }
        'custom content was carried over during load_classes downgrade';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel,
        $res->{classes}{bazs} }
'namespaced class names in custom content are translated during load_classes '.
'downgrade';

    class_content_like $schema, $res->{classes}{quuxs}, qr/sub b_method { 'dongs' }/,
'custom content from namespaced Result loaded into static dump correctly '.
'during load_classes downgrade';
}

# rewrite from one result_namespace to another, with external content
{
    clean_dumpdir();
    my $temp_dir = setup_load_external({ Quux => 'Baz', Bar => 'Foo' }, { result_namespace => 'Result' });

    my $res = run_loader(static => 1, naming => 'current');

    # add some custom content to a Result that will be replaced
    add_custom_content($res->{schema}, { Quux => 'Baz' }, { result_namespace => 'Result', rel_name_map => { QuuxBaz => 'bazrel2' } });

    # Rewrite implicit 'Result' to 'MyResult'
    $res = run_loader(
        static => 1,
        result_namespace => 'MyResult',
        naming => 'current',
    );
    my $schema = $res->{schema};

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::MyResult::Quux',
        'using new result_namespace';

    is result_count('MyResult'), $RESULT_COUNT,
'correct number of Results after rewritten result_namespace';

    ok ((not -d schema_dir('Result')),
        'original Result dir was removed when rewriting result_namespace');

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->b_method, 'dongs' }
        'custom content was carried over when rewriting result_namespace';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel2,
        $res->{classes}{bazs} }
'class names in custom content are translated when rewriting result_namespace';

    class_content_like $schema, $res->{classes}{quuxs}, qr/sub b_method { 'dongs' }/,
'custom content from namespaced Result loaded into static dump correctly '.
'when rewriting result_namespace';

    # Now rewrite 'MyResult' to 'Mtfnpy'
    $res = run_loader(
        static => 1,
        result_namespace => 'Mtfnpy',
        naming => 'current',
    );
    $schema = $res->{schema};

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Mtfnpy::Quux',
        'using new result_namespace';

    is result_count('Mtfnpy'), $RESULT_COUNT,
'correct number of Results after rewritten result_namespace';

    ok ((not -d result_dir('MyResult')),
        'original Result dir was removed when rewriting result_namespace');

    # check that custom and external content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'hlagh' }
        'external content was carried over when rewriting result_namespace';

    lives_and { is $schema->resultset('Quux')->find(1)->b_method, 'dongs' }
        'custom content was carried over when rewriting result_namespace';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel2,
        $res->{classes}{bazs} }
'class names in custom content are translated when rewriting result_namespace';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel,
        $res->{classes}{bazs} }
'class names in external content are translated when rewriting '.
'result_namespace';

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel,
        $res->{classes}{foos} }
'class names in external content are translated when rewriting '.
'result_namespace';

    class_content_like $schema, $res->{classes}{quuxs}, qr/sub b_method { 'dongs' }/,
'custom content from namespaced Result loaded into static dump correctly '.
'when rewriting result_namespace';

    class_content_like $schema, $res->{classes}{quuxs}, qr/sub a_method { 'hlagh' }/,
'external content from unsingularized Result loaded into static dump correctly';
}

# test upgrading a v4 schema, then check that the version string is correct
{
    clean_dumpdir();
    write_v4_schema_pm();
    run_loader(static => 1);
    my $res = run_loader(static => 1, naming => 'current');
    my $schema = $res->{schema};

    my $file = $schema->loader->get_dump_filename($SCHEMA_CLASS);
    my $code = slurp_file $file;

    my ($dumped_ver) =
        $code =~ /^# Created by DBIx::Class::Schema::Loader v(\S+)/m;

    is $dumped_ver, $DBIx::Class::Schema::Loader::VERSION,
        'correct version dumped after upgrade of v4 static schema';
}

# Test upgrading an already singular result with custom content that refers to
# old class names.
{
    clean_dumpdir();
    write_v4_schema_pm();
    my $res = run_loader(static => 1);
    my $schema = $res->{schema};
    run_v4_tests($res);

    # add some custom content to a Result that will be replaced
    add_custom_content($schema, { Bar => 'Foos' });

    # now upgrade the schema
    $res = run_loader(static => 1, naming => 'current');
    $schema = $res->{schema};
    run_v7_tests($res);

    # check that custom content was preserved
    lives_and { is $schema->resultset('Bar')->find(1)->b_method, 'dongs' }
        'custom content was preserved from Result pre-upgrade';

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel,
        $res->{classes}{foos} }
'unsingularized class names in custom content from Result with unchanged ' .
'name are translated';

    class_content_like $schema, $res->{classes}{bar}, qr/sub b_method { 'dongs' }/,
'custom content from Result with unchanged name loaded into static dump ' .
'correctly';
}

# test creating static schema in v5 mode then upgrade to current with external
# content loaded
{
    clean_dumpdir();

    write_v5_schema_pm();

    my $res = run_loader(static => 1);

    like $res->{warnings}[0], qr/0.05003 static schema/, 'backcompat warning';

    run_v5_tests($res);

    my $temp_dir = setup_load_external({
        Baz => 'StationsVisited',
        StationsVisited => 'Quux',
    }, { result_namespace => 'Result' });

    add_custom_content($res->{schema}, {
        Baz => 'StationsVisited',
    }, {
        result_namespace => 'Result',
        rel_name_map => { BazStationsvisited => 'custom_content_rel' },
    });

    $res = run_loader(static => 1, naming => 'current');
    my $schema = $res->{schema};

    run_v7_tests($res);

    lives_and { is $schema->resultset('Baz')->find(1)->a_method, 'hlagh' }
        'external custom content loaded for v5 -> v6';

    lives_and { isa_ok $schema->resultset('Baz')->find(1)->stationsvisitedrel,
        $res->{classes}{stations_visited} }
        'external content rewritten for v5 -> v6';

    lives_and { isa_ok $schema->resultset('Baz')->find(1)->custom_content_rel,
        $res->{classes}{stations_visited} }
        'custom content rewritten for v5 -> v6';

    lives_and { isa_ok $schema->resultset('StationVisited')->find(1)->quuxrel,
        $res->{classes}{quuxs} }
        'external content rewritten for v5 -> v6 for upgraded Result class names';
}

# test creating static schema in v6 mode then upgrade to current with external
# content loaded
{
    clean_dumpdir();

    write_v6_schema_pm();

    my $res = run_loader(static => 1);

    like $res->{warnings}[0], qr/0.06001 static schema/, 'backcompat warning';

    run_v6_tests($res);

    my $temp_dir = setup_load_external({
        Routechange => 'Quux',
    }, { result_namespace => 'Result' });

    add_custom_content($res->{schema}, {
        Routechange => 'Quux',
    }, {
        result_namespace => 'Result',
        rel_name_map => { RoutechangeQuux => 'custom_content_rel' },
    });

    $res = run_loader(static => 1, naming => 'current');
    my $schema = $res->{schema};

    run_v7_tests($res);

    lives_and { is $schema->resultset('RouteChange')->find(1)->a_method, 'hlagh' }
        'external custom content loaded for v6 -> v7';

    lives_and { isa_ok $schema->resultset('RouteChange')->find(1)->quuxrel,
        $res->{classes}{quuxs} }
        'external content rewritten for v6 -> v7';

    lives_and { isa_ok $schema->resultset('RouteChange')->find(1)->custom_content_rel,
        $res->{classes}{quuxs} }
        'custom content rewritten for v6 -> v7';
}

done_testing;

END {
    rmtree $DUMP_DIR unless $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP};
}

sub clean_dumpdir {
    rmtree $DUMP_DIR;
    make_path $DUMP_DIR;
}

sub run_loader {
    my %loader_opts = @_;

    $loader_opts{dump_directory} = $DUMP_DIR if delete $loader_opts{static};
    $loader_opts{preserve_case}  = 1 if $loader_opts{naming} && $loader_opts{naming} eq 'current';

    clean_dumpdir() if delete $loader_opts{clean_dumpdir};

    eval {
        foreach my $source_name ($SCHEMA_CLASS->clone->sources) {
            Class::Unload->unload("${SCHEMA_CLASS}::${source_name}");
        }

        Class::Unload->unload($SCHEMA_CLASS);
    };
    undef $@;

    my @connect_info = $make_dbictest_db_with_unique::dsn;
    my @loader_warnings;
    local $SIG{__WARN__} = sub { push(@loader_warnings, @_); };
    eval qq{
        package $SCHEMA_CLASS;
        use base qw/DBIx::Class::Schema::Loader/;

        __PACKAGE__->loader_options(\%loader_opts);
        __PACKAGE__->connection(\@connect_info);
    };

    ok(!$@, "Loader initialization") or diag $@;

    my $schema = $SCHEMA_CLASS->clone;
    my (%monikers, %classes);
    foreach my $source_name ($schema->sources) {
        my $table_name = $schema->source($source_name)->from;
        $monikers{$table_name} = $source_name;
        $classes{$table_name}  = $schema->source($source_name)->result_class;
    }

    return {
        schema => $schema,
        warnings => \@loader_warnings,
        monikers => \%monikers,
        classes => \%classes,
    };
}

sub write_v4_schema_pm {
    my %opts = @_;

    (my $schema_dir = "$DUMP_DIR/$SCHEMA_CLASS") =~ s/::[^:]+\z//;
    rmtree $schema_dir;
    make_path $schema_dir;
    my $schema_pm = "$schema_dir/Schema.pm";
    open my $fh, '>', $schema_pm or die $!;
    if (not $opts{use_namespaces}) {
        print $fh <<'EOF';
package DBIXCSL_Test::Schema;

use strict;
use warnings;

use base 'DBIx::Class::Schema';

__PACKAGE__->load_classes;


# Created by DBIx::Class::Schema::Loader v0.04006 @ 2009-12-25 01:49:25
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:ibIJTbfM1ji4pyD/lgSEog


# You can replace this text with custom content, and it will be preserved on regeneration
1;
EOF
    }
    else {
        print $fh <<'EOF';
package DBIXCSL_Test::Schema;

use strict;
use warnings;

use base 'DBIx::Class::Schema';

__PACKAGE__->load_namespaces;


# Created by DBIx::Class::Schema::Loader v0.04006 @ 2010-01-12 16:04:12
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:d3wRVsHBNisyhxeaWJZcZQ


# You can replace this text with custom content, and it will be preserved on
# regeneration
1;
EOF
    }
}

sub write_v5_schema_pm {
    my %opts = @_;

    (my $schema_dir = "$DUMP_DIR/$SCHEMA_CLASS") =~ s/::[^:]+\z//;
    rmtree $schema_dir;
    make_path $schema_dir;
    my $schema_pm = "$schema_dir/Schema.pm";
    open my $fh, '>', $schema_pm or die $!;
    if (exists $opts{use_namespaces} && $opts{use_namespaces} == 0) {
        print $fh <<'EOF';
package DBIXCSL_Test::Schema;

# Created by DBIx::Class::Schema::Loader
# DO NOT MODIFY THE FIRST PART OF THIS FILE

use strict;
use warnings;

use base 'DBIx::Class::Schema';

__PACKAGE__->load_classes;


# Created by DBIx::Class::Schema::Loader v0.05003 @ 2010-03-27 17:07:37
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:LIzC/LT5IYvWpgusfbqMrg


# You can replace this text with custom content, and it will be preserved on regeneration
1;
EOF
    }
    else {
        print $fh <<'EOF';
package DBIXCSL_Test::Schema;

# Created by DBIx::Class::Schema::Loader
# DO NOT MODIFY THE FIRST PART OF THIS FILE

use strict;
use warnings;

use base 'DBIx::Class::Schema';

__PACKAGE__->load_namespaces;


# Created by DBIx::Class::Schema::Loader v0.05003 @ 2010-03-29 19:44:52
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:D+MYxtGxz97Ghvido5DTEg


# You can replace this text with custom content, and it will be preserved on regeneration
1;
EOF
    }
}

sub write_v6_schema_pm {
    my %opts = @_;

    (my $schema_dir = "$DUMP_DIR/$SCHEMA_CLASS") =~ s/::[^:]+\z//;
    rmtree $schema_dir;
    make_path $schema_dir;
    my $schema_pm = "$schema_dir/Schema.pm";
    open my $fh, '>', $schema_pm or die $!;
    if (exists $opts{use_namespaces} && $opts{use_namespaces} == 0) {
        print $fh <<'EOF';
package DBIXCSL_Test::Schema;

# Created by DBIx::Class::Schema::Loader
# DO NOT MODIFY THE FIRST PART OF THIS FILE

use strict;
use warnings;

use base 'DBIx::Class::Schema';

__PACKAGE__->load_classes;


# Created by DBIx::Class::Schema::Loader v0.06001 @ 2010-04-21 19:56:03
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:/fqZCb95hsGIe1g5qyQQZg


# You can replace this text with custom content, and it will be preserved on regeneration
1;
EOF
    }
    else {
        print $fh <<'EOF';
package DBIXCSL_Test::Schema;

# Created by DBIx::Class::Schema::Loader
# DO NOT MODIFY THE FIRST PART OF THIS FILE

use strict;
use warnings;

use base 'DBIx::Class::Schema';

__PACKAGE__->load_namespaces;


# Created by DBIx::Class::Schema::Loader v0.06001 @ 2010-04-21 19:54:31
# DO NOT MODIFY THIS OR ANYTHING ABOVE! md5sum:nwO5Vi47kl0X9SpEoiVO5w


# You can replace this text with custom content, and it will be preserved on regeneration
1;
EOF
    }
}

sub run_v4_tests {
    my $res = shift;
    my $schema = $res->{schema};

    is_deeply [ @{ $res->{monikers} }{qw/foos bar bazs quuxs stations_visited RouteChange email/} ],
        [qw/Foos Bar Bazs Quuxs StationsVisited Routechange Email/],
        'correct monikers in 0.04006 mode';

    isa_ok ((my $bar = eval { $schema->resultset('Bar')->find(1) }),
        $res->{classes}{bar},
        'found a bar');

    isa_ok eval { $bar->foo_id }, $res->{classes}{foos},
        'correct rel name in 0.04006 mode';

    ok my $baz  = eval { $schema->resultset('Bazs')->find(1) };

    isa_ok eval { $baz->quux }, 'DBIx::Class::ResultSet',
        'correct rel type and name for UNIQUE FK in 0.04006 mode';

    ok my $foo = eval { $schema->resultset('Foos')->find(1) };

    isa_ok eval { $foo->email_to_ids }, 'DBIx::Class::ResultSet',
        'correct rel name inflection in 0.04006 mode';

    ok (($schema->resultset('Routechange')->find(1)->can('quuxsid')),
        'correct column accessor in 0.04006 mode');

    is $schema->resultset('Routechange')->find(1)->foo2bar, 3,
        'correct column accessor for column with word ending with digit in v4 mode';
}

sub run_v5_tests {
    my $res = shift;
    my $schema = $res->{schema};

    is_deeply [ @{ $res->{monikers} }{qw/foos bar bazs quuxs stations_visited RouteChange email/} ],
        [qw/Foo Bar Baz Quux StationsVisited Routechange Email/],
        'correct monikers in v5 mode';

    ok my $bar = eval { $schema->resultset('Bar')->find(1) };

    isa_ok eval { $bar->foo }, $res->{classes}{foos},
        'correct rel name in v5 mode';

    ok my $baz  = eval { $schema->resultset('Baz')->find(1) };

    isa_ok eval { $baz->quux }, $res->{classes}{quuxs},
        'correct rel type and name for UNIQUE FK in v5 mode';

    ok my $foo = eval { $schema->resultset('Foo')->find(1) };

    isa_ok eval { $foo->email_to_ids }, 'DBIx::Class::ResultSet',
        'correct rel name inflection in v5 mode';

    ok (($schema->resultset('Routechange')->find(1)->can('quuxsid')),
        'correct column accessor in v5 mode');

    is $schema->resultset('Routechange')->find(1)->foo2bar, 3,
        'correct column accessor for column with word ending with digit in v5 mode';
}

sub run_v6_tests {
    my $res = shift;
    my $schema = $res->{schema};

    is_deeply [ @{ $res->{monikers} }{qw/foos bar bazs quuxs stations_visited RouteChange email/} ],
        [qw/Foo Bar Baz Quux StationVisited Routechange Email/],
        'correct monikers in v6 mode';

    ok my $bar = eval { $schema->resultset('Bar')->find(1) };

    isa_ok eval { $bar->foo }, $res->{classes}{foos},
        'correct rel name in v6 mode';

    ok my $baz  = eval { $schema->resultset('Baz')->find(1) };

    isa_ok eval { $baz->quux }, $res->{classes}{quuxs},
        'correct rel type and name for UNIQUE FK in v6 mode';

    ok my $foo = eval { $schema->resultset('Foo')->find(1) };

    isa_ok eval { $foo->emails_to }, 'DBIx::Class::ResultSet',
        'correct rel name inflection in v6 mode';

    ok my $route_change = eval { $schema->resultset('Routechange')->find(1) };

    isa_ok eval { $route_change->quuxsid }, $res->{classes}{quuxs},
        'correct rel name in v6 mode';

    ok (($schema->resultset('Routechange')->find(1)->can('quuxsid')),
        'correct column accessor in v6 mode');

    is $schema->resultset('Routechange')->find(1)->foo2bar, 3,
        'correct column accessor for column with word ending with digit in v6 mode';
}

sub run_v7_tests {
    my $res = shift;
    my $schema = $res->{schema};

    is_deeply [ @{ $res->{monikers} }{qw/foos bar bazs quuxs stations_visited RouteChange email/} ],
        [qw/Foo Bar Baz Quux StationVisited RouteChange Email/],
        'correct monikers in current mode';

    ok my $bar = eval { $schema->resultset('Bar')->find(1) };

    isa_ok eval { $bar->foo }, $res->{classes}{foos},
        'correct rel name in current mode';

    ok my $baz  = eval { $schema->resultset('Baz')->find(1) };

    isa_ok eval { $baz->quux }, $res->{classes}{quuxs},
        'correct rel type and name for UNIQUE FK in current mode';

    ok my $foo = eval { $schema->resultset('Foo')->find(1) };

    isa_ok eval { $foo->emails_to }, 'DBIx::Class::ResultSet',
        'correct rel name inflection in current mode';

    ok my $route_change = eval { $schema->resultset('RouteChange')->find(1) };

    isa_ok eval { $route_change->quux }, $res->{classes}{quuxs},
        'correct rel name based on mixed-case column name in current mode';

    ok (($schema->resultset('RouteChange')->find(1)->can('quuxs_id')),
        'correct column accessor in current mode');

    is $schema->resultset('RouteChange')->find(1)->foo2_bar, 3,
        'correct column accessor for column with word ending with digit in current mode';
}

{
    package DBICSL::Test::TempExtDir;

    use overload '""' => sub { ${$_[0]} };

    sub DESTROY {
        pop @INC;
        File::Path::rmtree ${$_[0]};
    }
}

sub setup_load_external {
    my ($rels, $opts) = @_;

    my $temp_dir = tempdir(CLEANUP => 1);
    push @INC, $temp_dir;

    my $external_result_dir = join '/', $temp_dir, (split /::/, $SCHEMA_CLASS),
        ($opts->{result_namespace} || ());

    make_path $external_result_dir;

    while (my ($from, $to) = each %$rels) {
        write_ext_result($external_result_dir, $from, $to, $opts);
    }

    my $guard = bless \$temp_dir, 'DBICSL::Test::TempExtDir';

    return $guard;
}

sub write_ext_result {
    my ($result_dir, $from, $to, $opts) = @_;

    my $relname    = $opts->{rel_name_map}{_rel_key($from, $to)} || _relname($to);
    my $from_class = _qualify_class($from, $opts->{result_namespace});
    my $to_class   = _qualify_class($to,   $opts->{result_namespace});
    my $condition  = _rel_condition($from, $to);

    IO::File->new(">$result_dir/${from}.pm")->print(<<"EOF");
package ${from_class};
sub a_method { 'hlagh' }

__PACKAGE__->has_one('$relname', '$to_class',
{ $condition });

1;
EOF

    return $relname;
}

sub _relname {
    my $to = shift;

    return Lingua::EN::Inflect::Number::to_S(lc $to) . 'rel';
}

sub _qualify_class {
    my ($class, $result_namespace) = @_;

    return $SCHEMA_CLASS . '::'
        . ($result_namespace ? $result_namespace . '::' : '')
        . $class;
}

sub _rel_key {
    my ($from, $to) = @_;

    return join '', map ucfirst(Lingua::EN::Inflect::Number::to_S(lc($_))), $from, $to;
}

sub _rel_condition {
    my ($from, $to) = @_;

    return +{
        QuuxBaz => q{'foreign.baz_num' => 'self.baz_id'},
        BarFoo  => q{'foreign.fooid'   => 'self.foo_id'},
        BazStationsvisited => q{'foreign.id' => 'self.stations_visited_id'},
        StationsvisitedQuux => q{'foreign.quuxid' => 'self.quuxs_id'},
        RoutechangeQuux => q{'foreign.quuxid' => 'self.QuuxsId'},
    }->{_rel_key($from, $to)};
}

sub class_content_like {
    my ($schema, $class, $re, $test_name) = @_;

    my $file = $schema->loader->get_dump_filename($class);
    my $code = slurp_file $file;

    like $code, $re, $test_name;
}

sub add_custom_content {
    my ($schema, $rels, $opts) = @_;

    while (my ($from, $to) = each %$rels) {
        my $relname    = $opts->{rel_name_map}{_rel_key($from, $to)} || _relname($to);
        my $from_class = _qualify_class($from, $opts->{result_namespace});
        my $to_class   = _qualify_class($to,   $opts->{result_namespace});
        my $condition  = _rel_condition($from, $to);

        my $content = <<"EOF";
package ${from_class};
sub b_method { 'dongs' }

__PACKAGE__->has_one('$relname', '$to_class',
{ $condition });

1;
EOF

        _write_custom_content($schema, $from_class, $content);
    }
}

sub _write_custom_content {
    my ($schema, $class, $content) = @_;

    my $pm = $schema->loader->get_dump_filename($class);
    {
        local ($^I, @ARGV) = ('.bak', $pm);
        while (<>) {
            if (/DO NOT MODIFY THIS OR ANYTHING ABOVE/) {
                print;
                print $content;
            }
            else {
                print;
            }
        }
        close ARGV;
        unlink "${pm}.bak" or die $^E;
    }
}

sub result_count {
    my $path = shift || '';

    my $dir = result_dir($path);

    my $file_count =()= glob "$dir/*";

    return $file_count;
}

sub result_files {
    my $path = shift || '';

    my $dir = result_dir($path);

    return glob "$dir/*";
}

sub schema_files { result_files(@_) }

sub result_dir {
    my $path = shift || '';

    (my $dir = "$DUMP_DIR/$SCHEMA_CLASS/$path") =~ s{::}{/}g;
    $dir =~ s{/+\z}{};

    return $dir;
}

sub schema_dir { result_dir(@_) }

# vim:et sts=4 sw=4 tw=0:
