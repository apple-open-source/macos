package Mail::Audit::MimeEntity;

# $Id: MimeEntity.pm,v 1.1 2004/04/09 17:04:47 dasenbro Exp $

use strict;
use File::Path;
use MIME::Parser;
use MIME::Entity;
use Mail::Audit::MailInternet;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $MIME_PARSER_TMPDIR);
@ISA = qw(Mail::Audit MIME::Entity);

$VERSION = '2.0';

$MIME_PARSER_TMPDIR = "/tmp/".getpwuid($>)."-mailaudit";

my $parser = MIME::Parser->new();

my @to_rmdir;

sub autotype_new { 
    my $class = shift;
    my $mailinternet = shift;

    $parser->ignore_errors(1);
    mkdir ($MIME_PARSER_TMPDIR, 0777);
    if (! -d $MIME_PARSER_TMPDIR) { $MIME_PARSER_TMPDIR = "/tmp" }
    $parser->output_under($MIME_PARSER_TMPDIR);

    # todo: add eval error trapping.  if there's a problem, return Mail::Audit::MailInternet as a fallback.
    my $self = eval { $parser->parse_data([@{$mailinternet->head->header}, "\n", @{$mailinternet->body}]); };
    my $error = ($@); # we won't look at $parser->last_error because we're trying to handle as much as we can.
    if ($error) {
	return (Mail::Audit::MailInternet->autotype_new( $mailinternet ), "encountered error during parse: $error");

	# note to self:
	# if the error was due to an ill-formed message/rfc822 attachment,
	# we could reparse with extract_nested_messages => 0.
	# it depends how badly the attachment is formed.
	# for now we have ignore_errors(1) and we won't look at $parser->last_error.
    }	

    push @to_rmdir, $parser->filer->output_dir;

    bless($self, $class);
    return ($self, 0);
}

sub DESTROY {
    rmtree(\@to_rmdir, 0, 1);

    # we don't want to rmdir the top-level tmpdir because other instances may be using that dir.
    # rmdir $MIME_PARSER_TMPDIR;
}

sub parser { $parser }

sub is_mime        { 1; }

1;

