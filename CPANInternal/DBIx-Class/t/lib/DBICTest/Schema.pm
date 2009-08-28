package # hide from PAUSE
    DBICTest::Schema;

use base qw/DBIx::Class::Schema/;

no warnings qw/qw/;

__PACKAGE__->load_classes(qw/
  Artist
  Employee
  CD
  FileColumn
  Link
  Bookmark
  #dummy
  Track
  Tag
  /,
  { 'DBICTest::Schema' => [qw/
    LinerNotes
    OneKey
    #dummy
    TwoKeys
    Serialized
  /]},
  (
    'FourKeys',
    'FourKeys_to_TwoKeys',
    '#dummy',
    'SelfRef',
    'ArtistUndirectedMap',
    'ArtistSourceName',
    'ArtistSubclass',
    'Producer',
    'CD_to_Producer',
  ),
  qw/SelfRefAlias TreeLike TwoKeyTreeLike Event EventTZ NoPrimaryKey/,
  qw/Collection CollectionObject TypedObject/,
  qw/Owners BooksInLibrary/,
  qw/ForceForeign/  
);

sub sqlt_deploy_hook {
  my ($self, $sqlt_schema) = @_;

  $sqlt_schema->drop_table('link');
}

1;
