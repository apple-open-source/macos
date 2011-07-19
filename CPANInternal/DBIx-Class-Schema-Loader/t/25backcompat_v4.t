use strict;
use warnings;
use Test::More;
use Test::Exception;
use File::Path qw/rmtree make_path/;
use Class::Unload;
use File::Temp qw/tempfile tempdir/;
use IO::File;
use File::Slurp 'slurp';
use DBIx::Class::Schema::Loader ();
use lib qw(t/lib);
use make_dbictest_db2;

my $DUMP_DIR = './t/_common_dump';
rmtree $DUMP_DIR;
my $SCHEMA_CLASS = 'DBIXCSL_Test::Schema';

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
    run_v5_tests($res);
}

# test upgraded dynamic schema with external content loaded
{
    my $temp_dir = tempdir(CLEANUP => 1);
    push @INC, $temp_dir;

    my $external_result_dir = join '/', $temp_dir, split /::/, $SCHEMA_CLASS;
    make_path $external_result_dir;

    # make external content for Result that will be singularized
    IO::File->new(">$external_result_dir/Quuxs.pm")->print(<<"EOF");
package ${SCHEMA_CLASS}::Quuxs;
sub a_method { 'hlagh' }

__PACKAGE__->has_one('bazrel', 'DBIXCSL_Test::Schema::Bazs',
    { 'foreign.baz_num' => 'self.baz_id' });

1;
EOF

    # make external content for Result that will NOT be singularized
    IO::File->new(">$external_result_dir/Bar.pm")->print(<<"EOF");
package ${SCHEMA_CLASS}::Bar;

__PACKAGE__->has_one('foorel', 'DBIXCSL_Test::Schema::Foos',
    { 'foreign.fooid' => 'self.foo_id' });

1;
EOF

    my $res = run_loader(naming => 'current');
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

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel,
        $res->{classes}{foos} }
'unsingularized class names in external content from unchanged Result class ' .
'names are translated';

    run_v5_tests($res);

    pop @INC;
}

# test upgraded dynamic schema with use_namespaces with external content loaded
{
    my $temp_dir = tempdir(CLEANUP => 1);
    push @INC, $temp_dir;

    my $external_result_dir = join '/', $temp_dir, split /::/, $SCHEMA_CLASS;
    make_path $external_result_dir;

    # make external content for Result that will be singularized
    IO::File->new(">$external_result_dir/Quuxs.pm")->print(<<"EOF");
package ${SCHEMA_CLASS}::Quuxs;
sub a_method { 'hlagh' }

__PACKAGE__->has_one('bazrel4', 'DBIXCSL_Test::Schema::Bazs',
    { 'foreign.baz_num' => 'self.baz_id' });

1;
EOF

    # make external content for Result that will NOT be singularized
    IO::File->new(">$external_result_dir/Bar.pm")->print(<<"EOF");
package ${SCHEMA_CLASS}::Bar;

__PACKAGE__->has_one('foorel4', 'DBIXCSL_Test::Schema::Foos',
    { 'foreign.fooid' => 'self.foo_id' });

1;
EOF

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

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel4,
        $res->{classes}{bazs} }
        'unsingularized class names in external content are translated';

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel4,
        $res->{classes}{foos} }
'unsingularized class names in external content from unchanged Result class ' .
'names are translated';

    run_v5_tests($res);

    pop @INC;
}


# test upgraded static schema with external content loaded
{
    my $temp_dir = tempdir(CLEANUP => 1);
    push @INC, $temp_dir;

    my $external_result_dir = join '/', $temp_dir, split /::/, $SCHEMA_CLASS;
    make_path $external_result_dir;

    # make external content for Result that will be singularized
    IO::File->new(">$external_result_dir/Quuxs.pm")->print(<<"EOF");
package ${SCHEMA_CLASS}::Quuxs;
sub a_method { 'dongs' }

__PACKAGE__->has_one('bazrel2', 'DBIXCSL_Test::Schema::Bazs',
    { 'foreign.baz_num' => 'self.baz_id' });

1;
EOF

    # make external content for Result that will NOT be singularized
    IO::File->new(">$external_result_dir/Bar.pm")->print(<<"EOF");
package ${SCHEMA_CLASS}::Bar;

__PACKAGE__->has_one('foorel2', 'DBIXCSL_Test::Schema::Foos',
    { 'foreign.fooid' => 'self.foo_id' });

1;
EOF

    write_v4_schema_pm();

    my $res = run_loader(dump_directory => $DUMP_DIR, naming => 'current');
    my $schema = $res->{schema};

    run_v5_tests($res);

    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'dongs' }
'external custom content for unsingularized Result was loaded by upgraded ' .
'static Schema';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel2,
        $res->{classes}{bazs} }
        'unsingularized class names in external content are translated';

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel2,
        $res->{classes}{foos} }
'unsingularized class names in external content from unchanged Result class ' .
'names are translated in static schema';

    my $file = $schema->_loader->_get_dump_filename($res->{classes}{quuxs});
    my $code = slurp $file;

    like $code, qr/package ${SCHEMA_CLASS}::Quux;/,
'package line translated correctly from external custom content in static dump';

    like $code, qr/sub a_method { 'dongs' }/,
'external custom content loaded into static dump correctly';

    pop @INC;
}

# test running against v4 schema without upgrade, twice, then upgrade
{
    write_v4_schema_pm();
    my $res = run_loader(dump_directory => $DUMP_DIR);
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

    # add some custom content to a Result that will be replaced
    my $schema   = $res->{schema};
    my $quuxs_pm = $schema->_loader
        ->_get_dump_filename($res->{classes}{quuxs});
    {
        local ($^I, @ARGV) = ('.bak', $quuxs_pm);
        while (<>) {
            if (/DO NOT MODIFY THIS OR ANYTHING ABOVE/) {
                print;
                print <<EOF;
sub a_method { 'mtfnpy' }

__PACKAGE__->has_one('bazrel3', 'DBIXCSL_Test::Schema::Bazs',
    { 'foreign.baz_num' => 'self.baz_id' });
EOF
            }
            else {
                print;
            }
        }
        close ARGV;
        unlink "${quuxs_pm}.bak" or die $^E;
    }

    # Rerun the loader in backcompat mode to make sure it's still in backcompat
    # mode.
    $res = run_loader(dump_directory => $DUMP_DIR);
    run_v4_tests($res);

    # now upgrade the schema
    $res = run_loader(
        dump_directory => $DUMP_DIR,
        naming => 'current',
        use_namespaces => 1
    );
    $schema = $res->{schema};

    like $res->{warnings}[0], qr/Dumping manual schema/i,
        'correct warnings on upgrading static schema (with "naming" set)';

    like $res->{warnings}[1], qr/dump completed/i,
        'correct warnings on upgrading static schema (with "naming" set)';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on upgrading static schema (with "naming" set)'
        or diag @{ $res->{warnings} };

    run_v5_tests($res);

    (my $result_dir = "$DUMP_DIR/$SCHEMA_CLASS/Result") =~ s{::}{/}g;
    my $result_count =()= glob "$result_dir/*";

    is $result_count, 4,
        'un-singularized results were replaced during upgrade';

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'mtfnpy' }
        'custom content was carried over from un-singularized Result';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel3,
        $res->{classes}{bazs} }
        'unsingularized class names in custom content are translated';

    my $file = $schema->_loader->_get_dump_filename($res->{classes}{quuxs});
    my $code = slurp $file;

    like $code, qr/sub a_method { 'mtfnpy' }/,
'custom content from unsingularized Result loaded into static dump correctly';
}

# test running against v4 schema without upgrade, then upgrade with
# use_namespaces not explicitly set
{
    write_v4_schema_pm();
    my $res = run_loader(dump_directory => $DUMP_DIR);
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

    # add some custom content to a Result that will be replaced
    my $schema   = $res->{schema};
    my $quuxs_pm = $schema->_loader
        ->_get_dump_filename($res->{classes}{quuxs});
    {
        local ($^I, @ARGV) = ('.bak', $quuxs_pm);
        while (<>) {
            if (/DO NOT MODIFY THIS OR ANYTHING ABOVE/) {
                print;
                print <<EOF;
sub a_method { 'mtfnpy' }

__PACKAGE__->has_one('bazrel5', 'DBIXCSL_Test::Schema::Bazs',
    { 'foreign.baz_num' => 'self.baz_id' });
EOF
            }
            else {
                print;
            }
        }
        close ARGV;
        unlink "${quuxs_pm}.bak" or die $^E;
    }

    # now upgrade the schema
    $res = run_loader(
        dump_directory => $DUMP_DIR,
        naming => 'current'
    );
    $schema = $res->{schema};

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

    run_v5_tests($res);

    (my $result_dir = "$DUMP_DIR/$SCHEMA_CLASS") =~ s{::}{/}g;
    my $result_count =()= glob "$result_dir/*";

    is $result_count, 4,
        'un-singularized results were replaced during upgrade';

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'mtfnpy' }
        'custom content was carried over from un-singularized Result';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel5,
        $res->{classes}{bazs} }
        'unsingularized class names in custom content are translated';

    my $file = $schema->_loader->_get_dump_filename($res->{classes}{quuxs});
    my $code = slurp $file;

    like $code, qr/sub a_method { 'mtfnpy' }/,
'custom content from unsingularized Result loaded into static dump correctly';
}

# test running against v4 schema with load_namespaces, upgrade to v5 but
# downgrade to load_classes, with external content
{
    my $temp_dir = tempdir(CLEANUP => 1);
    push @INC, $temp_dir;

    my $external_result_dir = join '/', $temp_dir, split /::/,
        "${SCHEMA_CLASS}::Result";

    make_path $external_result_dir;

    # make external content for Result that will be singularized
    IO::File->new(">$external_result_dir/Quuxs.pm")->print(<<"EOF");
package ${SCHEMA_CLASS}::Result::Quuxs;
sub b_method { 'dongs' }

__PACKAGE__->has_one('bazrel11', 'DBIXCSL_Test::Schema::Result::Bazs',
    { 'foreign.baz_num' => 'self.baz_id' });

1;
EOF

    # make external content for Result that will NOT be singularized
    IO::File->new(">$external_result_dir/Bar.pm")->print(<<"EOF");
package ${SCHEMA_CLASS}::Result::Bar;

__PACKAGE__->has_one('foorel5', 'DBIXCSL_Test::Schema::Result::Foos',
    { 'foreign.fooid' => 'self.foo_id' });

1;
EOF

    write_v4_schema_pm(use_namespaces => 1);

    my $res = run_loader(dump_directory => $DUMP_DIR);
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

    # add some custom content to a Result that will be replaced
    my $schema   = $res->{schema};
    my $quuxs_pm = $schema->_loader
        ->_get_dump_filename($res->{classes}{quuxs});
    {
        local ($^I, @ARGV) = ('.bak', $quuxs_pm);
        while (<>) {
            if (/DO NOT MODIFY THIS OR ANYTHING ABOVE/) {
                print;
                print <<EOF;
sub a_method { 'mtfnpy' }

__PACKAGE__->has_one('bazrel6', 'DBIXCSL_Test::Schema::Result::Bazs',
    { 'foreign.baz_num' => 'self.baz_id' });
EOF
            }
            else {
                print;
            }
        }
        close ARGV;
        unlink "${quuxs_pm}.bak" or die $^E;
    }

    # now upgrade the schema to v5 but downgrade to load_classes
    $res = run_loader(
        dump_directory => $DUMP_DIR,
        naming => 'current',
        use_namespaces => 0,
    );
    $schema = $res->{schema};

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on upgrading static schema (with "naming" set and ' .
'use_namespaces => 0)';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on upgrading static schema (with "naming" set and ' .
'use_namespaces => 0)';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on upgrading static schema (with "naming" set)'
        or diag @{ $res->{warnings} };

    run_v5_tests($res);

    (my $result_dir = "$DUMP_DIR/$SCHEMA_CLASS") =~ s{::}{/}g;
    my $result_count =()= glob "$result_dir/*";

    is $result_count, 4,
'un-singularized results were replaced during upgrade and Result dir removed';

    ok ((not -d "$result_dir/Result"),
        'Result dir was removed for load_classes downgrade');

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Quux',
        'load_classes in upgraded mode';

    # check that custom and external content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'mtfnpy' }
        'custom content was carried over from un-singularized Result';

    lives_and { is $schema->resultset('Quux')->find(1)->b_method, 'dongs' }
        'external content was carried over from un-singularized Result';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel6,
        $res->{classes}{bazs} }
        'unsingularized class names in custom content are translated';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel11,
        $res->{classes}{bazs} }
        'unsingularized class names in external content are translated';

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel5,
        $res->{classes}{foos} }
'unsingularized class names in external content from unchanged Result class ' .
'names are translated in static schema';

    my $file = $schema->_loader->_get_dump_filename($res->{classes}{quuxs});
    my $code = slurp $file;

    like $code, qr/sub a_method { 'mtfnpy' }/,
'custom content from unsingularized Result loaded into static dump correctly';

    like $code, qr/sub b_method { 'dongs' }/,
'external content from unsingularized Result loaded into static dump correctly';

    pop @INC;
}

# test a regular schema with use_namespaces => 0 upgraded to
# use_namespaces => 1
{
    rmtree $DUMP_DIR;
    mkdir $DUMP_DIR;

    my $res = run_loader(
        dump_directory => $DUMP_DIR,
        use_namespaces => 0,
    );

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on dumping static schema with use_namespaces => 0';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on dumping static schema with use_namespaces => 0';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on dumping static schema with use_namespaces => 0'
        or diag @{ $res->{warnings} };

    run_v5_tests($res);

    # add some custom content to a Result that will be replaced
    my $schema   = $res->{schema};
    my $quuxs_pm = $schema->_loader
        ->_get_dump_filename($res->{classes}{quuxs});
    {
        local ($^I, @ARGV) = ('.bak', $quuxs_pm);
        while (<>) {
            if (/DO NOT MODIFY THIS OR ANYTHING ABOVE/) {
                print;
                print <<EOF;
sub a_method { 'mtfnpy' }

__PACKAGE__->has_one('bazrel7', 'DBIXCSL_Test::Schema::Baz',
    { 'foreign.baz_num' => 'self.baz_id' });
EOF
            }
            else {
                print;
            }
        }
        close ARGV;
        unlink "${quuxs_pm}.bak" or die $^E;
    }

    # test that with no use_namespaces option, there is a warning and
    # load_classes is preserved
    $res = run_loader(dump_directory => $DUMP_DIR);

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

    run_v5_tests($res);

    # now upgrade the schema to use_namespaces
    $res = run_loader(
        dump_directory => $DUMP_DIR,
        use_namespaces => 1,
    );
    $schema = $res->{schema};

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on upgrading to use_namespaces';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on upgrading to use_namespaces';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on upgrading to use_namespaces'
        or diag @{ $res->{warnings} };

    run_v5_tests($res);

    (my $schema_dir = "$DUMP_DIR/$SCHEMA_CLASS") =~ s{::}{/}g;
    my @schema_files = glob "$schema_dir/*";

    is 1, (scalar @schema_files),
        "schema dir $schema_dir contains only 1 entry";

    like $schema_files[0], qr{/Result\z},
        "schema dir contains only a Result/ directory";

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'mtfnpy' }
        'custom content was carried over during use_namespaces upgrade';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel7,
        $res->{classes}{bazs} }
        'un-namespaced class names in custom content are translated';

    my $file = $schema->_loader->_get_dump_filename($res->{classes}{quuxs});
    my $code = slurp $file;

    like $code, qr/sub a_method { 'mtfnpy' }/,
'custom content from un-namespaced Result loaded into static dump correctly';
}

# test a regular schema with default use_namespaces => 1, redump, and downgrade
# to load_classes
{
    rmtree $DUMP_DIR;
    mkdir $DUMP_DIR;

    my $res = run_loader(dump_directory => $DUMP_DIR);

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on dumping static schema';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on dumping static schema';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on dumping static schema'
        or diag @{ $res->{warnings} };

    run_v5_tests($res);

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Result::Quux',
        'defaults to use_namespaces on regular dump';

    # add some custom content to a Result that will be replaced
    my $schema   = $res->{schema};
    my $quuxs_pm = $schema->_loader
        ->_get_dump_filename($res->{classes}{quuxs});
    {
        local ($^I, @ARGV) = ('.bak', $quuxs_pm);
        while (<>) {
            if (/DO NOT MODIFY THIS OR ANYTHING ABOVE/) {
                print;
                print <<EOF;
sub a_method { 'mtfnpy' }

__PACKAGE__->has_one('bazrel8', 'DBIXCSL_Test::Schema::Result::Baz',
    { 'foreign.baz_num' => 'self.baz_id' });
EOF
            }
            else {
                print;
            }
        }
        close ARGV;
        unlink "${quuxs_pm}.bak" or die $^E;
    }

    # test that with no use_namespaces option, use_namespaces is preserved
    $res = run_loader(dump_directory => $DUMP_DIR);

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on re-dumping static schema';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on re-dumping static schema';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on re-dumping static schema'
        or diag @{ $res->{warnings} };

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Result::Quux',
        'use_namespaces preserved on re-dump';

    run_v5_tests($res);

    # now downgrade the schema to load_classes
    $res = run_loader(
        dump_directory => $DUMP_DIR,
        use_namespaces => 0,
    );
    $schema = $res->{schema};

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on downgrading to load_classes';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on downgrading to load_classes';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on downgrading to load_classes'
        or diag @{ $res->{warnings} };

    run_v5_tests($res);

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Quux',
        'load_classes downgrade correct';

    (my $result_dir = "$DUMP_DIR/$SCHEMA_CLASS") =~ s{::}{/}g;
    my $result_count =()= glob "$result_dir/*";

    is $result_count, 4,
'correct number of Results after upgrade and Result dir removed';

    ok ((not -d "$result_dir/Result"),
        'Result dir was removed for load_classes downgrade');

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'mtfnpy' }
        'custom content was carried over during load_classes downgrade';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel8,
        $res->{classes}{bazs} }
'namespaced class names in custom content are translated during load_classes '.
'downgrade';

    my $file = $schema->_loader->_get_dump_filename($res->{classes}{quuxs});
    my $code = slurp $file;

    like $code, qr/sub a_method { 'mtfnpy' }/,
'custom content from namespaced Result loaded into static dump correctly '.
'during load_classes downgrade';
}

# test a regular schema with use_namespaces => 1 and a custom result_namespace
# downgraded to load_classes
{
    rmtree $DUMP_DIR;
    mkdir $DUMP_DIR;

    my $res = run_loader(
        dump_directory => $DUMP_DIR,
        result_namespace => 'MyResult',
    );

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on dumping static schema';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on dumping static schema';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on dumping static schema'
        or diag @{ $res->{warnings} };

    run_v5_tests($res);

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::MyResult::Quux',
        'defaults to use_namespaces and uses custom result_namespace';

    # add some custom content to a Result that will be replaced
    my $schema   = $res->{schema};
    my $quuxs_pm = $schema->_loader
        ->_get_dump_filename($res->{classes}{quuxs});
    {
        local ($^I, @ARGV) = ('.bak', $quuxs_pm);
        while (<>) {
            if (/DO NOT MODIFY THIS OR ANYTHING ABOVE/) {
                print;
                print <<EOF;
sub a_method { 'mtfnpy' }

__PACKAGE__->has_one('bazrel9', 'DBIXCSL_Test::Schema::MyResult::Baz',
    { 'foreign.baz_num' => 'self.baz_id' });
EOF
            }
            else {
                print;
            }
        }
        close ARGV;
        unlink "${quuxs_pm}.bak" or die $^E;
    }

    # test that with no use_namespaces option, use_namespaces is preserved, and
    # the custom result_namespace is preserved
    $res = run_loader(dump_directory => $DUMP_DIR);

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on re-dumping static schema';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on re-dumping static schema';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on re-dumping static schema'
        or diag @{ $res->{warnings} };

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::MyResult::Quux',
        'use_namespaces and custom result_namespace preserved on re-dump';

    run_v5_tests($res);

    # now downgrade the schema to load_classes
    $res = run_loader(
        dump_directory => $DUMP_DIR,
        use_namespaces => 0,
    );
    $schema = $res->{schema};

    like $res->{warnings}[0], qr/Dumping manual schema/i,
'correct warnings on downgrading to load_classes';

    like $res->{warnings}[1], qr/dump completed/i,
'correct warnings on downgrading to load_classes';

    is scalar @{ $res->{warnings} }, 2,
'correct number of warnings on downgrading to load_classes'
        or diag @{ $res->{warnings} };

    run_v5_tests($res);

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Quux',
        'load_classes downgrade correct';

    (my $result_dir = "$DUMP_DIR/$SCHEMA_CLASS") =~ s{::}{/}g;
    my $result_count =()= glob "$result_dir/*";

    is $result_count, 4,
'correct number of Results after upgrade and Result dir removed';

    ok ((not -d "$result_dir/MyResult"),
        'Result dir was removed for load_classes downgrade');

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'mtfnpy' }
        'custom content was carried over during load_classes downgrade';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel9,
        $res->{classes}{bazs} }
'namespaced class names in custom content are translated during load_classes '.
'downgrade';

    my $file = $schema->_loader->_get_dump_filename($res->{classes}{quuxs});
    my $code = slurp $file;

    like $code, qr/sub a_method { 'mtfnpy' }/,
'custom content from namespaced Result loaded into static dump correctly '.
'during load_classes downgrade';
}

# rewrite from one result_namespace to another, with external content
{
    rmtree $DUMP_DIR;
    mkdir $DUMP_DIR;
    my $temp_dir = tempdir(CLEANUP => 1);
    push @INC, $temp_dir;

    my $external_result_dir = join '/', $temp_dir, split /::/,
        "${SCHEMA_CLASS}::Result";

    make_path $external_result_dir;

    IO::File->new(">$external_result_dir/Quux.pm")->print(<<"EOF");
package ${SCHEMA_CLASS}::Result::Quux;
sub c_method { 'dongs' }

__PACKAGE__->has_one('bazrel12', 'DBIXCSL_Test::Schema::Result::Baz',
    { 'foreign.baz_num' => 'self.baz_id' });

1;
EOF

    IO::File->new(">$external_result_dir/Bar.pm")->print(<<"EOF");
package ${SCHEMA_CLASS}::Result::Bar;

__PACKAGE__->has_one('foorel6', 'DBIXCSL_Test::Schema::Result::Foo',
    { 'foreign.fooid' => 'self.foo_id' });

1;
EOF

    my $res = run_loader(dump_directory => $DUMP_DIR);

    # add some custom content to a Result that will be replaced
    my $schema   = $res->{schema};
    my $quuxs_pm = $schema->_loader
        ->_get_dump_filename($res->{classes}{quuxs});
    {
        local ($^I, @ARGV) = ('.bak', $quuxs_pm);
        while (<>) {
            if (/DO NOT MODIFY THIS OR ANYTHING ABOVE/) {
                print;
                print <<EOF;
sub a_method { 'mtfnpy' }

__PACKAGE__->has_one('bazrel10', 'DBIXCSL_Test::Schema::Result::Baz',
    { 'foreign.baz_num' => 'self.baz_id' });
EOF
            }
            else {
                print;
            }
        }
        close ARGV;
        unlink "${quuxs_pm}.bak" or die $^E;
    }

    # Rewrite implicit 'Result' to 'MyResult'
    $res = run_loader(
        dump_directory => $DUMP_DIR,
        result_namespace => 'MyResult',
    );
    $schema = $res->{schema};

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::MyResult::Quux',
        'using new result_namespace';

    (my $schema_dir = "$DUMP_DIR/$SCHEMA_CLASS")          =~ s{::}{/}g;
    (my $result_dir = "$DUMP_DIR/$SCHEMA_CLASS/MyResult") =~ s{::}{/}g;
    my $result_count =()= glob "$result_dir/*";

    is $result_count, 4,
'correct number of Results after rewritten result_namespace';

    ok ((not -d "$schema_dir/Result"),
        'original Result dir was removed when rewriting result_namespace');

    # check that custom content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'mtfnpy' }
        'custom content was carried over when rewriting result_namespace';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel10,
        $res->{classes}{bazs} }
'class names in custom content are translated when rewriting result_namespace';

    my $file = $schema->_loader->_get_dump_filename($res->{classes}{quuxs});
    my $code = slurp $file;

    like $code, qr/sub a_method { 'mtfnpy' }/,
'custom content from namespaced Result loaded into static dump correctly '.
'when rewriting result_namespace';

    # Now rewrite 'MyResult' to 'Mtfnpy'
    $res = run_loader(
        dump_directory => $DUMP_DIR,
        result_namespace => 'Mtfnpy',
    );
    $schema = $res->{schema};

    is $res->{classes}{quuxs}, 'DBIXCSL_Test::Schema::Mtfnpy::Quux',
        'using new result_namespace';

    ($schema_dir = "$DUMP_DIR/$SCHEMA_CLASS")        =~ s{::}{/}g;
    ($result_dir = "$DUMP_DIR/$SCHEMA_CLASS/Mtfnpy") =~ s{::}{/}g;
    $result_count =()= glob "$result_dir/*";

    is $result_count, 4,
'correct number of Results after rewritten result_namespace';

    ok ((not -d "$schema_dir/MyResult"),
        'original Result dir was removed when rewriting result_namespace');

    # check that custom and external content was preserved
    lives_and { is $schema->resultset('Quux')->find(1)->a_method, 'mtfnpy' }
        'custom content was carried over when rewriting result_namespace';

    lives_and { is $schema->resultset('Quux')->find(1)->c_method, 'dongs' }
        'custom content was carried over when rewriting result_namespace';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel10,
        $res->{classes}{bazs} }
'class names in custom content are translated when rewriting result_namespace';

    lives_and { isa_ok $schema->resultset('Quux')->find(1)->bazrel12,
        $res->{classes}{bazs} }
'class names in external content are translated when rewriting '.
'result_namespace';

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel6,
        $res->{classes}{foos} }
'class names in external content are translated when rewriting '.
'result_namespace';

    $file = $schema->_loader->_get_dump_filename($res->{classes}{quuxs});
    $code = slurp $file;

    like $code, qr/sub a_method { 'mtfnpy' }/,
'custom content from namespaced Result loaded into static dump correctly '.
'when rewriting result_namespace';

    like $code, qr/sub c_method { 'dongs' }/,
'external content from unsingularized Result loaded into static dump correctly';

    pop @INC;
}

# test upgrading a v4 schema, the check that the version string is correct
{
    write_v4_schema_pm();
    run_loader(dump_directory => $DUMP_DIR);
    my $res = run_loader(dump_directory => $DUMP_DIR, naming => 'current');
    my $schema = $res->{schema};

    my $file = $schema->_loader->_get_dump_filename($SCHEMA_CLASS);
    my $code = slurp $file;

    my ($dumped_ver) =
        $code =~ /^# Created by DBIx::Class::Schema::Loader v(\S+)/m;

    is $dumped_ver, $DBIx::Class::Schema::Loader::VERSION,
        'correct version dumped after upgrade of v4 static schema';
}

# Test upgrading an already singular result with custom content that refers to
# old class names.
{
    write_v4_schema_pm();
    my $res = run_loader(dump_directory => $DUMP_DIR);
    my $schema   = $res->{schema};
    run_v4_tests($res);

    # add some custom content to a Result that will be replaced
    my $bar_pm = $schema->_loader
        ->_get_dump_filename($res->{classes}{bar});
    {
        local ($^I, @ARGV) = ('.bak', $bar_pm);
        while (<>) {
            if (/DO NOT MODIFY THIS OR ANYTHING ABOVE/) {
                print;
                print <<EOF;
sub a_method { 'lalala' }

__PACKAGE__->has_one('foorel3', 'DBIXCSL_Test::Schema::Foos',
    { 'foreign.fooid' => 'self.foo_id' });
EOF
            }
            else {
                print;
            }
        }
        close ARGV;
        unlink "${bar_pm}.bak" or die $^E;
    }

    # now upgrade the schema
    $res = run_loader(dump_directory => $DUMP_DIR, naming => 'current');
    $schema = $res->{schema};
    run_v5_tests($res);

    # check that custom content was preserved
    lives_and { is $schema->resultset('Bar')->find(1)->a_method, 'lalala' }
        'custom content was preserved from Result pre-upgrade';

    lives_and { isa_ok $schema->resultset('Bar')->find(1)->foorel3,
        $res->{classes}{foos} }
'unsingularized class names in custom content from Result with unchanged ' .
'name are translated';

    my $file = $schema->_loader->_get_dump_filename($res->{classes}{bar});
    my $code = slurp $file;

    like $code, qr/sub a_method { 'lalala' }/,
'custom content from Result with unchanged name loaded into static dump ' .
'correctly';
}

done_testing;

END {
    rmtree $DUMP_DIR unless $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP};
}

sub run_loader {
    my %loader_opts = @_;

    eval {
        foreach my $source_name ($SCHEMA_CLASS->clone->sources) {
            Class::Unload->unload("${SCHEMA_CLASS}::${source_name}");
        }

        Class::Unload->unload($SCHEMA_CLASS);
    };
    undef $@;

    my @connect_info = $make_dbictest_db2::dsn;
    my @loader_warnings;
    local $SIG{__WARN__} = sub { push(@loader_warnings, $_[0]); };
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

sub run_v4_tests {
    my $res = shift;
    my $schema = $res->{schema};

    is_deeply [ @{ $res->{monikers} }{qw/foos bar bazs quuxs/} ],
        [qw/Foos Bar Bazs Quuxs/],
        'correct monikers in 0.04006 mode';

    isa_ok ((my $bar = eval { $schema->resultset('Bar')->find(1) }),
        $res->{classes}{bar},
        'found a bar');

    isa_ok eval { $bar->foo_id }, $res->{classes}{foos},
        'correct rel name in 0.04006 mode';

    ok my $baz  = eval { $schema->resultset('Bazs')->find(1) };

    isa_ok eval { $baz->quux }, 'DBIx::Class::ResultSet',
        'correct rel type and name for UNIQUE FK in 0.04006 mode';
}

sub run_v5_tests {
    my $res = shift;
    my $schema = $res->{schema};

    is_deeply [ @{ $res->{monikers} }{qw/foos bar bazs quuxs/} ],
        [qw/Foo Bar Baz Quux/],
        'correct monikers in current mode';

    ok my $bar = eval { $schema->resultset('Bar')->find(1) };

    isa_ok eval { $bar->foo }, $res->{classes}{foos},
        'correct rel name in current mode';

    ok my $baz  = eval { $schema->resultset('Baz')->find(1) };

    isa_ok eval { $baz->quux }, $res->{classes}{quuxs},
        'correct rel type and name for UNIQUE FK in current mode';
}
