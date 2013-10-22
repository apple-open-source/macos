use strict;
use Test::More;
use Test::Warn;
use DBIx::Class::Schema::Loader::Optional::Dependencies;
use lib qw(t/lib);
use make_dbictest_db;

# Takes a $schema as input, runs 4 basic tests
sub test_schema {
  my ($testname, $schema) = @_;

  warnings_are ( sub {
    $schema = $schema->clone if !ref $schema;
    isa_ok($schema, 'DBIx::Class::Schema', $testname);

    my $rel_foo_rs = $schema->resultset('Bar')->search({ barid => 3})->search_related('fooref');
    isa_ok($rel_foo_rs, 'DBIx::Class::ResultSet', $testname);

    my $rel_foo = $rel_foo_rs->next;
    isa_ok($rel_foo, "DBICTest::Schema::_${testname}::Foo", $testname);

    is($rel_foo->footext, 'Foo record associated with the Bar with barid 3', "$testname correct object");

    my $foo_rs = $schema->resultset('Foo');
    my $foo_new = $foo_rs->create({footext => "${testname}_foo"});
    is ($foo_rs->search({footext => "${testname}_foo"})->count, 1, "$testname object created") || die;
  }, [], "No warnings during $testname invocations");
}

my @invocations = (
    'hardcode' => sub {
        package DBICTest::Schema::_hardcode;
        use base qw/ DBIx::Class::Schema::Loader /;
        __PACKAGE__->naming('current');
        __PACKAGE__->use_namespaces(0);
        __PACKAGE__->connection($make_dbictest_db::dsn);
        __PACKAGE__;
    },
    'normal' => sub {
        package DBICTest::Schema::_normal;
        use base qw/ DBIx::Class::Schema::Loader /;
        __PACKAGE__->loader_options();
        __PACKAGE__->naming('current');
        __PACKAGE__->use_namespaces(0);
        __PACKAGE__->connect($make_dbictest_db::dsn);
    },
    'make_schema_at' => sub {
        use DBIx::Class::Schema::Loader qw/ make_schema_at /;
        make_schema_at(
            'DBICTest::Schema::_make_schema_at',
            {
                really_erase_my_files => 1,
                naming => 'current',
                use_namespaces => 0
            },
            [ $make_dbictest_db::dsn ],
        );
        DBICTest::Schema::_make_schema_at->clone;
    },
    'embedded_options' => sub {
        package DBICTest::Schema::_embedded_options;
        use base qw/ DBIx::Class::Schema::Loader /;
        __PACKAGE__->naming('current');
        __PACKAGE__->use_namespaces(0);
        __PACKAGE__->connect(
            $make_dbictest_db::dsn,
            { loader_options => { really_erase_my_files => 1 } }
        );
    },
    'embedded_options_in_attrs' => sub {
        package DBICTest::Schema::_embedded_options_in_attrs;
        use base qw/ DBIx::Class::Schema::Loader /;
        __PACKAGE__->naming('current');
        __PACKAGE__->use_namespaces(0);
        __PACKAGE__->connect(
            $make_dbictest_db::dsn,
            undef,
            undef,
            { AutoCommit => 1, loader_options => { really_erase_my_files => 1 } }
        );
    },
    'embedded_options_make_schema_at' => sub {
        use DBIx::Class::Schema::Loader qw/ make_schema_at /;
        make_schema_at(
            'DBICTest::Schema::_embedded_options_make_schema_at',
            { },
            [
                $make_dbictest_db::dsn,
                { loader_options => {
                    really_erase_my_files => 1,
                    naming => 'current',
                    use_namespaces => 0,
                } },
            ],
        );
        "DBICTest::Schema::_embedded_options_make_schema_at";
    },
    'almost_embedded' => sub {
        package DBICTest::Schema::_almost_embedded;
        use base qw/ DBIx::Class::Schema::Loader /;
        __PACKAGE__->loader_options(
            really_erase_my_files => 1,
            naming => 'current',
            use_namespaces => 0,
        );
        __PACKAGE__->connect(
            $make_dbictest_db::dsn,
            undef, undef, { AutoCommit => 1 }
        );
    },
    'make_schema_at_explicit' => sub {
        use DBIx::Class::Schema::Loader;
        DBIx::Class::Schema::Loader::make_schema_at(
            'DBICTest::Schema::_make_schema_at_explicit',
            {
                really_erase_my_files => 1,
                naming => 'current',
                use_namespaces => 0,
            },
            [ $make_dbictest_db::dsn ],
        );
        DBICTest::Schema::_make_schema_at_explicit->clone;
    },
    'no_skip_load_external' => sub {
        # By default we should pull in t/lib/DBICTest/Schema/_no_skip_load_external/Foo.pm $skip_me since t/lib is in @INC
        use DBIx::Class::Schema::Loader;
        DBIx::Class::Schema::Loader::make_schema_at(
            'DBICTest::Schema::_no_skip_load_external',
            {
                really_erase_my_files => 1,
                naming => 'current',
                use_namespaces => 0,
            },
            [ $make_dbictest_db::dsn ],
        );
        DBICTest::Schema::_no_skip_load_external->clone;
    },
    'skip_load_external' => sub {
        # When we explicitly skip_load_external t/lib/DBICTest/Schema/_skip_load_external/Foo.pm should be ignored
        use DBIx::Class::Schema::Loader;
        DBIx::Class::Schema::Loader::make_schema_at(
            'DBICTest::Schema::_skip_load_external',
            {
                really_erase_my_files => 1,
                naming => 'current',
                use_namespaces => 0,
                skip_load_external => 1,
            },
            [ $make_dbictest_db::dsn ],
        );
        DBICTest::Schema::_skip_load_external->clone;
    },
    (DBIx::Class::Schema::Loader::Optional::Dependencies->req_ok_for('use_moose') ?
        ('use_moose' => sub {
            package DBICTest::Schema::_use_moose;
            use base qw/ DBIx::Class::Schema::Loader /;
            __PACKAGE__->naming('current');
            __PACKAGE__->use_namespaces(0);
            __PACKAGE__->connect(
                $make_dbictest_db::dsn,
                { loader_options => { use_moose =>  1 } }
            );
        })
        : ()
    ),
);

# 6 tests per k/v pair
plan tests => 6 * (@invocations/2) + 2;  # + 2 more manual ones below.

while(@invocations) {
    my $style = shift @invocations;
    my $cref = shift @invocations;

    my $schema = do {
      local $SIG{__WARN__} = sub {
        warn $_[0] unless $_[0] =~ /Deleting existing file .+ due to 'really_erase_my_files' setting/
      };
      $cref->();
    };

    test_schema($style, $schema);
}

{
    no warnings 'once';

    is($DBICTest::Schema::_no_skip_load_external::Foo::skip_me, "bad mojo",
        "external content loaded");
    is($DBICTest::Schema::_skip_load_external::Foo::skip_me, undef,
        "external content not loaded with skip_load_external => 1");
}
