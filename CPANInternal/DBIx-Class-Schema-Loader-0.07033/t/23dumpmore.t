use strict;
use warnings;
use Test::More;
use DBIx::Class::Schema::Loader::Utils qw/slurp_file write_file/;
use namespace::clean;
use File::Temp ();
use lib qw(t/lib);
use dbixcsl_dumper_tests;
my $t = 'dbixcsl_dumper_tests';

$t->cleanup;

# test loading external content
$t->dump_test(
  classname => 'DBICTest::Schema::_no_skip_load_external',
  regexes => {
    Foo => [
      qr/package DBICTest::Schema::_no_skip_load_external::Foo;\nour \$skip_me = "bad mojo";\n1;/
    ],
  },
);

# test skipping external content
$t->dump_test(
  classname => 'DBICTest::Schema::_skip_load_external',
  options => {
    skip_load_external => 1,
  },
  neg_regexes => {
    Foo => [
      qr/package DBICTest::Schema::_skip_load_external::Foo;\nour \$skip_me = "bad mojo";\n1;/
    ],
  },
);

$t->cleanup;
# test config_file
{
  my $config_file = File::Temp->new (UNLINK => 1);

  print $config_file "{ skip_relationships => 1 }\n";
  close $config_file;

  $t->dump_test(
    classname => 'DBICTest::Schema::_config_file',
    options => { config_file => "$config_file" },
    neg_regexes => {
      Foo => [
        qr/has_many/,
      ],
    },
  );
}

# proper exception
$t->dump_test(
  classname => 'DBICTest::Schema::_clashing_monikers',
  test_db_class => 'make_dbictest_db_clashing_monikers',
  error => qr/tables (?:"bar", "bars"|"bars", "bar") reduced to the same source moniker 'Bar'/,
);


$t->cleanup;

# test naming => { column_accessors => 'preserve' }
# also test POD for unique constraint
$t->dump_test(
    classname => 'DBICTest::Schema::_preserve_column_accessors',
    test_db_class => 'make_dbictest_db_with_unique',
    options => { naming => { column_accessors => 'preserve' } },
    neg_regexes => {
        RouteChange => [
            qr/\baccessor\b/,
        ],
    },
    regexes => {
        Baz => [
            qr/\n\n=head1 UNIQUE CONSTRAINTS\n\n=head2 C<baz_num_unique>\n\n=over 4\n\n=item \* L<\/baz_num>\n\n=back\n\n=cut\n\n__PACKAGE__->add_unique_constraint\("baz_num_unique"\, \["baz_num"\]\);\n\n/,
        ],
    }
);

$t->cleanup;

# test that rels are sorted
$t->dump_test(
    classname => 'DBICTest::Schema::_sorted_rels',
    test_db_class => 'make_dbictest_db_with_unique',
    regexes => {
        Baz => [
            qr/->might_have\(\n  "quux".*->belongs_to\(\n  "station_visited"/s,
        ],
    }
);

$t->cleanup;

$t->dump_test(
    classname => 'DBICTest::Schema::_sorted_uniqs',
    test_db_class => 'make_dbictest_db_multi_unique',
    regexes => {
        Bar => [
            qr/->add_unique_constraint\("uniq1_unique".*->add_unique_constraint\("uniq2_unique"/s,
        ],
    },
);

$t->cleanup;

# test naming => { monikers => 'plural' }
$t->dump_test(
    classname => 'DBICTest::Schema::_plural_monikers',
    options => { naming => { monikers => 'plural' } },
    regexes => {
        Foos => [
            qr/\n=head1 NAME\n\nDBICTest::Schema::_plural_monikers::Foos\n\n=cut\n\n/,
        ],
        Bars => [
            qr/\n=head1 NAME\n\nDBICTest::Schema::_plural_monikers::Bars\n\n=cut\n\n/,
        ],
    },
);

$t->cleanup;

# test naming => { monikers => 'singular' }
$t->dump_test(
    classname => 'DBICTest::Schema::_singular_monikers',
    test_db_class => 'make_dbictest_db_plural_tables',
    options => { naming => { monikers => 'singular' } },
    regexes => {
        Foo => [
            qr/\n=head1 NAME\n\nDBICTest::Schema::_singular_monikers::Foo\n\n=cut\n\n/,
        ],
        Bar => [
            qr/\n=head1 NAME\n\nDBICTest::Schema::_singular_monikers::Bar\n\n=cut\n\n/,
        ],
    },
);

$t->cleanup;

# test naming => { monikers => 'preserve' }
$t->dump_test(
    classname => 'DBICTest::Schema::_preserve_monikers',
    test_db_class => 'make_dbictest_db_plural_tables',
    options => { naming => { monikers => 'preserve' } },
    regexes => {
        Foos => [
            qr/\n=head1 NAME\n\nDBICTest::Schema::_preserve_monikers::Foos\n\n=cut\n\n/,
        ],
        Bars => [
            qr/\n=head1 NAME\n\nDBICTest::Schema::_preserve_monikers::Bars\n\n=cut\n\n/,
        ],
    },
);

$t->cleanup;

# test out the POD and "use utf8;"
$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    custom_column_info => sub {
      my ($table, $col, $info) = @_;
      return +{ extra => { is_footext => 1 } } if $col eq 'footext';
    },
    result_base_class => 'My::ResultBaseClass',
    additional_classes => 'TestAdditional',
    additional_base_classes => 'TestAdditionalBase',
    left_base_classes => 'TestLeftBase',
    components => [ 'TestComponent', '+TestComponentFQN' ],
  },
  regexes => {
    schema => [
      qr/^use utf8;\n/,
      qr/package DBICTest::DumpMore::1;/,
      qr/->load_classes/,
    ],
    Foo => [
      qr/^use utf8;\n/,
      qr/package DBICTest::DumpMore::1::Foo;/,
      qr/\n=head1 NAME\n\nDBICTest::DumpMore::1::Foo\n\n=cut\n\nuse strict;\nuse warnings;\n\n/,
      qr/\n=head1 BASE CLASS: L<My::ResultBaseClass>\n\n=cut\n\nuse base 'My::ResultBaseClass';\n\n/,
      qr/\n=head1 ADDITIONAL CLASSES USED\n\n=over 4\n\n=item \* L<TestAdditional>\n\n=back\n\n=cut\n\n/,
      qr/\n=head1 ADDITIONAL BASE CLASSES\n\n=over 4\n\n=item \* L<TestAdditionalBase>\n\n=back\n\n=cut\n\n/,
      qr/\n=head1 LEFT BASE CLASSES\n\n=over 4\n\n=item \* L<TestLeftBase>\n\n=back\n\n=cut\n\n/,
      qr/\n=head1 COMPONENTS LOADED\n\n=over 4\n\n=item \* L<DBIx::Class::TestComponent>\n\n=item \* L<TestComponentFQN>\n\n=back\n\n=cut\n\n/,
      qr/\n=head1 TABLE: C<foo>\n\n=cut\n\n__PACKAGE__->table\("foo"\);\n\n/,
      qr/\n=head1 ACCESSORS\n\n/,
      qr/\n=head2 fooid\n\n  data_type: 'integer'\n  is_auto_increment: 1\n  is_nullable: 0\n\n/,
      qr/\n=head2 footext\n\n  data_type: 'text'\n  default_value: 'footext'\n  extra: {is_footext => 1}\n  is_nullable: 1\n\n/,
      qr/\n=head1 PRIMARY KEY\n\n=over 4\n\n=item \* L<\/fooid>\n\n=back\n\n=cut\n\n__PACKAGE__->set_primary_key\("fooid"\);\n/,
      qr/\n=head1 RELATIONS\n\n/,
      qr/\n=head2 bars\n\nType: has_many\n\nRelated object: L<DBICTest::DumpMore::1::Bar>\n\n=cut\n\n/,
      qr/1;\n$/,
    ],
    Bar => [
      qr/^use utf8;\n/,
      qr/package DBICTest::DumpMore::1::Bar;/,
      qr/\n=head1 NAME\n\nDBICTest::DumpMore::1::Bar\n\n=cut\n\nuse strict;\nuse warnings;\n\n/,
      qr/\n=head1 BASE CLASS: L<My::ResultBaseClass>\n\n=cut\n\nuse base 'My::ResultBaseClass';\n\n/,
      qr/\n=head1 ADDITIONAL CLASSES USED\n\n=over 4\n\n=item \* L<TestAdditional>\n\n=back\n\n=cut\n\n/,
      qr/\n=head1 ADDITIONAL BASE CLASSES\n\n=over 4\n\n=item \* L<TestAdditionalBase>\n\n=back\n\n=cut\n\n/,
      qr/\n=head1 LEFT BASE CLASSES\n\n=over 4\n\n=item \* L<TestLeftBase>\n\n=back\n\n=cut\n\n/,
      qr/\n=head1 COMPONENTS LOADED\n\n=over 4\n\n=item \* L<DBIx::Class::TestComponent>\n\n=item \* L<TestComponentFQN>\n\n=back\n\n=cut\n\n/,
      qr/\n=head1 TABLE: C<bar>\n\n=cut\n\n__PACKAGE__->table\("bar"\);\n\n/,
      qr/\n=head1 ACCESSORS\n\n/,
      qr/\n=head2 barid\n\n  data_type: 'integer'\n  is_auto_increment: 1\n  is_nullable: 0\n\n/,
      qr/\n=head2 fooref\n\n  data_type: 'integer'\n  is_foreign_key: 1\n  is_nullable: 1\n\n/,
      qr/\n=head1 PRIMARY KEY\n\n=over 4\n\n=item \* L<\/barid>\n\n=back\n\n=cut\n\n__PACKAGE__->set_primary_key\("barid"\);\n/,
      qr/\n=head1 RELATIONS\n\n/,
      qr/\n=head2 fooref\n\nType: belongs_to\n\nRelated object: L<DBICTest::DumpMore::1::Foo>\n\n=cut\n\n/,
      qr/\n1;\n$/,
    ],
  },
);

$t->append_to_class('DBICTest::DumpMore::1::Foo',q{# XXX This is my custom content XXX});


$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  regexes => {
    schema => [
      qr/package DBICTest::DumpMore::1;/,
      qr/->load_classes/,
    ],
    Foo => [
      qr/package DBICTest::DumpMore::1::Foo;/,
      qr/->set_primary_key/,
      qr/1;\n# XXX This is my custom content XXX/,
    ],
    Bar => [
      qr/package DBICTest::DumpMore::1::Bar;/,
      qr/->set_primary_key/,
      qr/1;\n$/,
    ],
  },
);


$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    really_erase_my_files => 1 
  },
  regexes => {
    schema => [
      qr/package DBICTest::DumpMore::1;/,
      qr/->load_classes/,
    ],
    Foo => [
      qr/package DBICTest::DumpMore::1::Foo;/,
      qr/->set_primary_key/,
      qr/1;\n$/,
    ],
    Bar => [
      qr/package DBICTest::DumpMore::1::Bar;/,
      qr/->set_primary_key/,
      qr/1;\n$/,
    ],
  },
  neg_regexes => {
    Foo => [
      qr/# XXX This is my custom content XXX/,
    ],
  },
);


$t->cleanup;

# test namespaces
$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    use_namespaces => 1,
    generate_pod => 0
  },
  neg_regexes => {
    'Result/Foo' => [
      qr/^=/m,
    ],
  },
);


$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    db_schema => 'foo_schema',
    qualify_objects => 1,
    use_namespaces => 1
  },
  warnings => [
    qr/^db_schema is not supported on SQLite/,
  ],
  regexes => {
    'Result/Foo' => [
      qr/^\Q__PACKAGE__->table("foo_schema.foo");\E/m,
      # the has_many relname should not have the schema in it!
      qr/^__PACKAGE__->has_many\(\n  "bars"/m,
    ],
  },
);

# test qualify_objects
$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    db_schema => [ 'foo_schema', 'bar_schema' ],
    qualify_objects => 0,
    use_namespaces => 1,
  },
  warnings => [
    qr/^db_schema is not supported on SQLite/,
  ],
  regexes => {
    'Result/Foo' => [
      # the table name should not include the db schema
      qr/^\Q__PACKAGE__->table("foo");\E/m,
    ],
    'Result/Bar' => [
      # the table name should not include the db schema
      qr/^\Q__PACKAGE__->table("bar");\E/m,
    ],
  },
);

# test moniker_parts
$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    db_schema => 'my_schema',
    moniker_parts => ['_schema', 'name'],
    qualify_objects => 1,
    use_namespaces => 1,
  },
  warnings => [
    qr/^db_schema is not supported on SQLite/,
  ],
  regexes => {
    'Result/MySchemaFoo' => [
      qr/^\Q__PACKAGE__->table("my_schema.foo");\E/m,
      # the has_many relname should not have the schema in it!
      qr/^__PACKAGE__->has_many\(\n  "bars"/m,
    ],
  },
);

$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    use_namespaces => 1
  },
  regexes => {
    schema => [
      qr/package DBICTest::DumpMore::1;/,
      qr/->load_namespaces/,
    ],
    'Result/Foo' => [
      qr/package DBICTest::DumpMore::1::Result::Foo;/,
      qr/->set_primary_key/,
      qr/1;\n$/,
    ],
    'Result/Bar' => [
      qr/package DBICTest::DumpMore::1::Result::Bar;/,
      qr/->set_primary_key/,
      qr/1;\n$/,
    ],
  },
);


$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    use_namespaces => 1,
    result_namespace => 'Res',
    resultset_namespace => 'RSet',
    default_resultset_class => 'RSetBase',
  },
  regexes => {
    schema => [
      qr/package DBICTest::DumpMore::1;/,
      qr/->load_namespaces/,
      qr/result_namespace => "Res"/,
      qr/resultset_namespace => "RSet"/,
      qr/default_resultset_class => "RSetBase"/,
    ],
    'Res/Foo' => [
      qr/package DBICTest::DumpMore::1::Res::Foo;/,
      qr/->set_primary_key/,
      qr/1;\n$/,
    ],
    'Res/Bar' => [
      qr/package DBICTest::DumpMore::1::Res::Bar;/,
      qr/->set_primary_key/,
      qr/1;\n$/,
    ],
  },
);


$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    use_namespaces => 1,
    result_namespace => '+DBICTest::DumpMore::1::Res',
    resultset_namespace => 'RSet',
    default_resultset_class => 'RSetBase',
    result_base_class => 'My::ResultBaseClass',
    schema_base_class => 'My::SchemaBaseClass',
  },
  regexes => {
    schema => [
      qr/package DBICTest::DumpMore::1;/,
      qr/->load_namespaces/,
      qr/result_namespace => "\+DBICTest::DumpMore::1::Res"/,
      qr/resultset_namespace => "RSet"/,
      qr/default_resultset_class => "RSetBase"/,
      qr/use base 'My::SchemaBaseClass'/,
    ],
    'Res/Foo' => [
      qr/package DBICTest::DumpMore::1::Res::Foo;/,
      qr/use base 'My::ResultBaseClass'/,
      qr/->set_primary_key/,
      qr/1;\n$/,
    ],
    'Res/Bar' => [
      qr/package DBICTest::DumpMore::1::Res::Bar;/,
      qr/use base 'My::ResultBaseClass'/,
      qr/->set_primary_key/,
      qr/1;\n$/,
    ],
  },
);

$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    use_namespaces    => 1,
    result_base_class => 'My::MissingResultBaseClass',
  },
  error => qr/My::MissingResultBaseClass.*is not installed/,
);

# test quote_char in connect_info for dbicdump
$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  extra_connect_info => [
    '',
    '',
    { quote_char => '"' },
  ],
);

# test fix for RT#70507 (end comment and 1; gets lost if left with actual
# custom content)

$t->dump_test(
    classname => 'DBICTest::DumpMore::Upgrade',
    options => {
        use_namespaces => 0,
    },
);

my $file = $t->class_file('DBICTest::DumpMore::Upgrade::Foo');

my $code = slurp_file $file;

$code =~ s/(?=# You can replace)/sub custom_method { 'custom_method works' }\n0;\n\n/;

write_file $file, $code;

$t->dump_test(
    classname => 'DBICTest::DumpMore::Upgrade',
    options => {
        use_namespaces => 1,
    },
    regexes => {
        'Result/Foo' => [
            qr/sub custom_method { 'custom_method works' }\n0;\n\n# You can replace.*\n1;\n\z/,
        ],
    },
);

done_testing;
# vim:et sts=4 sw=4 tw=0:
