package Pod::ProjectDocs::Parser::PerlPod;
use strict;
use warnings;
use base qw/Pod::ProjectDocs::Parser/;
use URI::Escape;
__PACKAGE__->language('perl');

sub _makeLinkToCommunity {
    my ($self, $page) = @_;
    return "http://search.cpan.org/perldoc?" . URI::Escape::uri_escape($page);
}

1;
__END__

