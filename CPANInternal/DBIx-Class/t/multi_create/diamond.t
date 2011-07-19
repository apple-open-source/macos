use strict;
use warnings;

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

sub mc_diag { diag (@_) if $ENV{DBIC_MULTICREATE_DEBUG} };

my $schema = DBICTest->init_schema();

mc_diag (<<'DG');
* Try a diamond multicreate

Artist -> has_many -> Artwork_to_Artist -> belongs_to
                                               /
  belongs_to <- CD <- belongs_to <- Artwork <-/
    \
     \-> Artist2

DG

lives_ok (sub {
  $schema->resultset ('Artist')->create ({
    name => 'The wooled wolf',
    artwork_to_artist => [{
      artwork => {
        cd => {
          title => 'Wool explosive',
          year => 1999,
          artist => { name => 'The black exploding sheep' },
        }
      }
    }],
  });

  my $art2 = $schema->resultset ('Artist')->find ({ name => 'The black exploding sheep' });
  ok ($art2, 'Second artist exists');

  my $cd = $art2->cds->single;
  is ($cd->title, 'Wool explosive', 'correctly created CD');

  is_deeply (
    [ $cd->artwork->artists->get_column ('name')->all ],
    [ 'The wooled wolf' ],
    'Artist correctly attached to artwork',
  );

}, 'Diamond chain creation ok');

done_testing;
