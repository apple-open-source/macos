package Pod::ProjectDocs::Parser::JavaScriptPod;
use strict;
use warnings;
use base qw/Pod::ProjectDocs::Parser/;
use URI::Escape;
__PACKAGE__->language('javascript');

sub _makeLinkToCommunity {
    my ($self, $page) = @_;
    return "http://www.openjsan.org/go/?t=l&q=" . URI::Escape::uri_escape($page);
}

1;
__END__

