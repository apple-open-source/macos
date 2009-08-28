package DBIx::Class::InflateColumn::File;

use strict;
use warnings;
use base 'DBIx::Class';
use File::Path;
use File::Copy;
use IO::File;

__PACKAGE__->load_components(qw/InflateColumn/);


sub register_column {
  my ($self, $column, $info, @rest) = @_;
  $self->next::method($column, $info, @rest);
  return unless defined($info->{is_file_column});
    $self->inflate_column(
      $column =>
        {
          inflate => sub { 
            my ($value, $obj) = @_;
            #$self->_inflate_file_column;
          },
          deflate => sub {
            my ($value, $obj) = @_;
            #my ( $file, @column_names ) = $self->_load_file_column_information;
            #$self->_save_file_column( $file, $self, @column_names );
          },
        }
    );
}


sub delete {
    my ( $self, @rest ) = @_;

    my @column_names = $self->columns;
    for (@column_names) {
        if ( $self->column_info($_)->{is_file_column} ) {
            my $path =
              File::Spec->catdir( $self->column_info($_)->{file_column_path},
                $self->id );
            rmtree( [$path], 0, 0 );
        }
    }

    my $ret = $self->next::method(@rest);

    return $ret;
}

sub _inflate_file_column {
    my $self = shift;

    my @column_names = $self->columns;
    for(@column_names) {
        if ( $self->column_info($_)->{is_file_column} ) {
            # make sure everything checks out
            unless (defined $self->$_) {
                # if something is wrong set it to undef
                $self->$_(undef);
                next;
            }
            my $fs_file =
              File::Spec->catfile( $self->column_info($_)->{file_column_path}, 
                $self->id, $self->$_ );
            $self->$_({handle => new IO::File($fs_file, "r"), filename => $self->$_});
        }
    }
}

sub _load_file_column_information {
    my $self = shift;

    my $file;
    my @column_names;

    @column_names = $self->columns;
    for (@column_names) {
        if ( $self->column_info($_)->{is_file_column} ) {
            # make sure everything checks out
            unless ((defined $self->$_) ||
             (defined $self->$_->{filename} && defined $self->$_->{handle})) {
                # if something is wrong set it to undef
                $self->$_(undef);
                next;
            }
            $file->{$_} = $self->$_;
            $self->$_( $self->$_->{filename} );
        }
    }

    return ( $file, @column_names );
}

sub _save_file_column {
    my ( $self, $file, $ret, @column_names ) = @_;

    for (@column_names) {
        if ( $ret->column_info($_)->{is_file_column} ) {
            next unless (defined $ret->$_);
            my $file_path =
              File::Spec->catdir( $ret->column_info($_)->{file_column_path},
                $ret->id );
            mkpath [$file_path];
            
            my $outfile =
              File::Spec->catfile( $file_path, $file->{$_}->{filename} );
            File::Copy::copy( $file->{$_}->{handle}, $outfile );
        
            $self->_file_column_callback($file->{$_},$ret,$_);
        }
    }
}

=head1 NAME

DBIx::Class::InflateColumn::File -  map files from the Database to the filesystem.

=head1 SYNOPSIS

In your L<DBIx::Class> table class:

    __PACKAGE__->load_components( "PK::Auto", "InflateColumn::File", "Core" );
    
    # define your columns
    __PACKAGE__->add_columns(
        "id",
        {
            data_type         => "integer",
            is_auto_increment => 1,
            is_nullable       => 0,
            size              => 4,
        },
        "filename",
        {
            data_type           => "varchar",
            is_file_column      => 1,
            file_column_path    =>'/tmp/uploaded_files',
            # or for a Catalyst application 
            # file_column_path  => MyApp->path_to('root','static','files'),
            default_value       => undef,
            is_nullable         => 1,
            size                => 255,
        },
    );
    

In your L<Catalyst::Controller> class:

FileColumn requires a hash that contains L<IO::File> as handle and the file's
name as name.

    my $entry = $c->model('MyAppDB::Articles')->create({ 
        subject => 'blah',
        filename => { 
            handle => $c->req->upload('myupload')->fh, 
            filename => $c->req->upload('myupload')->basename 
        },
        body => '....'
    });
    $c->stash->{entry}=$entry;
    

And Place the following in your TT template
    
    Article Subject: [% entry.subject %]
    Uploaded File: 
    <a href="/static/files/[% entry.id %]/[% entry.filename.filename %]">File</a>
    Body: [% entry.body %]
    
The file will be stored on the filesystem for later retrieval.  Calling delete
on your resultset will delete the file from the filesystem.  Retrevial of the
record automatically inflates the column back to the set hash with the
IO::File handle and filename.

=head1 DESCRIPTION

InflateColumn::File

=head1 METHODS

=head2 _file_column_callback ($file,$ret,$target)

method made to be overridden for callback purposes.

=cut

sub _file_column_callback {
    my ($self,$file,$ret,$target) = @_;
}

=head1 AUTHOR

Victor Igumnov

=head1 LICENSE

This library is free software, you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut

1;
