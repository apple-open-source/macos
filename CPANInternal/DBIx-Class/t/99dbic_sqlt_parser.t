use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;
use DBICTest::Schema;
use Scalar::Util ();

BEGIN {
  require DBIx::Class;
  plan skip_all =>
      'Test needs ' . DBIx::Class::Optional::Dependencies->req_missing_for ('deploy')
    unless DBIx::Class::Optional::Dependencies->req_ok_for ('deploy')
}

# Test for SQLT-related leaks
{
  my $s = DBICTest::Schema->clone;
  my $sqlt_schema = create_schema ({ schema => $s });
  Scalar::Util::weaken ($s);

  ok (!$s, 'Schema not leaked');

  isa_ok ($sqlt_schema, 'SQL::Translator::Schema', 'SQLT schema object produced');
}

# make sure classname-style works
lives_ok { isa_ok (create_schema ({ schema => 'DBICTest::Schema' }), 'SQL::Translator::Schema', 'SQLT schema object produced') };


my $schema = DBICTest->init_schema();
# Dummy was yanked out by the sqlt hook test
# CustomSql tests the horrific/deprecated ->name(\$sql) hack
# YearXXXXCDs are views
#
my @sources = grep
  { $_ !~ /^ (?: Dummy | CustomSql | Year\d{4}CDs ) $/x }
  $schema->sources
;

my $idx_exceptions = {
    'Artwork'       => -1,
    'ForceForeign'  => -1,
    'LinerNotes'    => -1,
    'TwoKeys'       => -1, # TwoKeys has the index turned off on the rel def
};

{
  my $sqlt_schema = create_schema({ schema => $schema, args => { parser_args => { } } });

  foreach my $source_name (@sources) {
    my $table = get_table($sqlt_schema, $schema, $source_name);

    my $fk_count = scalar(grep { $_->type eq 'FOREIGN KEY' } $table->get_constraints);
    $fk_count += $idx_exceptions->{$source_name} || 0;
    my @indices = $table->get_indices;

    my $index_count = scalar(@indices);
    is($index_count, $fk_count, "correct number of indices for $source_name with no args");

    for my $index (@indices) {
        my $source = $schema->source($source_name);
        my $pk_test = join("\x00", $source->primary_columns);
        my $idx_test = join("\x00", $index->fields);
        isnt ( $pk_test, $idx_test, "no additional index for the primary columns exists in $source_name");
    }
  }
}

{
  my $sqlt_schema = create_schema({ schema => $schema, args => { parser_args => { add_fk_index => 1 } } });

  foreach my $source_name (@sources) {
    my $table = get_table($sqlt_schema, $schema, $source_name);

    my $fk_count = scalar(grep { $_->type eq 'FOREIGN KEY' } $table->get_constraints);
    $fk_count += $idx_exceptions->{$source_name} || 0;
    my @indices = $table->get_indices;
    my $index_count = scalar(@indices);
    is($index_count, $fk_count, "correct number of indices for $source_name with add_fk_index => 1");
  }
}

{
  my $sqlt_schema = create_schema({ schema => $schema, args => { parser_args => { add_fk_index => 0 } } });

  foreach my $source (@sources) {
    my $table = get_table($sqlt_schema, $schema, $source);

    my @indices = $table->get_indices;
    my $index_count = scalar(@indices);
    is($index_count, 0, "correct number of indices for $source with add_fk_index => 0");
  }
}

{
    {
        package # hide from PAUSE
            DBICTest::Schema::NoViewDefinition;

        use base qw/DBICTest::BaseResult/;

        __PACKAGE__->table_class('DBIx::Class::ResultSource::View');
        __PACKAGE__->table('noviewdefinition');

        1;
    }

    my $schema_invalid_view = $schema->clone;
    $schema_invalid_view->register_class('NoViewDefinition', 'DBICTest::Schema::NoViewDefinition');

    throws_ok { create_schema({ schema => $schema_invalid_view }) }
        qr/view noviewdefinition is missing a view_definition/,
        'parser detects views with a view_definition';
}

lives_ok (sub {
  my $sqlt_schema = create_schema ({
    schema => $schema,
    args => {
      parser_args => {
        sources => ['CD']
      },
    },
  });

  is_deeply (
    [$sqlt_schema->get_tables ],
    ['cd'],
    'sources limitng with relationships works',
  );

});

done_testing;

sub create_schema {
  my $args = shift;

  my $schema = $args->{schema};
  my $additional_sqltargs = $args->{args} || {};

  my $sqltargs = {
    add_drop_table => 1, 
    ignore_constraint_names => 1,
    ignore_index_names => 1,
    %{$additional_sqltargs}
  };

  my $sqlt = SQL::Translator->new( $sqltargs );

  $sqlt->parser('SQL::Translator::Parser::DBIx::Class');
  return $sqlt->translate({ data => $schema }) || die $sqlt->error;
}

sub get_table {
    my ($sqlt_schema, $schema, $source) = @_;

    my $table_name = $schema->source($source)->from;
    $table_name    = $$table_name if ref $table_name;

    return $sqlt_schema->get_table($table_name);
}
