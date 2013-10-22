package Pod::ProjectDocs::DocManager;
use strict;
use warnings;
use base qw/Class::Accessor::Fast/;

use File::Find;
use Pod::ProjectDocs::Doc;

__PACKAGE__->mk_accessors(qw/
    config
    desc
    suffix
    parser
    docs
/);

sub new {
    my $class = shift;
    my $self = bless {}, $class;
    $self->_init(@_);
    return $self;
}

sub _init {
    my ( $self, %args ) = @_;
    $args{suffix} = [ $args{suffix} ] unless ref $args{suffix};
    $self->config( $args{config} );
    $self->desc( $args{desc} );
    $self->suffix( $args{suffix} );
    $self->parser( $args{parser} );
    $self->docs( [] );
    $self->_find_files;
}

sub _find_files {
    my $self = shift;
    foreach my $dir ( @{ $self->config->libroot } ) {
        unless ( -e $dir && -d _ ) {
            $self->_croak(qq/$dir isn't detected or it's not a directory./);
        }
    }
    my $suffixs = $self->suffix;
    foreach my $dir ( @{ $self->config->libroot } ) {
        foreach my $suffix (@$suffixs) {
            my $wanted = sub {
                return unless $File::Find::name =~ /\.$suffix$/;
                ( my $path = $File::Find::name ) =~ s#^\\.##;
                my ( $fname, $fdir ) =
                  File::Basename::fileparse( $path, qr/\.$suffix/ );
                my $reldir = File::Spec->abs2rel( $fdir, $dir );
                $reldir ||= File::Spec->curdir;
                my $relpath = File::Spec->catdir( $reldir, $fname );
                $relpath .= ".";
                $relpath .= $suffix;
                $relpath =~ s:\\:/:g if $^O eq 'MSWin32';
                my $matched = 0;

                foreach my $regex ( @{ $self->config->except } ) {
                    if ( $relpath =~ /$regex/ ) {
                        $matched = 1;
                        last;
                    }
                }
                unless ($matched) {
                    push @{ $self->docs },
                      Pod::ProjectDocs::Doc->new(
                        config      => $self->config,
                        origin      => $path,
                        origin_root => $dir,
                        suffix      => $suffix,
                      );
                }
            };
            File::Find::find( { no_chdir => 1, wanted => $wanted }, $dir );
        }
    }
    $self->docs( [ sort { $a->name cmp $b->name } @{ $self->docs } ] );
}

sub get_doc_names {
    my $self = shift;
    my @names = map { $_->name } @{ $self->docs };
    return wantarray ? @names : \@names;
}

sub get_docs_num {
    my $self = shift;
    return scalar @{ $self->docs };
}

sub get_doc_at {
    my ( $self, $index ) = @_;
    return $self->docs->[$index];
}

sub doc_iterator {
    my $self = shift;
    return Pod::ProjectDocs::DocManager::Iterator->new($self);
}

sub _croak {
    my ( $self, $msg ) = @_;
    require Carp;
    Carp::croak($msg);
}

package Pod::ProjectDocs::DocManager::Iterator;
use base qw/Class::Accessor::Fast/;
__PACKAGE__->mk_accessors(qw/manager index/);

sub new {
    my $class = shift;
    my $self = bless {}, $class;
    $self->_init(@_);
    return $self;
}

sub _init {
    my ( $self, $manager ) = @_;
    $self->index(0);
    $self->manager($manager);
}

sub next {
    my $self = shift;
    if ( $self->manager->get_docs_num > $self->index ) {
        my $doc = $self->manager->get_doc_at( $self->index );
        $self->index( $self->index + 1 );
        return $doc;
    }
    else {
        return undef;
    }
}

sub reset {
    my $self = shift;
    $self->index(0);
}

1;
__END__

