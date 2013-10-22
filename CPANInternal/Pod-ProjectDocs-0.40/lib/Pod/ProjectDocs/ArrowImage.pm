package Pod::ProjectDocs::ArrowImage;
use strict;
use warnings;
use base qw/Pod::ProjectDocs::File/;
use MIME::Base64;
use File::Basename;

__PACKAGE__->default_name('up.gif');
__PACKAGE__->data( do{ local $/; <DATA> } );
__PACKAGE__->is_bin(1);

sub tag {
    my($self, $doc) = @_;
    my($name, $path) = fileparse $doc->get_output_path, qw/\.html/;
    my $relpath = File::Spec->abs2rel($self->get_output_path, $path);
    $relpath =~ s:\\:/:g if $^O eq 'MSWin32';
    return sprintf qq|<a href="#TOP" class="toplink"><img alt="^" src="%s" /></a>|, $relpath;
}

sub _get_data {
    my $self = shift;
    return decode_base64($self->data);
}

1;
__DATA__
R0lGODlhDwAPAIAAAABmmf///yH5BAEAAAEALAAAAAAPAA8AAAIjhI8Jwe1tXlgvulMpS1crT33W
uGBkpm3pZEEr1qGZHEuSKBYAOw==
