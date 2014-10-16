#!/usr/bin/perl -w
use HTTP::Proxy;
use HTTP::Proxy::HeaderFilter::simple;
use HTTP::Proxy::BodyFilter::simple;
use HTTP::Proxy::BodyFilter::complete;
use MIME::Base64;
use Fcntl ':flock';
use strict;

# the proxy
my $proxy = HTTP::Proxy->new( @ARGV );

# the status page:
# - auto-refresh (quickly at first, then more slowly)
# - count the number of games and modify the title
my $seen_title;
$proxy->push_filter(
    host     => 'www.dragongoserver.net',
    path     => '^/status.php',
    # auto-refresh
    response => HTTP::Proxy::HeaderFilter::simple->new(
        sub {
            my ( $self, $headers, $response ) = @_;
            ($response->request->uri->query || '') =~ /reload=(\d+)/;
            my $n = ($1 || 0) + 1;
            my $delay = $n < 5 ? 30 : $n < 15 ? 60 : $n < 25 ? 300 : 3600;
            $headers->push_header( Refresh => "$delay;url="
                  . $response->request->uri->path
                  . "?reload=$n" );
        }
    ),
    # count games
    response => HTTP::Proxy::BodyFilter::complete->new(),
    response => HTTP::Proxy::BodyFilter::simple->new(
        filter => sub {
             my ( $self, $dataref, $message, $protocol, $buffer ) = @_;
             next if ! $$dataref;

             # count the games and change the title
             my $n = 0; $n++ while $$dataref =~ /game\.php\?gid=\d+/g;
             my $s = $n > 1 ? "s" : ""; $n ||= "No";
             $$dataref =~ s!<TITLE>.*?</TITLE>!<TITLE>$n go game$s pending</TITLE>!s;
        },
    ),
);

# the game page:
# - remove the Message: textarea
# - add a link to make it appear when needed
$proxy->push_filter(
    host     => 'www.dragongoserver.net',
    path     => '^/game.php',
    response => HTTP::Proxy::BodyFilter::complete->new(),
    response => HTTP::Proxy::BodyFilter::simple->new(
      sub { 
          my $msg = '&msg=yes';
          my $uri = $_[2]->request->uri;
          if( $uri =~ s/$msg//o ) { $msg = ''; }
          else { ${$_[1]} =~ s|(</?textarea.*>)|<!-- $1 -->|; }
          ${$_[1]} =~ s|(Message:)|<a href="$uri$msg">$1</a>|;
      }
    )
);

$proxy->start;

