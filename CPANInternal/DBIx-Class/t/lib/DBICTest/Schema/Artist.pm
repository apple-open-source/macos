package # hide from PAUSE 
    DBICTest::Schema::Artist;

use base qw/DBICTest::BaseResult/;

__PACKAGE__->table('artist');
__PACKAGE__->source_info({
    "source_info_key_A" => "source_info_value_A",
    "source_info_key_B" => "source_info_value_B",
    "source_info_key_C" => "source_info_value_C",
});
__PACKAGE__->add_columns(
  'artistid' => {
    data_type => 'integer',
    is_auto_increment => 1,
  },
  'name' => {
    data_type => 'varchar',
    size      => 100,
    is_nullable => 1,
  },
  rank => {
    data_type => 'integer',
    default_value => 13,
  },
  charfield => {
    data_type => 'char',
    size => 10,
    is_nullable => 1,
  },
);
__PACKAGE__->set_primary_key('artistid');
__PACKAGE__->add_unique_constraint(artist => ['artistid']); # do not remove, part of a test

__PACKAGE__->mk_classdata('field_name_for', {
    artistid    => 'primary key',
    name        => 'artist name',
});

__PACKAGE__->has_many(
    cds => 'DBICTest::Schema::CD', undef,
    { order_by => 'year' },
);
__PACKAGE__->has_many(
    cds_unordered => 'DBICTest::Schema::CD'
);
__PACKAGE__->has_many(
    cds_very_very_very_long_relationship_name => 'DBICTest::Schema::CD'
);

__PACKAGE__->has_many( twokeys => 'DBICTest::Schema::TwoKeys' );
__PACKAGE__->has_many( onekeys => 'DBICTest::Schema::OneKey' );

__PACKAGE__->has_many(
  artist_undirected_maps => 'DBICTest::Schema::ArtistUndirectedMap',
  [ {'foreign.id1' => 'self.artistid'}, {'foreign.id2' => 'self.artistid'} ],
  { cascade_copy => 0 } # this would *so* not make sense
);

__PACKAGE__->has_many(
    artwork_to_artist => 'DBICTest::Schema::Artwork_to_Artist' => 'artist_id'
);
__PACKAGE__->many_to_many('artworks', 'artwork_to_artist', 'artwork');


sub sqlt_deploy_hook {
  my ($self, $sqlt_table) = @_;

  if ($sqlt_table->schema->translator->producer_type =~ /SQLite$/ ) {
    $sqlt_table->add_index( name => 'artist_name_hookidx', fields => ['name'] )
      or die $sqlt_table->error;
  }
}

sub store_column {
  my ($self, $name, $value) = @_;
  $value = 'X '.$value if ($name eq 'name' && $value && $value =~ /(X )?store_column test/);
  $self->next::method($name, $value);
}


1;
