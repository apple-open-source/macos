use Test;
BEGIN { plan tests => 2 }
use XML::SAX::Base;
use strict;

# Tests for in-stream switch of Handler classes.
my $handler = HandlerOne->new();
my $filter  = FilterOne->new( ContentHandler => $handler );
my $driver  = Driver->new( Handler => $filter);

ok( $filter->get_handler('ContentHandler') =~ /HandlerOne/ );
ok( $filter->get_content_handler() =~ /HandlerOne/ );

# end main

package HandlerOne;
use base qw(XML::SAX::Base);

1;

package FilterOne;
use base qw(XML::SAX::Base);

1;


package Driver;
use base qw(XML::SAX::Base);

1;

