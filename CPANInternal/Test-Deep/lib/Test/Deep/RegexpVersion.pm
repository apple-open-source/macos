use strict;
use warnings;

package Test::Deep::RegexpVersion;

use vars qw( $OldStyle );

# Older versions of Perl treated Regexp refs as opaque scalars blessed
# into the "Regexp" class. Several bits of code need this so we
# centralise the test for that kind of version.
$OldStyle = ($] < 5.011);

1;
