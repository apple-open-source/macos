if ($^O eq "MacOS") {
    print "1..0\n";
    exit(0);
}

unless (-f "CAN_TALK_TO_OURSELF") {
    print "1..0 # Skipped: Can't talk to ourself (misconfigured system)\n";
    exit;
}

# Hm, this should really use Test.pm, but not worth changing over, really.


$| = 1; # autoflush

require IO::Socket;  # make sure this work before we try to make a HTTP::Daemon

# First we make ourself a daemon in another process
my $D = shift || '';
if ($D eq 'daemon') {

    require HTTP::Daemon;

    my $d = HTTP::Daemon->new(Timeout => 10);

    print "Please to meet you at: <URL:", $d->url, ">\n";
    open(STDOUT, $^O eq 'VMS'? ">nl: " : ">/dev/null");

    while ($c = $d->accept) {
	$r = $c->get_request;
	if ($r) {
	    my $p = ($r->url->path_segments)[1];
	    my $func = lc("httpd_" . $r->method . "_$p");
	    if (defined &$func) {
		&$func($c, $r);
	    }
	    else {
		$c->send_error(404);
	    }
	}
	$c = undef;  # close connection
    }
    print STDERR "HTTP Server terminated\n";
    exit;
}
else {
    use Config;
    my $perl = $Config{'perlpath'};
    $perl = $^X if $^O eq 'VMS' or -x $^X and $^X =~ m,^([a-z]:)?/,i;
    open(DAEMON, "$perl local/http-get.t daemon |") or die "Can't exec daemon: $!";
}

print "1..20\n";


my $greeting = <DAEMON>;
$greeting =~ /(<[^>]+>)/;

require URI;
my $base = URI->new($1);
sub url {
   my $u = URI->new(@_);
   $u = $u->abs($_[1]) if @_ > 1;
   $u->as_string;
}

print "# Will access HTTP server at $base\n";

require LWP::UserAgent;
require HTTP::Request;
$ua = new LWP::UserAgent;
$ua->agent("Mozilla/0.01 " . $ua->agent);
$ua->from('gisle@aas.no');
$ua->cookie_jar({});

#----------------------------------------------------------------
print "#------------Testing: Bad request...\n";
$res = $ua->get(
  url("/not_found", $base),
    'X-Foo' => "Bar",
);

print "not " unless $res->is_error
                and $res->code == 404
                and $res->message =~ /not\s+found/i;
print "ok 1\n";
# we also expect a few headers
print "not " if !$res->server and !$res->date;
print "ok 2\n";

#----------------------------------------------------------------
print "#------------Testing: Simple echo...\n";
sub httpd_get_echo
{
    my($c, $req) = @_;
    $c->send_basic_header(200);
    print $c "Content-Type: text/plain\015\012";
    $c->send_crlf;
    print $c $req->as_string;
}

$res = $ua->get(
  url("/echo/path_info?query", $base),
    Accept => 'text/html',
    Accept => 'text/plain; q=0.9',
    Accept => 'image/*',
    Long_text => 'This is a very long header line
which is broken between
more than one line.',
    X_Foo => "Bar",
    
);
#print $res->as_string;

print "not " unless $res->is_success
               and  $res->code == 200 && $res->message eq "OK";
print "ok 3\n";

$_ = $res->content;
@accept = /^Accept:\s*(.*)/mg;

#print "$_\n";

print "not " unless /^From:\s*gisle\@aas\.no$/m
                and /^Host:/m
                and @accept == 3
	        and /^Accept:\s*text\/html/m
	        and /^Accept:\s*text\/plain/m
	        and /^Accept:\s*image\/\*/m
                and /^Long-Text:\s*This.*broken between/m
		and /^X-Foo:\s*Bar$/m
		and /^User-Agent:\s*Mozilla\/0.01/m;
print "ok 4\n";

#----------------------------------------------------------------
print "#------------Testing: Send file...\n";

my $file = "test-$$.html";
sub _write_file {
  open(FILE, ">$file") or die "Can't create $file: $!";
  binmode FILE or die "Can't binmode $file: $!";
  print FILE <<EOT;
<html><title>En prøve</title>
<h1>Dette er en testfil</h1>
Jeg vet ikke hvor stor fila behøver å være heller, men dette
er sikkert nok i massevis.
EOT
  close(FILE);
  print "# ", -s $file, " bytes written to $file\n";
  return;
}

sub httpd_get_file
{
    my($c, $r) = @_;
    my %form = $r->url->query_form;
    my $file = $form{'name'};
    $c->send_file_response($file);
}

_write_file();

$res = $ua->get( url("/file?name=$file", $base) );

#print $res->as_string;

print "not " unless $res->is_success
                and $res->content_type eq 'text/html'
		and $res->content_length == 147
		and $res->title eq 'En prøve'
		and $res->content =~ /å være/;
print "ok 5\n";		



{

my $content;

$res = $ua->get( url("/file?name=$file", $base),
  ':content_cb'     => sub { $content .= $_[0]; return; },
);
#print $res->as_string;

print "not " unless $res->is_success
                and $res->content_type eq 'text/html'
		and $res->content_length == 147
		and defined $content
		and $res->title eq 'En prøve'
		and ! $res->content   # No content, because callback
		and $content =~ /å være/;
print "ok 6\n";		

}

unlink($file);



# Then try to list current directory
$res = $ua->get( url("/file?name=.", $base) );
#print $res->as_string;
print "not " unless $res->code == 501;   # NYI
print "ok 7\n";


#----------------------------------------------------------------
print "#------------Testing: Check redirect...\n";
sub httpd_get_redirect
{
   my($c) = @_;
   $c->send_redirect("/echo/redirect");
}

$res = $ua->get( url("/redirect/foo", $base) );
#print $res->as_string;

print "not " unless $res->is_success
                and $res->content =~ m|/echo/redirect|;
print "ok 8\n";
print "not " unless $res->previous->is_redirect
                and $res->previous->code == 301;
print "ok 9\n";

# Let's test a redirect loop too
sub httpd_get_redirect2 { shift->send_redirect("/redirect3/") }
sub httpd_get_redirect3 { shift->send_redirect("/redirect2/") }

$res = $ua->get(url("/redirect2", $base));
#print $res->as_string;
print "not " unless $res->is_redirect
                and $res->header("Client-Warning") =~ /loop detected/i;
print "ok 10\n";
$i = 0;
while ($res->previous) {
   $i++;
   $res = $res->previous;
}
print "not " unless $i == 7;
print "ok 11\n";

sub httpd_get_redirect_file { shift->send_redirect("file:/etc/passwd") }
$res = $ua->get(url("/redirect_file/", $base));
#print $res->as_string;
print "not " unless $res->is_redirect
                and $res->header("Client-Warning") =~ /can't redirect to a file:/i;
print "ok 12\n";


#----------------------------------------------------------------
print "#------------Testing: Check basic authorization...\n";
sub httpd_get_basic
{
    my($c, $r) = @_;
    #print STDERR $r->as_string;
    my($u,$p) = $r->authorization_basic;
    if (defined($u) && $u eq 'ok 13' && $p eq 'xyzzy') {
        $c->send_basic_header(200);
	print $c "Content-Type: text/plain";
	$c->send_crlf;
	$c->send_crlf;
	$c->print("$u\n");
    }
    else {
        $c->send_basic_header(401);
	$c->print("WWW-Authenticate: Basic realm=\"libwww-perl\"\015\012");
	$c->send_crlf;
    }
}

{
   package MyUA; @ISA=qw(LWP::UserAgent);
   sub get_basic_credentials {
      my($self, $realm, $uri, $proxy) = @_;
      if ($realm eq "libwww-perl" && $uri->rel($base) eq "basic") {
	  return ("ok 13", "xyzzy");
      }
      else {
          return undef;
      }
   }
}

{
my $that_url = url("/basic", $base);

$res = MyUA->new->get( $that_url );
#print $res->as_string;

my $host_port = $res->request->uri->host_port;

print "not " unless $res->is_success;
print $res->content;

# Let's try with a $ua that does not pass out credentials
$res = $ua->get( $that_url );
print "not " unless $res->code == 401;
print "ok 14\n";


print "# Host port: $host_port\n";

# Let's try to set credentials for this realm
$ua->credentials($host_port, "libwww-perl", "ok 13", "xyzzy");

$res = $ua->get( $that_url );

print "not " unless $res->is_success;
print "ok 15\n";

# Then illegal credentials
$ua->credentials($host_port, "libwww-perl", "user", "passwd");
$res = $ua->get( $that_url );
print "not " unless $res->code == 401;
print "ok 16\n";
}

#----------------------------------------------------------------
print "#------------Testing: Check proxy...\n";
sub httpd_get_proxy
{
   my($c,$r) = @_;
   if ($r->method eq "GET" and
       $r->url->scheme eq "ftp") {
       $c->send_basic_header(200);
       $c->send_crlf;
   }
   else {
       $c->send_error;
   }
}

$ua->proxy(ftp => $base);

$res = $ua->get( "ftp://ftp.perl.com/proxy" );
#print $res->as_string;
print "not " unless $res->is_success;
print "ok 17\n";

#----------------------------------------------------------------
print "#------------Testing: Check POSTing...\n";
sub httpd_post_echo
{
   my($c,$r) = @_;
   $c->send_basic_header;
   $c->print("Content-Type: text/plain");
   $c->send_crlf;
   $c->send_crlf;
   $c->print($r->as_string);
}

$res = $ua->post(
  url("/echo/foo", $base),
    ['foo' => 'bar', 'bar' => 'test'],
);
#print $res->as_string;

$_ = $res->content;
print "not " unless $res->is_success
                and /^Content-Length:\s*16$/mi
		and /^Content-Type:\s*application\/x-www-form-urlencoded$/mi
		and /^foo=bar&bar=test/m;
print "ok 18\n";		


{

my $content;

$res = $ua->post(
  url("/echo/foo", $base),
    ['foo' => 'bar', 'bar' => 'test'],
  ':content_cb'     => sub { $content .= $_[0]; return; },
);

$_ = $content;
print "not " unless $res->is_success
                and /^Content-Length:\s*16$/mi
		and /^Content-Type:\s*application\/x-www-form-urlencoded$/mi
		and /^foo=bar&bar=test/m
		and ! $res->content
;
print "ok 19\n";		

}

#----------------------------------------------------------------
print "#------------Testing: Terminating server...\n";
sub httpd_get_quit
{
    my($c) = @_;
    $c->send_error(503, "Bye, bye");
    exit;  # terminate HTTP server
}

$res = $ua->get( url("/quit", $base) );

print "not " unless $res->code == 503 and $res->content =~ /Bye, bye/;
print "ok 20\n";

