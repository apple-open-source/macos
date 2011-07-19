package # hide from PAUSE
    DBICTest::Schema;

use base qw/DBIx::Class::Schema/;

no warnings qw/qw/;

__PACKAGE__->load_classes(qw/
  Artist
  SequenceTest
  BindType
  Employee
  CD
  FileColumn
  Genre
  Link
  Bookmark
  #dummy
  Track
  Tag
  Year2000CDs
  Year1999CDs
  CustomSql
  Money
  /,
  { 'DBICTest::Schema' => [qw/
    LinerNotes
    Artwork
    Artwork_to_Artist
    Image
    Lyrics
    LyricVersion
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
    'Dummy',    # this is a real result class we remove in the hook below
  ),
  qw/SelfRefAlias TreeLike TwoKeyTreeLike Event EventTZ NoPrimaryKey/,
  qw/Collection CollectionObject TypedObject Owners BooksInLibrary/,
  qw/ForceForeign Encoded/,
);

sub sqlt_deploy_hook {
  my ($self, $sqlt_schema) = @_;

  $sqlt_schema->drop_table('dummy');
}

1;
