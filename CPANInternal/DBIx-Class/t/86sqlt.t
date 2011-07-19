use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;

BEGIN {
  require DBIx::Class;
  plan skip_all =>
      'Test needs ' . DBIx::Class::Optional::Dependencies->req_missing_for ('deploy')
    unless DBIx::Class::Optional::Dependencies->req_ok_for ('deploy')
}

my $custom_deployment_statements_called = 0;

sub DBICTest::Schema::deployment_statements {
  $custom_deployment_statements_called = 1;
  my $self = shift;
  return $self->next::method(@_);
}

my $schema = DBICTest->init_schema (no_deploy => 1);


# Check deployment statements ctx sensitivity
{
  my $not_first_table_creation_re = qr/CREATE TABLE fourkeys_to_twokeys/;


  my $statements = $schema->deployment_statements;
  like (
    $statements,
    $not_first_table_creation_re,
    'All create statements returned in 1 string in scalar ctx'
  );

  my @statements = $schema->deployment_statements;
  cmp_ok (scalar @statements, '>', 1, 'Multiple statement lines in array ctx');

  my $i = 0;
  while ($i <= $#statements) {
    last if $statements[$i] =~ $not_first_table_creation_re;
    $i++;
  }

  ok (
    ($i > 0) && ($i <= $#statements),
    "Creation statement was found somewherere within array ($i)"
  );
}



{
  my $deploy_hook_called = 0;

  # replace the sqlt calback with a custom version ading an index
  $schema->source('Track')->sqlt_deploy_callback(sub {
    my ($self, $sqlt_table) = @_;

    $deploy_hook_called = 1;

    is (
      $sqlt_table->schema->translator->producer_type,
      join ('::', 'SQL::Translator::Producer', $schema->storage->sqlt_type),
      'Production type passed to translator object',
    );

    if ($schema->storage->sqlt_type eq 'SQLite' ) {
      $sqlt_table->add_index( name => 'track_title', fields => ['title'] )
        or die $sqlt_table->error;
    }

    $self->default_sqlt_deploy_hook($sqlt_table);
  });

  $schema->deploy; # do not remove, this fires the is() test in the callback above
  ok($deploy_hook_called, 'deploy hook got called');
  ok($custom_deployment_statements_called, '->deploy used the schemas deploy_statements method');
}


my $translator = SQL::Translator->new( 
  parser_args => {
    'DBIx::Schema' => $schema,
  },
  producer_args => {},
);

{
    my $warn = '';
    local $SIG{__WARN__} = sub { $warn = shift };

    my $relinfo = $schema->source('Artist')->relationship_info ('cds');
    local $relinfo->{attrs}{on_delete} = 'restrict';


    $translator->parser('SQL::Translator::Parser::DBIx::Class');
    $translator->producer('SQLite');

    my $output = $translator->translate();

    ok($output, "SQLT produced someoutput")
      or diag($translator->error);


    like (
      $warn,
      qr/SQLT attribute .+? was supplied for relationship .+? which does not appear to be a foreign constraint/,
      'Warn about dubious on_delete/on_update attributes',
    );
}

# Note that the constraints listed here are the only ones that are tested -- if
# more exist in the Schema than are listed here and all listed constraints are
# correct, the test will still pass. If you add a class with UNIQUE or FOREIGN
# KEY constraints to DBICTest::Schema, add tests here if you think the existing
# test coverage is not sufficient

my %fk_constraints = (

  # TwoKeys
  twokeys => [
    {
      'display' => 'twokeys->cd',
      'name' => 'twokeys_fk_cd', 'index_name' => 'twokeys_idx_cd',
      'selftable' => 'twokeys', 'foreigntable' => 'cd', 
      'selfcols'  => ['cd'], 'foreigncols' => ['cdid'], 
      'noindex'  => 1,
      on_delete => '', on_update => '', deferrable => 0,
    },
    {
      'display' => 'twokeys->artist',
      'name' => 'twokeys_fk_artist', 'index_name' => 'twokeys_idx_artist',
      'selftable' => 'twokeys', 'foreigntable' => 'artist', 
      'selfcols'  => ['artist'], 'foreigncols' => ['artistid'],
      on_delete => 'CASCADE', on_update => 'CASCADE', deferrable => 1,
    },
  ],

  # FourKeys_to_TwoKeys
  fourkeys_to_twokeys => [
    {
      'display' => 'fourkeys_to_twokeys->twokeys',
      'name' => 'fourkeys_to_twokeys_fk_t_artist_t_cd', 'index_name' => 'fourkeys_to_twokeys_idx_t_artist_t_cd',
      'selftable' => 'fourkeys_to_twokeys', 'foreigntable' => 'twokeys', 
      'selfcols'  => ['t_artist', 't_cd'], 'foreigncols' => ['artist', 'cd'], 
      on_delete => 'CASCADE', on_update => 'CASCADE', deferrable => 1,
    },
    {
      'display' => 'fourkeys_to_twokeys->fourkeys', 'index_name' => 'fourkeys_to_twokeys_idx_f_foo_f_bar_f_hello_f_goodbye',
      'name' => 'fourkeys_to_twokeys_fk_f_foo_f_bar_f_hello_f_goodbye',
      'selftable' => 'fourkeys_to_twokeys', 'foreigntable' => 'fourkeys', 
      'selfcols'  => [qw(f_foo f_bar f_hello f_goodbye)],
      'foreigncols' => [qw(foo bar hello goodbye)], 
      on_delete => 'CASCADE', on_update => 'CASCADE', deferrable => 1,
    },
  ],

  # CD_to_Producer
  cd_to_producer => [
    {
      'display' => 'cd_to_producer->cd',
      'name' => 'cd_to_producer_fk_cd', 'index_name' => 'cd_to_producer_idx_cd',
      'selftable' => 'cd_to_producer', 'foreigntable' => 'cd', 
      'selfcols'  => ['cd'], 'foreigncols' => ['cdid'],
      on_delete => 'CASCADE', on_update => 'CASCADE', deferrable => 1,
    },
    {
      'display' => 'cd_to_producer->producer',
      'name' => 'cd_to_producer_fk_producer', 'index_name' => 'cd_to_producer_idx_producer',
      'selftable' => 'cd_to_producer', 'foreigntable' => 'producer', 
      'selfcols'  => ['producer'], 'foreigncols' => ['producerid'],
      on_delete => '', on_update => '', deferrable => 1,
    },
  ],

  # Self_ref_alias
  self_ref_alias => [
    {
      'display' => 'self_ref_alias->self_ref for self_ref',
      'name' => 'self_ref_alias_fk_self_ref', 'index_name' => 'self_ref_alias_idx_self_ref',
      'selftable' => 'self_ref_alias', 'foreigntable' => 'self_ref', 
      'selfcols'  => ['self_ref'], 'foreigncols' => ['id'],
      on_delete => 'CASCADE', on_update => 'CASCADE', deferrable => 1,
    },
    {
      'display' => 'self_ref_alias->self_ref for alias',
      'name' => 'self_ref_alias_fk_alias', 'index_name' => 'self_ref_alias_idx_alias',
      'selftable' => 'self_ref_alias', 'foreigntable' => 'self_ref', 
      'selfcols'  => ['alias'], 'foreigncols' => ['id'],
      on_delete => '', on_update => '', deferrable => 1,
    },
  ],

  # CD
  cd => [
    {
      'display' => 'cd->artist',
      'name' => 'cd_fk_artist', 'index_name' => 'cd_idx_artist',
      'selftable' => 'cd', 'foreigntable' => 'artist', 
      'selfcols'  => ['artist'], 'foreigncols' => ['artistid'],
      on_delete => 'CASCADE', on_update => 'CASCADE', deferrable => 1,
    },
  ],

  # Artist_undirected_map
  artist_undirected_map => [
    {
      'display' => 'artist_undirected_map->artist for id1',
      'name' => 'artist_undirected_map_fk_id1', 'index_name' => 'artist_undirected_map_idx_id1',
      'selftable' => 'artist_undirected_map', 'foreigntable' => 'artist', 
      'selfcols'  => ['id1'], 'foreigncols' => ['artistid'],
      on_delete => 'RESTRICT', on_update => 'CASCADE', deferrable => 1,
    },
    {
      'display' => 'artist_undirected_map->artist for id2',
      'name' => 'artist_undirected_map_fk_id2', 'index_name' => 'artist_undirected_map_idx_id2',
      'selftable' => 'artist_undirected_map', 'foreigntable' => 'artist', 
      'selfcols'  => ['id2'], 'foreigncols' => ['artistid'],
      on_delete => '', on_update => '', deferrable => 1,
    },
  ],

  # Track
  track => [
    {
      'display' => 'track->cd',
      'name' => 'track_fk_cd', 'index_name' => 'track_idx_cd',
      'selftable' => 'track', 'foreigntable' => 'cd', 
      'selfcols'  => ['cd'], 'foreigncols' => ['cdid'],
      on_delete => 'CASCADE', on_update => 'CASCADE', deferrable => 1,
    },
  ],

  # TreeLike
  treelike => [
    {
      'display' => 'treelike->treelike for parent',
      'name' => 'treelike_fk_parent', 'index_name' => 'treelike_idx_parent',
      'selftable' => 'treelike', 'foreigntable' => 'treelike', 
      'selfcols'  => ['parent'], 'foreigncols' => ['id'],
      on_delete => 'CASCADE', on_update => 'CASCADE', deferrable => 1,
    },
  ],

  # TwoKeyTreeLike
  twokeytreelike => [
    {
      'display' => 'twokeytreelike->twokeytreelike for parent1,parent2',
      'name' => 'twokeytreelike_fk_parent1_parent2', 'index_name' => 'twokeytreelike_idx_parent1_parent2',
      'selftable' => 'twokeytreelike', 'foreigntable' => 'twokeytreelike', 
      'selfcols'  => ['parent1', 'parent2'], 'foreigncols' => ['id1','id2'],
      on_delete => '', on_update => '', deferrable => 1,
    },
  ],

  # Tags
  tags => [
    {
      'display' => 'tags->cd',
      'name' => 'tags_fk_cd', 'index_name' => 'tags_idx_cd',
      'selftable' => 'tags', 'foreigntable' => 'cd', 
      'selfcols'  => ['cd'], 'foreigncols' => ['cdid'],
      on_delete => 'CASCADE', on_update => 'CASCADE', deferrable => 1,
    },
  ],

  # Bookmark
  bookmark => [
    {
      'display' => 'bookmark->link',
      'name' => 'bookmark_fk_link', 'index_name' => 'bookmark_idx_link',
      'selftable' => 'bookmark', 'foreigntable' => 'link', 
      'selfcols'  => ['link'], 'foreigncols' => ['id'],
      on_delete => 'SET NULL', on_update => 'CASCADE', deferrable => 1,
    },
  ],
  # ForceForeign
  forceforeign => [
    {
      'display' => 'forceforeign->artist',
      'name' => 'forceforeign_fk_artist', 'index_name' => 'forceforeign_idx_artist',
      'selftable' => 'forceforeign', 'foreigntable' => 'artist', 
      'selfcols'  => ['artist'], 'foreigncols' => ['artistid'], 
      'noindex'  => 1,
      on_delete => '', on_update => '', deferrable => 1,
    },
  ],
);

my %unique_constraints = (
  # CD
  cd => [
    {
      'display' => 'cd artist and title unique',
      'name' => 'cd_artist_title',
      'table' => 'cd', 'cols' => ['artist', 'title'],
    },
  ],

  # Producer
  producer => [
    {
      'display' => 'producer name unique',
      'name' => 'prod_name', # explicit name
      'table' => 'producer', 'cols' => ['name'],
    },
  ],

  # TwoKeyTreeLike
  twokeytreelike => [
    {
      'display' => 'twokeytreelike name unique',
      'name' => 'tktlnameunique', # explicit name
      'table' => 'twokeytreelike', 'cols'  => ['name'],
    },
  ],

  # Employee
# Constraint is commented out in DBICTest/Schema/Employee.pm
#  employee => [
#    {
#      'display' => 'employee position and group_id unique',
#      'name' => 'position_group',
#      'table' => 'employee', cols => ['position', 'group_id'],
#    },
#  ],
);

my %indexes = (
  artist => [
    {
      'fields' => ['name']
    },
  ],
  track => [
    {
      'fields' => ['title']
    }
  ],
);

my $tschema = $translator->schema();
# Test that the $schema->sqlt_deploy_hook was called okay and that it removed
# the 'dummy' table
ok( !defined($tschema->get_table('dummy')), "Dummy table was removed by hook");

# Test that the Artist resultsource sqlt_deploy_hook was called okay and added
# an index
SKIP: {
    skip ('Artist sqlt_deploy_hook is only called with an SQLite backend', 1)
        if $schema->storage->sqlt_type ne 'SQLite';

    ok( ( grep 
        { $_->name eq 'artist_name_hookidx' }
        $tschema->get_table('artist')->get_indices
    ), 'sqlt_deploy_hook fired within a resultsource');
}

# Test that nonexistent constraints are not found
my $constraint = get_constraint('FOREIGN KEY', 'cd', ['title'], 'cd', ['year']);
ok( !defined($constraint), 'nonexistent FOREIGN KEY constraint not found' );
$constraint = get_constraint('UNIQUE', 'cd', ['artist']);
ok( !defined($constraint), 'nonexistent UNIQUE constraint not found' );
$constraint = get_constraint('FOREIGN KEY', 'forceforeign', ['cd'], 'cd', ['cdid']);
ok( !defined($constraint), 'forced nonexistent FOREIGN KEY constraint not found' );

for my $expected_constraints (keys %fk_constraints) {
  for my $expected_constraint (@{ $fk_constraints{$expected_constraints} }) {
    my $desc = $expected_constraint->{display};
    my $constraint = get_constraint(
      'FOREIGN KEY',
      $expected_constraint->{selftable}, $expected_constraint->{selfcols},
      $expected_constraint->{foreigntable}, $expected_constraint->{foreigncols},
    );
    ok( defined($constraint), "FOREIGN KEY constraint matching `$desc' found" );
    test_fk($expected_constraint, $constraint);
  }
}

for my $expected_constraints (keys %unique_constraints) {
  for my $expected_constraint (@{ $unique_constraints{$expected_constraints} }) {
    my $desc = $expected_constraint->{display};
    my $constraint = get_constraint(
      'UNIQUE', $expected_constraint->{table}, $expected_constraint->{cols},
    );
    ok( defined($constraint), "UNIQUE constraint matching `$desc' found" );
    test_unique($expected_constraint, $constraint);
  }
}

for my $table_index (keys %indexes) {
  for my $expected_index ( @{ $indexes{$table_index} } ) {
    ok ( get_index($table_index, $expected_index), "Got a matching index on $table_index table");
  }
}

# Returns the Constraint object for the specified constraint type, table and
# columns from the SQL::Translator schema, or undef if no matching constraint
# is found.
#
# NB: $type is either 'FOREIGN KEY' or 'UNIQUE'. In UNIQUE constraints the last
# two parameters are not used.
sub get_constraint {
  my ($type, $table_name, $cols, $f_table, $f_cols) = @_;
  $f_table ||= ''; # For UNIQUE constraints, reference_table is ''
  $f_cols ||= [];

  my $table = $tschema->get_table($table_name);

  my %fields = map { $_ => 1 } @$cols;
  my %f_fields = map { $_ => 1 } @$f_cols;

  die "No $table_name" unless $table;
 CONSTRAINT:
  for my $constraint ( $table->get_constraints ) {
    next unless $constraint->type eq $type;
    next unless $constraint->reference_table eq $f_table;

    my %rev_fields = map { $_ => 1 } $constraint->fields;
    my %rev_f_fields = map { $_ => 1 } $constraint->reference_fields;

    # Check that the given fields are a subset of the constraint's fields
    for my $field ($constraint->fields) {
      next CONSTRAINT unless $fields{$field};
    }
    if ($type eq 'FOREIGN KEY') {
      for my $f_field ($constraint->reference_fields) {
        next CONSTRAINT unless $f_fields{$f_field};
      }
    }

    # Check that the constraint's fields are a subset of the given fields
    for my $field (@$cols) {
      next CONSTRAINT unless $rev_fields{$field};
    }
    if ($type eq 'FOREIGN KEY') {
      for my $f_field (@$f_cols) {
        next CONSTRAINT unless $rev_f_fields{$f_field};
      }
    }

    return $constraint; # everything passes, found the constraint
  }
  return undef; # didn't find a matching constraint
}

sub get_index {
  my ($table_name, $index) = @_;

  my $table = $tschema->get_table($table_name);

 CAND_INDEX:
  for my $cand_index ( $table->get_indices ) {
   
    next CAND_INDEX if $index->{name} && $cand_index->name ne $index->{name}
                    || $index->{type} && $cand_index->type ne $index->{type};

    my %idx_fields = map { $_ => 1 } $cand_index->fields;

    for my $field ( @{ $index->{fields} } ) {
      next CAND_INDEX unless $idx_fields{$field};
    }

    %idx_fields = map { $_ => 1 } @{$index->{fields}};
    for my $field ( $cand_index->fields) {
      next CAND_INDEX unless $idx_fields{$field};
    }

    return $cand_index;
  }

  return undef; # No matching idx
}

# Test parameters in a FOREIGN KEY constraint other than columns
sub test_fk {
  my ($expected, $got) = @_;
  my $desc = $expected->{display};
  is( $got->name, $expected->{name},
      "name parameter correct for '$desc'" );
  is( $got->on_delete, $expected->{on_delete},
      "on_delete parameter correct for '$desc'" );
  is( $got->on_update, $expected->{on_update},
      "on_update parameter correct for '$desc'" );
  is( $got->deferrable, $expected->{deferrable},
      "is_deferrable parameter correct for '$desc'" );

  my $index = get_index( $got->table, { fields => $expected->{selfcols} } );

  if ($expected->{noindex}) {
      ok( !defined $index, "index doesn't for '$desc'" );
  } else {
      ok( defined $index, "index exists for '$desc'" );
      is( $index->name, $expected->{index_name}, "index has correct name for '$desc'" );
  }
}

sub test_unique {
  my ($expected, $got) = @_;
  my $desc = $expected->{display};
  is( $got->name, $expected->{name},
      "name parameter correct for '$desc'" );
}

done_testing;
