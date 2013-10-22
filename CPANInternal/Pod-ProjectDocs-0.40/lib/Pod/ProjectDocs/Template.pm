package Pod::ProjectDocs::Template;
use strict;
use warnings;
use Template;
use File::Basename;
use File::Spec;

sub new {
    my $class = shift;
    my $self  = bless { }, $class;
    $self->_init(@_);
    return $self;
}

sub _init {
    my $self = shift;
    $self->{_curpath} = '';
    $self->{_tt} = Template->new( {
        FILTERS => {
            relpath => sub {
                my $path = shift;
                my $curpath = $self->{_curpath};
                my($name, $dir) = fileparse $curpath, qr/\.html/;
                return File::Spec->abs2rel($path, $dir);
            },
            return2br => sub {
                my $text = shift;
                $text =~ s!\r\n!<br />!g;
                $text =~ s!\n!<br />!g;
                return $text;
            }
        },
    } );
}

sub process {
    my($self, $doc, $data, $output) = @_;
    $self->{_curpath} = $doc->get_output_path;
    $self->{_tt}->process(\$data, $output, \my $text)
        or $self->_croak($self->{_tt}->error);
    $self->{_curpath} = '';
    return $text;
}

sub _croak {
    my($self, $msg) = @_;
    require Carp;
    Carp::croak($msg);
}

1;
__END__

