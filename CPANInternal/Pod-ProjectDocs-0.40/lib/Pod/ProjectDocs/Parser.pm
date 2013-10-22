package Pod::ProjectDocs::Parser;
use strict;
use warnings;
use base qw/Pod::Parser Class::Accessor::Fast Class::Data::Inheritable/;
use Pod::ParseUtils;
use URI::Escape;
use File::Spec;
use File::Basename;
use Pod::ProjectDocs::Template;


our $METHOD_REGEXP ||= qr/^(\w+).*/;

BEGIN {
    our $HIGHLIGHTER;
    eval {
        require Syntax::Highlight::Universal;
        $HIGHLIGHTER = Syntax::Highlight::Universal->new;
    };
    *highlighten = $HIGHLIGHTER ? sub {
        my ($self, $type, $str) = @_;
        $HIGHLIGHTER->highlight($type, $str);
    } : sub { return $_[2] };
}

# most of code is borrowed from Pod::Xhtml
__PACKAGE__->mk_accessors(qw/components local_modules current_files_output_path/);
__PACKAGE__->mk_classdata($_) for qw/COMMANDS SEQ language/;

__PACKAGE__->COMMANDS( {
    map { $_ => 1 } qw/pod head1 head2 head3 head4 item over back for begin end/
} );

__PACKAGE__->SEQ( {
    B => \&seqB,
    C => \&seqC,
    E => \&seqE,
    F => \&seqF,
    I => \&seqI,
    L => \&seqL,
    S => \&seqS,
    X => \&seqX,
    Z => \&seqZ,
} );

########## New PUBLIC methods for this class
sub asString { my $self = shift; return $self->{buffer}; }
sub asStringRef { my $self = shift; return \$self->{buffer}; }
sub addHeadText { my $self = shift; $self->{HeadText} .= shift; }
sub addBodyOpenText { my $self = shift; $self->{BodyOpenText} .= shift; }
sub addBodyCloseText { my $self = shift; $self->{BodyCloseText} .= shift; }

########## Override methods in Pod::Parser
########## PUBLIC INTERFACE
sub parse_from_file {
    my $self = shift;
    $self->resetMe;
    $self->SUPER::parse_from_file(@_);
}

sub parse_from_filehandle {
    my $self = shift;
    $self->resetMe;
    $self->SUPER::parse_from_filehandle(@_);
}

########## INTERNALS
sub initialize {
    my $self = shift;

    $self->{TopLinks} = qq(<p><a href="#<<<G?TOP>>>" class="toplink">Top</a></p>) unless defined $self->{TopLinks};
    $self->{MakeIndex} = 1 unless defined $self->{MakeIndex};
    $self->{MakeMeta} = 1 unless defined $self->{MakeMeta};
    $self->{FragmentOnly} = 0 unless defined $self->{FragmentOnly};
    $self->{HeadText} = $self->{BodyOpenText} = $self->{BodyCloseText} = '';
    $self->{LinkParser} ||= new Pod::Hyperlink;
    $self->{IsFirstCommand} = 1;
    $self->{FirstAnchor} = "TOP";
    $self->SUPER::initialize();
}

sub command {
    my ($parser, $command, $paragraph, $line_num, $pod_para) = @_;
    my $ptree = $parser->parse_text( $paragraph, $line_num );
    $pod_para->parse_tree( $ptree );
    $parser->parse_tree->append( $pod_para );
}

sub verbatim {
    my ($parser, $paragraph, $line_num, $pod_para) = @_;
    $parser->parse_tree->append( $pod_para );
}

sub textblock {
    my ($parser, $paragraph, $line_num, $pod_para) = @_;
    my $ptree = $parser->parse_text( $paragraph, $line_num );
    $pod_para->parse_tree( $ptree );
    $parser->parse_tree->append( $pod_para );
}

sub end_pod {
    my $self = shift;
    my $ptree = $self->parse_tree;

    # clean up tree ready for parse
    foreach my $para (@$ptree) {
        if ($para->{'-prefix'} eq '=') {
            $para->{'TYPE'} = 'COMMAND';
        } elsif (! @{$para->{'-ptree'}}) {
            $para->{'-ptree'}->[0] = $para->{'-text'};
            $para->{'TYPE'} = 'VERBATIM';
        } else {
            $para->{'TYPE'} = 'TEXT';
        }
        foreach (@{$para->{'-ptree'}}) {
            unless (ref $_) { s/\n\s+$//; }
        }
    }

    # now loop over each para and expand any html escapes or sequences
    $self->_paraExpand( $_ ) foreach (@$ptree);

    $self->{buffer} =~ s/(\n?)<\/pre>\s*<pre>/$1/sg; # concatenate 'pre' blocks
    1 while $self->{buffer} =~ s/<pre>(\s+)<\/pre>/$1/sg;
    $self->{buffer} = $self->_makeIndex . $self->{buffer} if $self->{MakeIndex};
    $self->{buffer} =~ s/<<<G\?TOP>>>/$self->{FirstAnchor}/ge;
    $self->{buffer} = join "\n", qq[<div class="pod">], $self->{buffer}, "</div>";

    # Expand internal L<> links to the correct sections
    $self->{buffer} =~ s/#<<<(.*?)>>>/'#' . $self->_findSection($1)/eg;
    die "gotcha" if $self->{buffer} =~ /#<<</;

    my $headblock = sprintf "%s\n%s\n\t<title>%s</title>\n",
        qq(<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">),
        qq(<html xmlns="http://www.w3.org/1999/xhtml" xml:lang=").$self->{Lang}.qq(">\n<head>),
        _htmlEscape( $self->{doctitle} );
    $headblock .= $self->_makeMeta if $self->{MakeMeta};

    unless ($self->{FragmentOnly}) {
        $self->{buffer} = $headblock . $self->{HeadText} . "</head>\n<body>\n" . $self->{BodyOpenText} . $self->{buffer};
        $self->{buffer} .= $self->{BodyCloseText} . "</body>\n</html>\n";
    }

    # in stringmode we only accumulate the XHTML else we print it to the
    # filehandle
    unless ($self->{StringMode}) {
        my $out_fh = $self->output_handle;
        print $out_fh $self->{buffer};
    }
}

########## Everything else is PRIVATE
sub resetMe {
    my $self = shift;
    $self->{'-ptree'} = new Pod::ParseTree;
    $self->{'sections'} = [];
    $self->{'listKind'} = [];
    $self->{'listHasItems'} = [];
    $self->{'dataSections'} = [];
    $self->{'section_names'} = {};
    $self->{'section_ids'} = {};

    foreach (qw(inList titleflag )) { $self->{$_} = 0; }
    foreach (qw(buffer doctitle)) { $self->{$_} = ''; }
}

sub parse_tree { return $_[0]->{'-ptree'}; }

sub _paraExpand {
    my $self = shift;
    my $para = shift;

    # collapse interior sequences and strings
    foreach ( @{$para->{'-ptree'}} ) {
        $_ = (ref $_) ? $self->_handleSequence($_) : _htmlEscape( $_ );
    }

    # the parse tree has now been collapsed into a list of strings
    if ($para->{TYPE} eq 'TEXT') {
        return if @{$self->{dataSections}};
        $self->_addTextblock( join('', @{$para->{'-ptree'}}) );
    } elsif ($para->{TYPE} eq 'VERBATIM') {
        return if @{$self->{dataSections}};
        my $paragraph = "<pre>" . join('', @{$para->{'-ptree'}}) . "\n\n</pre>";
        my $parent_list = $self->{listKind}[-1];
        if ($parent_list && $parent_list == 2) {
            $paragraph = "<dd>$paragraph</dd>";
        }
        $self->{buffer} .= $paragraph;
        if ($self->{titleflag} != 0) {
            $self->_setTitle( $paragraph );
            warn "NAME followed by verbatim paragraph";
        }
    } elsif ($para->{TYPE} eq 'COMMAND') {
        $self->_addCommand($para->{'-name'}, join('', @{$para->{'-ptree'}}), $para->{'-text'}, $para->{'-line'} )
    } else {
        warn "Unrecognized paragraph type $para->{TYPE} found at $self->{_INFILE} line $para->{'-line'}\n";
    }
}

sub _addCommand {
    my $self = shift;
    my ($command, $paragraph, $raw_para, $line) = @_;
    my $anchor;

    unless (exists $self->COMMANDS->{$command}) {
        warn "Unrecognized command '$command' skipped at $self->{_INFILE} line $line\n";
        return;
    }

    for ($command) {
        /^head1/ && do {
            $anchor = $self->_addSection( 'head1', $paragraph );
            $self->{buffer} .= qq(<h1 id="$anchor">$paragraph )
                    .($self->{TopLinks} ? $self->{TopLinks} : '').qq(</h1>)."\n\n";
            if ($anchor eq 'NAME') { $self->{titleflag} = 1; }
            last;
        };
        /^head([234])/ && do {
            my $head_level = $1;
            $anchor = $self->_addSection( "head${head_level}", $paragraph );
            $self->{buffer} .= qq(<h${head_level} id="$anchor">$paragraph</h${head_level}>\n\n);
            (my $method = $paragraph) =~ s#$METHOD_REGEXP#$1#;
            if ( exists $self->{_source_code}{$method} ) {
                $self->{buffer} .= qq{<p><a href="#" onclick="toggleCode('method_$method');return false;">[Source]</a></p>
                                        <div class="method-source-code" id="method_$method">
                                        <pre>\n\n};
                $self->{buffer} .= $self->{_source_code}{$method};
                $self->{buffer} .= qq{</pre></div>\n\n};
            }
            last;
        };
        /^item/ && do {
            unless ($self->{inList}) {
                warn "Not in list at $self->{_INFILE} line $line\n";
                last;
            }

            $self->{listHasItems}[-1] = 1;
            $self->{listCurrentParas}[-1] = 0;

            # is this the first item in the list?
            if (@{$self->{listKind}} && $self->{listKind}[-1] == 0) {
                my $parent_list = $self->{listKind}[-2]; # this is a sub-list
                if ($parent_list && $parent_list == 1) {
                    # <ul> sub lists must be in an <li> [BEGIN]
                    $self->{buffer} .= "<li>";
                } elsif ($parent_list && $parent_list == 2) {
                    # <dl> sub lists must be in a <dd> [BEGIN]
                    $self->{buffer} .= "<dd>";
                }

                if ($paragraph eq '*') {
                    $self->{listKind}[-1] = 1;
                    $self->{buffer} .= "<ul>\n";
                } else {
                    $self->{listKind}[-1] = 2;
                    $self->{buffer} .= "<dl>\n";
                }
            } else {
                # close last list item's tag#
                if ($self->{listKind}[-1] == 1) {
                    $self->{buffer} .= "</li>\n";
                }
            }
            if (@{$self->{listKind}} && $self->{listKind}[-1] == 2) {
                $self->{buffer} .= qq(\t<dt);
                if ($self->{MakeIndex} >= 2) {
                    $anchor = $self->_addSection( 'list', $paragraph );
                    $self->{buffer} .= qq( id="$anchor");
                }
                $self->{buffer} .= ">";
                $self->{buffer} .= qq($paragraph</dt>\n);
            }
            last;
        };
        /^over/ && do {
            $self->{inList}++;
            push @{$self->{listKind}}, 0;
            push @{$self->{listHasItems}}, 0;
            push @{$self->{sections}}, 'OVER';
            push @{$self->{listCurrentParas}}, 0;
        };
        /^back/ && do {
            if (--$self->{inList} < 0) {
                warn "=back commands don't balance =overs at $self->{_INFILE} line $line\n";
                last;
            } elsif ($self->{listHasItems} == 0) {
                warn "empty list at $self->{_INFILE} line $line\n";
                last;
            } elsif (@{$self->{listKind}} && $self->{listKind}[-1] == 1) {
                $self->{buffer} .= "</li>\n</ul>\n\n";
            } else {
                $self->{buffer} .= "</dl>\n";
            }

            my $parent_list = $self->{listKind}[-2]; # this is a sub-list
            if ($parent_list && $parent_list == 1) {
                # <ul> sub lists must be in an <li> [END]
                $self->{buffer} .= "</li>\n";
            }
            if ($parent_list && $parent_list == 2) {
                # <dl> sub lists must be in a <dd> [END]
                $self->{buffer} .= "</dd>\n";
            }

            if ($self->{sections}[-1] eq 'OVER')
            {
                pop @{$self->{sections}};
            } else {
                push @{$self->{sections}}, 'BACK';
            }
            pop  @{$self->{listHasItems}};
            pop  @{$self->{listKind}};
            pop  @{$self->{listCurrentParas}};
            last;
        };
        /^for/ && do {
            my ($html) = $raw_para =~ /^\s*(?:pod2)?x?html\s+(.*)/;
            $self->{buffer} .= $html if $html;
        };
        /^begin/ && do {
            my ($ident) = $paragraph =~ /(\S+)/;
            push @{$self->{dataSections}}, $ident;
            last;
        };
        /^end/ && do {
            my ($ident) = $paragraph =~ /(\S+)/;
            unless (@{$self->{dataSections}}) {
                warn "no corresponding '=begin $ident' marker at $self->{_INFILE} line $line\n";
                last;
            }
            my $current_section = $self->{dataSections}[-1];
            unless ($current_section eq $ident) {
                warn "'=end $ident' doesn't match '=begin $current_section' at $self->{_INFILE} line $line\n";
                last;
            }
            pop @{$self->{dataSections}};
            last;
        };
    }
    if ($anchor && $self->{IsFirstCommand})
    {
        $self->{FirstAnchor} = $anchor;
        $self->{IsFirstCommand} = 0;
    }
}

sub _addTextblock {
    my $self = shift;
    my $paragraph = shift;

    if ($self->{titleflag} != 0) { $self->_setTitle( $paragraph ); }

    if (! @{$self->{listKind}} || $self->{listKind}[-1] == 0) {
        $self->{buffer} .= "<p>$paragraph</p>\n\n";
    } elsif (@{$self->{listKind}} && $self->{listKind}[-1] == 1) {
        if ($self->{listCurrentParas}[-1]++ == 0) {
            $self->{buffer} .= "\t<li>$paragraph";
        } else {
            $self->{buffer} .= "\n<br /><br />$paragraph";
        }
    } else {
        $self->{buffer} .= "\t\t<dd><p>$paragraph</p></dd>\n";
    }
}

# expand interior sequences recursively, bottom up
sub _handleSequence {
    my $self = shift;
    my $seq = shift;
    my $buffer = '';

    foreach (@{$seq->{'-ptree'}}) {
        if (ref $_) {
            $buffer .= $self->_handleSequence($_);
        } else {
            $buffer .= _htmlEscape($_);
        }
    }

    unless (exists $self->SEQ->{$seq->{'-name'}}) {
        warn "Unrecognized special sequence '$seq->{'-name'}' skipped at $self->{_INFILE} line $seq->{'-line'}\n";
        return $buffer;
    }
    return $self->SEQ->{$seq->{'-name'}}->($self, $buffer);
}

sub _makeIndexId {
    my $arg = shift;

    $arg =~ s/\W+/_/g;
    $arg =~ s/^_+|_+$//g;
    $arg =~ s/__+/_/g;
    $arg = substr($arg, 0, 36);
    return $arg;
}

sub _addSection {
    my $self = shift;
    my ($type, $htmlarg) = @_;
    return unless defined $htmlarg;

    my $index_id;
    if ($self->{section_names}{$htmlarg}) {
        $index_id = $self->{section_names}{$htmlarg};
    } else {
        $index_id = _makeIndexId($htmlarg);
        if ($self->{section_ids}{$index_id}) {
            $index_id .= "-" . ++$self->{section_ids}{$index_id};
        } else {
            $self->{section_ids}{$index_id}++;
        }
        $self->{section_names}{$htmlarg} = $index_id;
    }

    push( @{$self->{sections}}, [$type, $index_id, $htmlarg]);
    return $index_id;
}

sub _findSection {
    my $self = shift;
    my ($htmlarg) = @_;

    my $index_id;
    if ($index_id = $self->{section_names}{$htmlarg}) {
        return $index_id;
    } else {
        return _makeIndexId($htmlarg);
    }
}

sub _get_elem_level {
    my $elem = shift;
    if (ref($elem))
    {
        my $type = $elem->[0];
                if ($type =~ /^head(\d+)$/)
                {
                    return $1;
                }
                else
                {
                        return 0;
                }
        }
        else
        {
         return 0;
        }
}

sub _makeIndex {
    my $self = shift;

    $self->{FirstAnchor} = "TOP";
    my $string = "<!-- INDEX START -->\n<h3 id=\"TOP\">Index</h3>\n<ul>\n";
    $self->{FirstAnchor} = "TOP";
    my $i = 0;
    my $previous_level = 0;
    for (my $i=0;$i< @{$self->{sections}} ; $i++)
    {
        local $_ = $self->{sections}->[$i];
        my $next = ($self->{'sections'}->[$i+1] || "");
        if (ref $_) {
            my ($type, $href, $name) = @$_;
            my $index_link = "";
            my $next_level = _get_elem_level($next);
            my $this_level = _get_elem_level($_) || $previous_level;
            if ($this_level < $previous_level)
            {
                $index_link .=
                    ("</ul>\n</li>\n" x ($previous_level - $this_level));
            }

            $index_link .= qq(\t<li><a href="#${href}">${name}</a>);

            if ($next eq "OVER")
            {
                $index_link .= "<br />\n";
            }
            elsif ($next_level > $this_level)
            {
                $index_link .= "<br />\n";
                $index_link .=
                    ("<ul>\n<li>\n" x ($next_level - $this_level - 1)) .
                        "<ul>\n";
            }
            else
            {
                $index_link .= "</li>\n";
            }
            # $index_link = qq(<ul>$index_link</ul>) unless ($type eq 'head1');
            $string .= $index_link;
        } elsif ($_ eq 'OVER') {
            $string .= qq(\t<ul>\n);
        } elsif ($_ eq 'BACK') {
            $string .= qq(\t</ul>\n</li>\n);
        }
        $previous_level = _get_elem_level($_) || $previous_level;
    }
    $string .=
        ("</ul>\n</li>\n" x ($previous_level-1)) . "</ul>\n";

    $string .= "<hr />\n<!-- INDEX END -->\n\n";
    return $string;
}

sub _makeMeta {
    my $self = shift;
    return
        qq(\t<meta name="description" content="Pod documentation for ) . _htmlEscape( $self->{doctitle} ) . qq(" />\n)
        . qq(\t<meta name="inputfile" content=") . _htmlEscape( $self->input_file ) . qq(" />\n)
        . qq(\t<meta name="outputfile" content=") . _htmlEscape( $self->output_file ) . qq(" />\n)
        . qq(\t<meta name="created" content=") . _htmlEscape( scalar(localtime) ) . qq(" />\n);
}

sub _setTitle {
    my $self = shift;
    my $paragraph = shift;

    if ($paragraph =~ m/^(.+?) - /) {
        $self->{doctitle} = $1;
    } elsif ($paragraph =~ m/^(.+?): /) {
        $self->{doctitle} = $1;
    } elsif ($paragraph =~ m/^(.+?)\.pm/) {
        $self->{doctitle} = $1;
    } else {
        $self->{doctitle} = substr($paragraph, 0, 80);
    }
    $self->{titleflag} = 0;
}

sub _htmlEscape {
    my $txt = shift;
    $txt =~ s/&(?!(amp|lt|gt|quot);)/&amp;/g;
    $txt =~ s/</&lt;/g;
    $txt =~ s/>/&gt;/g;
    $txt =~ s/\"/&quot;/g;
    return $txt;
}

########## Sequence handlers
sub seqI { return '<i>' . $_[1] . '</i>'; }
sub seqB { return '<strong>' . $_[1] . '</strong>'; }
sub seqC { return '<code>' . $_[1] . '</code>'; }
sub seqF { return '<cite>' . $_[1] . '</cite>'; }
sub seqZ { return ''; }

sub seqL {
    my ($self, $link) = @_;
    $self->{LinkParser}->parse( $link );

    my $kind = $self->{LinkParser}->type;
    my $string = '';

    if ($kind eq 'hyperlink') {    #easy, a hyperlink
        my $targ = _htmlEscape( $self->{LinkParser}->node );
        my $text = _htmlEscape( $self->{LinkParser}->text );
        $string = qq(<a href="$targ">$text</a>);
    } elsif ($self->{LinkParser}->page eq '') {    # a link to this page
        # Post-process these links so we can things up to the correct sections
        my $targ = $self->{LinkParser}->node;
        my $text = _htmlEscape( $self->{LinkParser}->text );
        $string = qq(<a href="#<<<$targ>>>">$text</a>);
    } elsif ($kind eq 'item') {    # link to the other page
        my $targ = $self->_resolvePage($self->{LinkParser}->page);
        my $node = $self->{LinkParser}->node;
        my $text = _htmlEscape( $self->{LinkParser}->text );
        $string = qq(<a href="$targ#$node">$text</a>);
    } else {
        my $targ = $self->_resolvePage($self->{LinkParser}->page);
        my $text = _htmlEscape( $self->{LinkParser}->text );
        $string = qq(<a href="$targ">$text</a>);
    }
    return $string;
}

sub _resolvePage {
    my ($self, $page) = @_;
    my $modules = $self->local_modules->{ $self->language } || [];
    foreach my $module ( @$modules ) {
        if ( $module->{name} eq $page ) {
            my $targ = $self->_resolveRelPath( $module->{path} );
            return $targ;
        }
    }
    return $self->_makeLinkToCommunity($page);
}

sub _makeLinkToCommunity { "abstract method" }

sub _resolveRelPath {
    my ($self, $path ) = @_;
    my $curpath = $self->current_files_output_path;
    my ($name, $dir) = File::Basename::fileparse $curpath, qr/\.html/;
    return File::Spec->abs2rel($path, $dir);
}

sub seqS {
    my $text = $_[1];
    $text =~ s/\s/&nbsp;/g;
    return $text;
}

sub seqX {
    my $self = shift;
    my $arg = shift;
    my $anchor = $self->_addSection( 'head1', $arg );
    return qq[<span id="$anchor">$arg</span>];
}

sub seqE {
    my $self = shift;
    my $arg = shift;
    my $rv;

    if ($arg eq 'sol') {
        $rv = '/';
    } elsif ($arg eq 'verbar') {
        $rv = '|';
    } elsif ($arg =~ /^\d$/) {
        $rv = "&#$arg;";
    } elsif ($arg =~ /^0?x(\d+)$/) {
        $rv = $1;
    } else {
        $rv = "&$arg;";
    }
    return $rv;
}

sub gen_html {
    my($self, %args) = @_;
    my $doc        = $args{doc};
    my $components = $args{components};
    my $mgr_desc   = $args{desc};
    open(FILE, $doc->origin) or warn $!;
    while(<FILE>) {
        next unless /^\s*sub\s+(\w+)/;
        my $method = $1;
        my $sub = $_;
        while(<FILE>){
            $sub .= $_;
            last if /^}/;
        }
        my $result = $self->highlighten("perl", $sub);
        $self->{_source_code}{$method} = $result;
    }
    close(FILE);
    $self->current_files_output_path( $doc->get_output_path );
    $self->_prepare($doc, $components, $mgr_desc);
#   local $SIG{__WARN__} = sub { };
    $self->parse_from_file($doc->origin);
    my $title = $self->_get_title;
    $doc->title($title);
    $self->current_files_output_path('');
    return $self->asString;
}

sub _prepare {
    my($self, $doc, $components, $mgr_desc) = @_;
    my $charset = $doc->config->charset || 'UTF-8';
    $self->{StringMode} = 1;
    $self->{MakeMeta}   = 0;
    $self->{TopLinks}   = $components->{arrow}->tag($doc);
    $self->{MakeIndex}  = $doc->config->index;
    $self->{Lang}       = $doc->config->lang;
    $self->initialize();
    $self->addHeadText($components->{css}->tag($doc));
    $self->addHeadText(qq|<meta http-equiv="Content-Type" content="text/html; charset=$charset" />\n|);
    $self->addHeadText(q|  <script type="text/javascript">

  function toggleCode( id ) {
    if ( document.getElementById )
      elem = document.getElementById( id );
    else if ( document.all )
      elem = eval( "document.all." + id );
    else
      return false;

    elemStyle = elem.style;

    if ( elemStyle.display != "block" ) {
      elemStyle.display = "block"
    } else {
      elemStyle.display = "none"
    }

    return true;
  }
  document.writeln( "<style type=\"text/css\">div.method-source-code { display: none }</style>" )
  </script>|);
    $self->addBodyOpenText($self->_get_data($doc, $mgr_desc));
    $self->addBodyCloseText(
        qq|<div class="footer">generated by <a href="http://search.cpan.org/perldoc?|
        .URI::Escape::uri_escape("Pod::ProjectDocs")
        .qq|">Pod::ProjectDocs</a></div>|
    );
}

sub _get_title {
    my $self = shift;
    my $name_node = 0;
    my $title = '';
    foreach my $node ( @{ $self->parse_tree } ) {
        if ($node->{'-ptree'}[0] && $node->{'-ptree'}[0] eq 'NAME') {
            $name_node = 1; next;
        }
        if($name_node == 1){
            $title = join "", @{ $node->{'-ptree'} };
            last;
        }
    }
    $title =~ s/^\s*\S*\s*-\s(.*)$/$1/;
    return $title;
}

sub _get_data {
    my($self, $doc, $mgr_desc) = @_;
    my $tt = Pod::ProjectDocs::Template->new;
    my $text = $tt->process($doc, $doc->data, {
        title    => $doc->config->title,
        desc     => $doc->config->desc,
        name     => $doc->name,
        outroot  => $doc->config->outroot,
        src      => $doc->get_output_src_path,
        mgr_desc => $mgr_desc,
    });
    return $text if $^O ne 'MSWin32';

    while ( $text =~ s|href="(.*?)\\(.*?)"|href="$1/$2"| ) {
        next;
    }
    return $text;
}


1;
__END__
