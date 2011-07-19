package Module::Build::WikiDoc;
use strict;
use base qw/Module::Build/;
use File::Spec;

sub ACTION_wikidoc {
    my $self = shift;
    eval "use Pod::WikiDoc";
    if ( $@ eq '' ) {
        my $parser = Pod::WikiDoc->new({ 
            comment_blocks => 1,
            keywords => { VERSION => $self->dist_version },
        });
        for my $src ( keys %{ $self->find_pm_files() } ) {
            (my $tgt = $src) =~ s{\.pm$}{.pod};
            $parser->filter( {
                input   => $src,
                output  => $tgt,
            });
            print "Creating $tgt\n";
            $tgt =~ s{\\}{/}g;
            $self->_add_to_manifest( 'MANIFEST', $tgt );
        }
    }
    else {
        warn "Pod::WikiDoc not available. Skipping wikidoc.\n";
    }
}

sub ACTION_test {
    my $self = shift;
    my $missing_pod;
    for my $src ( keys %{ $self->find_pm_files() } ) {
        (my $tgt = $src) =~ s{\.pm$}{.pod};
        $missing_pod = 1 if ! -e $tgt;
    }
    if ( $missing_pod ) {
        $self->depends_on('wikidoc');
        $self->depends_on('build');
    }
    $self->SUPER::ACTION_test;
}

sub ACTION_testpod {
    my $self = shift;
    $self->depends_on('wikidoc');
    $self->SUPER::ACTION_testpod;
}

sub ACTION_distmeta {
    my $self = shift;
    $self->depends_on('wikidoc');
    $self->SUPER::ACTION_distmeta;
}

1;
