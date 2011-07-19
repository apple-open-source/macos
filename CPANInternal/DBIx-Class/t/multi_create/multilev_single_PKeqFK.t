use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

sub mc_diag { diag (@_) if $ENV{DBIC_MULTICREATE_DEBUG} };

my $schema = DBICTest->init_schema();

mc_diag (<<'DG');
* Test a multilevel might-have/has_one with a PK == FK in the mid-table

CD -> might have -> Artwork
    \- has_one -/     \
                       \
                        \-> has_many \
                                      --> Artwork_to_Artist
                        /-> has_many /
                       /
                     Artist
DG

my $rels = {
  has_one => 'mandatory_artwork',
  might_have => 'artwork',
};

for my $type (qw/has_one might_have/) {

  lives_ok (sub {

    my $rel = $rels->{$type};
    my $cd_title = "Simple test $type cd";

    my $cd = $schema->resultset('CD')->create ({
      artist => 1,
      title => $cd_title,
      year => 2008,
      $rel => {},
    });

    isa_ok ($cd, 'DBICTest::CD', 'Main CD object created');
    is ($cd->title, $cd_title, 'Correct CD title');

    isa_ok ($cd->$rel, 'DBICTest::Artwork', 'Related artwork present');
    ok ($cd->$rel->in_storage, 'And in storage');

  }, "Simple $type creation");
}

my $artist_rs = $schema->resultset('Artist');
for my $type (qw/has_one might_have/) {

  my $rel = $rels->{$type};

  my $cd_title = "Test $type cd";
  my $artist_names = [ map { "Artist via $type $_" } (1, 2) ];

  my $someartist = $artist_rs->next;

  lives_ok (sub {
    my $cd = $schema->resultset('CD')->create ({
      artist => $someartist,
      title => $cd_title,
      year => 2008,
      $rel => {
      artwork_to_artist => [ map {
            { artist => { name => $_ } }
          } (@$artist_names)
        ]
      },
    });


    isa_ok ($cd, 'DBICTest::CD', 'Main CD object created');
    is ($cd->title, $cd_title, 'Correct CD title');

    my $art_obj = $cd->$rel;
    ok ($art_obj->has_column_loaded ('cd_id'), 'PK/FK present on artwork object');
    is ($art_obj->artists->count, 2, 'Correct artwork creator count via the new object');
    is_deeply (
      [ sort $art_obj->artists->get_column ('name')->all ],
      $artist_names,
      'Artists named correctly when queried via object',
    );

    my $artwork = $schema->resultset('Artwork')->search (
      { 'cd.title' => $cd_title },
      { join => 'cd' },
    )->single;
    is ($artwork->artists->count, 2, 'Correct artwork creator count via a new search');
    is_deeply (
        [ sort $artwork->artists->get_column ('name')->all ],
      $artist_names,
      'Artists named correctly queried via a new search',
    );
  }, "multilevel $type with a PK == FK in the $type/has_many table ok");
}

done_testing;
