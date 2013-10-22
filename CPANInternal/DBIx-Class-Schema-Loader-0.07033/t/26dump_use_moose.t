use warnings;
use strict;

use Test::More;
use DBIx::Class::Schema::Loader::Optional::Dependencies ();
BEGIN {
  use DBIx::Class::Schema::Loader::Optional::Dependencies ();
  plan skip_all => 'Tests needs ' . DBIx::Class::Schema::Loader::Optional::Dependencies->req_missing_for('use_moose')
    unless (DBIx::Class::Schema::Loader::Optional::Dependencies->req_ok_for('use_moose'));
}

use lib qw(t/lib);
use dbixcsl_dumper_tests;
my $t = 'dbixcsl_dumper_tests';

$t->cleanup;

# first dump a fresh use_moose=1 schema
$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    use_moose => 1,
    result_base_class => 'My::ResultBaseClass',
    schema_base_class => 'My::SchemaBaseClass',
    result_roles => ['TestRole', 'TestRole2'],
  },
  regexes => {
    schema => [
      qr/\nuse Moose;\nuse MooseX::MarkAsMethods autoclean => 1;\nextends 'My::SchemaBaseClass';\n\n/,
      qr/\n__PACKAGE__->meta->make_immutable\(inline_constructor => 0\);\n1;(?!\n1;\n)\n.*/,
    ],
    Foo => [
      qr/\nuse Moose;\nuse MooseX::NonMoose;\nuse MooseX::MarkAsMethods autoclean => 1;\nextends 'My::ResultBaseClass';\n\n/,
      qr/=head1 L<Moose> ROLES APPLIED\n\n=over 4\n\n=item \* L<TestRole>\n\n=item \* L<TestRole2>\n\n=back\n\n=cut\n\n/,
      qr/\nwith 'TestRole', 'TestRole2';\n\n/,
      qr/\n__PACKAGE__->meta->make_immutable;\n1;(?!\n1;\n)\n.*/,
    ],
    Bar => [
      qr/\nuse Moose;\nuse MooseX::NonMoose;\nuse MooseX::MarkAsMethods autoclean => 1;\nextends 'My::ResultBaseClass';\n\n/,
      qr/=head1 L<Moose> ROLES APPLIED\n\n=over 4\n\n=item \* L<TestRole>\n\n=item \* L<TestRole2>\n\n=back\n\n=cut\n\n/,
      qr/\nwith 'TestRole', 'TestRole2';\n\n/,
      qr/\n__PACKAGE__->meta->make_immutable;\n1;(?!\n1;\n)\n.*/,
    ],
  },
);

$t->cleanup;

# check protect_overloads works as expected
$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    use_moose      => 1,
    only_autoclean => 1,
  },
  regexes => {
    schema => [
      qr/\nuse namespace::autoclean;\n/,
    ],
    Foo => [
      qr/\nuse namespace::autoclean;\n/,
    ],
  },
);

$t->cleanup;

# now upgrade a fresh non-moose schema to use_moose=1
$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    use_moose => 0,
    result_base_class => 'My::ResultBaseClass',
    schema_base_class => 'My::SchemaBaseClass',
  },
  regexes => {
    schema => [
      qr/\nuse base 'My::SchemaBaseClass';\n/,
    ],
    Foo => [
      qr/\nuse base 'My::ResultBaseClass';\n/,
    ],
    Bar => [
      qr/\nuse base 'My::ResultBaseClass';\n/,
    ],
  },
);

# check that changed custom content is upgraded for Moose bits
$t->append_to_class('DBICTest::DumpMore::1::Foo', q{# XXX This is my custom content XXX});

$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    use_moose => 1,
    result_base_class => 'My::ResultBaseClass',
    schema_base_class => 'My::SchemaBaseClass',
  },
  regexes => {
    schema => [
      qr/\nuse Moose;\nuse MooseX::MarkAsMethods autoclean => 1;\nextends 'My::SchemaBaseClass';\n\n/,
      qr/\n__PACKAGE__->meta->make_immutable\(inline_constructor => 0\);\n1;(?!\n1;\n)\n.*/,
    ],
    Foo => [
      qr/\nuse Moose;\nuse MooseX::NonMoose;\nuse MooseX::MarkAsMethods autoclean => 1;\nextends 'My::ResultBaseClass';\n\n/,
      qr/\n__PACKAGE__->meta->make_immutable;\n1;(?!\n1;\n)\n.*/,
      qr/# XXX This is my custom content XXX/,
    ],
    Bar => [
      qr/\nuse Moose;\nuse MooseX::NonMoose;\nuse MooseX::MarkAsMethods autoclean => 1;\nextends 'My::ResultBaseClass';\n\n/,
      qr/\n__PACKAGE__->meta->make_immutable;\n1;(?!\n1;\n)\n.*/,
    ],
  },
);

$t->cleanup;

# check with a fresh non-moose schema that Moose custom content added to a use_moose=0 schema is not repeated
$t->dump_test(
  classname => 'DBICTest::DumpMore::1',
  options => {
    result_base_class => 'My::ResultBaseClass',
    schema_base_class => 'My::SchemaBaseClass',
  },
  regexes => {
    schema => [
      qr/\nuse base 'My::SchemaBaseClass';\n/,
    ],
    Foo => [
      qr/\nuse base 'My::ResultBaseClass';\n/,
    ],
    Bar => [
      qr/\nuse base 'My::ResultBaseClass';\n/,
    ],
  },
);

# add Moose custom content then check it is not repeated
# after that regen again *without* the use_moose flag, make
# sure moose isn't stripped away
$t->append_to_class('DBICTest::DumpMore::1::Foo', qq{use Moose;\n__PACKAGE__->meta->make_immutable;\n1;\n});

for my $supply_use_moose (1, 0) {
  $t->dump_test(
    classname => 'DBICTest::DumpMore::1',
    options => {
      $supply_use_moose ? (use_moose => 1) : (),
      result_base_class => 'My::ResultBaseClass',
      schema_base_class => 'My::SchemaBaseClass',
    },
    regexes => {
      schema => [
        qr/\nuse Moose;\nuse MooseX::MarkAsMethods autoclean => 1;\nextends 'My::SchemaBaseClass';\n\n/,
        qr/\n__PACKAGE__->meta->make_immutable\(inline_constructor => 0\);\n1;(?!\n1;\n)\n.*/,
      ],
      Foo => [
        qr/\nuse Moose;\nuse MooseX::NonMoose;\nuse MooseX::MarkAsMethods autoclean => 1;\nextends 'My::ResultBaseClass';\n\n/,
        qr/\n__PACKAGE__->meta->make_immutable;\n1;(?!\n1;\n)\n.*/,
      ],
      Bar => [
        qr/\nuse Moose;\nuse MooseX::NonMoose;\nuse MooseX::MarkAsMethods autoclean => 1;\nextends 'My::ResultBaseClass';\n\n/,
        qr/\n__PACKAGE__->meta->make_immutable;\n1;(?!\n1;\n)\n.*/,
      ],
    },
    neg_regexes => {
      Foo => [
#        qr/\nuse Moose;\n.*\nuse Moose;/s, # TODO
        qr/\n__PACKAGE__->meta->make_immutable;\n.*\n__PACKAGE__->meta->make_immutable;/s,
      ],
    },
  );
}

# check that a moose schema can *not* be downgraded

$t->dump_test (
  classname => 'DBICTest::DumpMore::1',
  options => {
    use_moose => 0,
    result_base_class => 'My::ResultBaseClass',
    schema_base_class => 'My::SchemaBaseClass',
  },
  error => qr/\QIt is not possible to "downgrade" a schema that was loaded with use_moose => 1\E/,
);

done_testing;
