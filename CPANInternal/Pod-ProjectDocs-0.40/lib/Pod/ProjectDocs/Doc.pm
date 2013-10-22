package Pod::ProjectDocs::Doc;
use strict;
use warnings;
use base qw/Pod::ProjectDocs::File/;
use File::Basename;
use File::Spec;
use File::Copy;

__PACKAGE__->mk_accessors(qw/origin suffix origin_root title/);
__PACKAGE__->data( do{ local $/; <DATA> } );

sub _init {
    my($self, %args) = @_;
    $self->SUPER::_init(%args);
    $self->origin( $args{origin} );
    $self->origin_root( $args{origin_root} );
    $self->suffix( $args{suffix} );
    $self->_set_relpath;
}

sub _set_relpath {
    my $self   = shift;
    my $suffix = $self->suffix;
    my($name, $dir) = fileparse $self->origin, qr/\.$suffix/;
    my $reldir = File::Spec->abs2rel($dir, $self->origin_root);
    $reldir ||= File::Spec->curdir;
    my $outroot = $self->config->outroot;
    $self->_check_dir($reldir, $outroot);
    $self->_check_dir($reldir, File::Spec->catdir($outroot, "src"));
    my $relpath = File::Spec->catdir($reldir, $name);
    $relpath =~ s:\\:/:g if $^O eq 'MSWin32';
    $self->name( join "-", File::Spec->splitdir($relpath) );
    $self->relpath($relpath.".".$suffix.".html");
}

sub _check_dir {
    my($self, $dir, $path) = @_;
    $self->_mkdir($path);
    my @dirs = File::Spec->splitdir($dir);
    foreach my $dir (@dirs) {
        $path = File::Spec->catdir($path, $dir);
        $self->_mkdir($path);
    }
}

sub _mkdir {
    my($self, $path) = @_;
    unless(-e $path && -d _) {
        mkdir($path, 0755)
        or $self->_croak(qq/Can't make directory [$path]./);
    }
}

sub get_output_src_path {
    my $self = shift;
    my $outroot = File::Spec->catdir($self->config->outroot, "src");
    my $relpath = $self->relpath;
    my $suffix  = $self->suffix;
    $relpath =~ s/\.html$//;
    my $path = File::Spec->catfile($outroot, $relpath);
    return $path;
}

sub copy_src {
    my $self   = shift;
    my $origin = $self->origin;
    my $newsrc = $self->get_output_src_path;
    File::Copy::copy($origin, $newsrc);
}

sub is_modified {
    my $self   = shift;
    my $origin = $self->origin;
    my $newsrc = $self->get_output_src_path;
    return 1 unless( -e $newsrc );
    return (-M $origin < -M $newsrc) ? 1 : 0;
}

1;
__DATA__
<div class="box">
  <h1 class="t1">[% title | html %]</h1>
  <table>
    <tr>
      <td class="label">Description</td>
      <td class="cell">[% desc | html | html_line_break %]</td>
    </tr>
  </table>
</div>
<div class="path">
  <a href="[% outroot _ '/index.html' | relpath %]">[% title | html %]</a> &gt; [% mgr_desc | html %] &gt;
  [% name | html %]
</div>
<div>
<a href="[% src | relpath %]">Source</a>
</div>


