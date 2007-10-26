package # hide from PAUSE 
    DBICTest::Schema::ArtistUndirectedMap;

use base 'DBIx::Class::Core';

__PACKAGE__->table('artist_undirected_map');
__PACKAGE__->add_columns(
  'id1' => { data_type => 'integer' },
  'id2' => { data_type => 'integer' },
);
__PACKAGE__->set_primary_key(qw/id1 id2/);

__PACKAGE__->belongs_to( 'artist1', 'DBICTest::Schema::Artist', 'id1' );
__PACKAGE__->belongs_to( 'artist2', 'DBICTest::Schema::Artist', 'id2');
__PACKAGE__->has_many(
  'mapped_artists', 'DBICTest::Schema::Artist',
  [ {'foreign.artistid' => 'self.id1'}, {'foreign.artistid' => 'self.id2'} ],
);

1;
