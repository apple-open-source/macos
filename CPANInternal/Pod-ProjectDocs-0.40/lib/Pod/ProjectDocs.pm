package Pod::ProjectDocs;
use strict;
use warnings;

use base qw/Class::Accessor::Fast/;

use File::Spec;
use JSON;
use Pod::ProjectDocs::DocManager;
use Pod::ProjectDocs::Config;
use Pod::ProjectDocs::Parser::PerlPod;
use Pod::ProjectDocs::Parser::JavaScriptPod;
use Pod::ProjectDocs::CSS;
use Pod::ProjectDocs::ArrowImage;
use Pod::ProjectDocs::IndexPage;

__PACKAGE__->mk_accessors(qw/managers components config/);

our $VERSION = '0.40';

sub new {
    my $class = shift;
    my $self  = bless { }, $class;
    $self->_init(@_);
    return $self;
}

sub _init {
    my($self, %args) = @_;

    # set absolute path to 'outroot'
    $args{outroot} ||= File::Spec->curdir;
    $args{outroot} = File::Spec->rel2abs($args{outroot}, File::Spec->curdir)
        unless File::Spec->file_name_is_absolute( $args{outroot} );

    # set absolute path to 'libroot'
    $args{libroot} ||= File::Spec->curdir;
    $args{libroot} = [ $args{libroot} ] unless ref $args{libroot};
    $args{libroot} = [ map {
        File::Spec->file_name_is_absolute($_) ? $_
        : File::Spec->rel2abs($_, File::Spec->curdir)
    } @{ $args{libroot} } ];

    # check mtime by default, but can be overridden
    $args{forcegen} ||= 0;

    $args{except} ||= [];
    $args{except} = [ $args{except} ] unless ref $args{except};

    $self->config( Pod::ProjectDocs::Config->new(%args) );

    $self->_setup_components();
    $self->_setup_managers();
}

sub _setup_components {
    my $self = shift;
    $self->components( {} );
    $self->components->{css}
        = Pod::ProjectDocs::CSS->new( config => $self->config );
    $self->components->{arrow}
        = Pod::ProjectDocs::ArrowImage->new( config => $self->config );
}

sub _setup_managers {
    my $self = shift;
    $self->reset_managers();
    $self->add_manager('Perl Manuals', 'pod', Pod::ProjectDocs::Parser::PerlPod->new);
    $self->add_manager('Perl Modules', 'pm',  Pod::ProjectDocs::Parser::PerlPod->new);
    $self->add_manager('Trigger Scripts', ['cgi', 'pl'], Pod::ProjectDocs::Parser::PerlPod->new);
    $self->add_manager('JavaScript Libraries', 'js', Pod::ProjectDocs::Parser::JavaScriptPod->new);
}

sub reset_managers {
    my $self = shift;
    $self->managers( [] );
}

sub add_manager {
    my($self, $desc, $suffix, $parser) = @_;
    push @{ $self->managers },
        Pod::ProjectDocs::DocManager->new(
            config => $self->config,
            desc   => $desc,
            suffix => $suffix,
            parser => $parser,
        );
}

sub gen {
    my $self = shift;

    foreach my $comp_key ( keys %{ $self->components } ) {
        my $comp = $self->components->{$comp_key};
        $comp->publish();
    }

    my @perl_modules;
    my @js_libraries;
    foreach my $manager ( @{ $self->managers } ) {
        next if $manager->desc !~ /(Perl Modules|JavaScript Libraries)/;
        my $ite = $manager->doc_iterator();
        while ( my $doc = $ite->next ) {
            my $name = $doc->name;
            my $path = $doc->get_output_path;
            if ($manager->desc eq 'Perl Modules') {
                $name =~ s/\-/\:\:/g;
                push @perl_modules, { name => $name, path => $path };
            }
            elsif ($manager->desc eq 'JavaScript Libraries') {
                $name =~ s/\-/\./g;
                push @js_libraries, { name => $name, path => $path };
            }
        }
    }

    foreach my $manager ( @{ $self->managers } ) {

        $manager->parser->local_modules( {
            perl       => \@perl_modules,
            javascript => \@js_libraries,
        } );

        my $ite = $manager->doc_iterator();
        while ( my $doc = $ite->next ) {
            my $html = $manager->parser->gen_html(
                doc        => $doc,
                desc       => $manager->desc,
                components => $self->components,
            );
            if ( $self->config->forcegen || $doc->is_modified ) {
                $doc->copy_src();
                $doc->publish($html);
            }
        }
    }

    my $index_page = Pod::ProjectDocs::IndexPage->new(
        config     => $self->config,
        components => $self->components,
        json       => $self->get_managers_json,
    );
    $index_page->publish();
}

sub get_managers_json {
    my $self    = shift;
    my $js      = JSON->new;
    my $records = [];
    foreach my $manager ( @{ $self->managers } ) {
        my $record = {
            desc    => $manager->desc,
            records => [],
        };
        foreach my $doc ( @{ $manager->docs } ) {
            push @{ $record->{records} }, {
                path  => $doc->relpath,
                name  => $doc->name,
                title => $doc->title,
            };
        }
        if ( scalar( @{ $record->{records} } ) > 0 ) {
            push @$records, $record;
        }
    }
    return $js->encode($records);
}

sub _croak {
    my($self, $msg) = @_;
    require Carp;
    Carp::croak($msg);
}

1;
__END__

=head1 NAME

Pod::ProjectDocs - generates CPAN like pod pages

=head1 SYNOPSIS

    #!/usr/bin/perl -w
    use strict;
    use Pod::ProjectDocs;
    my $pd = Pod::ProjectDocs->new(
        outroot => '/output/directory',
        libroot => '/your/project/lib/root',
        title   => 'ProjectName',
    );
    $pd->gen();

    #or use pod2projdocs on your shell
    pod2projdocs -out /output/directory -lib /your/project/lib/root

=head1 DESCRIPTION

This module allows you to generates CPAN like pod pages from your modules (not only perl but also javascript including pod)
for your projects. Set your library modules' root directory with libroot option.
And you have to set output directory's path with outroot option.
And this module searches your pm and pod files from your libroot, and generates
html files, and an index page lists up all your modules there.

See the generated pages via HTTP with your browser.
Your documents of your modules are displayed like CPAN website.

=head1 OPTIONS

=over 4

=item outroot

directory where you want to put the generated docs into.

=item libroot

your library's root directory

You can set single path by string, or multiple by arrayref.

    my $pd = Pod::ProjectDocs->new(
        outroot => '/path/to/output/directory',
        libroot => '/path/to/lib'
    );

or

    my $pd = Pod::ProjectDocs->new(
        outroot => '/path/to/output/directory',
        libroot => ['/path/to/lib1', '/path/to/lib2'],
    );

=item title

your project's name.

=item desc

description for your project.

=item charset

This is used in meta tag. default 'UTF-8'

=item index

whether you want to create index on each pod pages or not.
set 1 or 0.

=item lang

what language is set for xml:lang

=item forcegen

whether you want to generate HTML document even if source files are not updated. default is 0.

=item except

if you set this parameter as regex, the file matches this regex won't be checked.

  Pod::ProjectDocs->new(
    except => qr/^specific_dir\//,
    ...other parameters
  );

  Pod::ProjectDocs->new(
    except => [qr/^specific_dir1\//, qr/^specific_dir2\//],
    ...other parameters
  );

=back

=head1 pod2projdocs

You need not to write script with this module,
I put the script named 'pod2projdocs' in this package.
At first, please execute follows.

    pod2projdocs -help

or

    pod2projdocs -?

=head1 SEE ALSO

L<Pod::Parser>

=head1 AUTHOR

Lyo Kato E<lt>lyo.kato@gmail.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright(C) 2005 by Lyo Kato

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.5 or,
at your option, any later version of Perl 5 you may have available.

=cut
