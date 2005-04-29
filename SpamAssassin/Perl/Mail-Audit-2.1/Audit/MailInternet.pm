package Mail::Audit::MailInternet;

# $Id: MailInternet.pm,v 1.1 2004/04/09 17:04:47 dasenbro Exp $

use strict;
use Mail::Internet;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
@ISA = qw(Mail::Audit Mail::Internet);

$VERSION = '2.0';

sub autotype_new { 
    my $class = shift;
    my $self = shift;
    bless($self, $class);
}

sub is_mime        { 0; }

1;
